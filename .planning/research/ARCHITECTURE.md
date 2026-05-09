# Architecture Research

**Domain:** Wearable assistive AI system — CV + voice + MQTT sensor/haptic + cloud LLM with offline fallback
**Researched:** 2026-05-09
**Confidence:** HIGH (existing codebase verified against domain patterns; patterns corroborated by multiple 2025-2026 sources)

## Standard Architecture

### System Overview

```
┌───────────────────────────────────────────────────────────────────┐
│                        ESP32 Firmware (two nodes)                  │
│  ┌──────────────────────┐      ┌──────────────────────────────┐   │
│  │   sonar_esp32        │      │   haptic_esp32               │   │
│  │  HC-SR04 × 4+        │      │  4-motor ERM compass         │   │
│  │  → helios/sonar/     │      │  ← helios/haptic/<ch>        │   │
│  │    distance_mm       │      │                              │   │
│  └──────────┬───────────┘      └──────────────┬───────────────┘   │
│             │ MQTT pub                         │ MQTT sub          │
└─────────────┼─────────────────────────────────┼───────────────────┘
              │ WiFi (laptop hotspot)            │
┌─────────────▼─────────────────────────────────▼───────────────────┐
│                     mosquitto broker (localhost)                    │
└─────────────┬─────────────────────────────────┬───────────────────┘
              │ paho-mqtt sub                    │ paho-mqtt pub
┌─────────────▼──────────────────────────────────────────────────────┐
│                        Python Host (macOS M5)                       │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                   Perception Layer                           │   │
│  │  WebcamSource (bg thread)  MQTTSonarSource (MQTT net thread) │   │
│  │  YoloWorld (RLock)         HandTracker (MediaPipe)           │   │
│  │  FrameSource protocol      DistanceSource protocol           │   │
│  └────────────────────────┬────────────────────────────────────┘   │
│                           │ PerceptionTick (bboxes, hands, dist)    │
│  ┌────────────────────────▼────────────────────────────────────┐   │
│  │                   State Machine (MODE-01)                    │   │
│  │   IDLE ──── LOCOMOTION ──── MANIPULATION ──── IDLE           │   │
│  │   (guard: intent/overlap/timeout)                            │   │
│  └───────┬────────────────┬────────────────────────────────────┘   │
│          │ intent event   │ state query                            │
│  ┌───────▼────────┐  ┌────▼────────────────────────────────────┐   │
│  │  Voice Layer   │  │           Intent Handlers               │   │
│  │  MicSource     │  │  SCENE_DESC  → VisionRouter → TTS       │   │
│  │  Whisper STT   │  │  DISTANCE_QUERY → sonar + haptic        │   │
│  │  Intent clf    │  │  GRAB_GUIDANCE → compass + state FSM    │   │
│  └───────┬────────┘  └────────────────────────────────────────┘   │
│          │ PTT event (MQTT helios/button/ptt or keyboard)          │
│  ┌───────▼────────────────────────────────────────────────────┐    │
│  │                    Cloud Layer                              │    │
│  │  VisionRouter: GPT-4o ←→ LLaVA fallback (60s window)       │    │
│  │  Whisper API   GPT-4o-mini intent    TTS-1                  │    │
│  └────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              Output Layer                                    │   │
│  │  HapticPublisher (MQTT pub → haptic_esp32)                   │   │
│  │  TTS speaker (laptop speaker)                               │   │
│  └─────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Boundary |
|-----------|----------------|----------|
| WebcamSource | Pumps USB-C frames at full camera rate into a single "latest slot"; drops old frames | Owns cv2.VideoCapture; exposes `get_latest_frame() → np.ndarray` only |
| MQTTSonarSource | Subscribes `helios/sonar/distance_mm`; caches under threading.Lock | Owns MQTT client connection; exposes `get_distance_mm() → int\|None` |
| YoloWorld | Open-vocab object detection on downsampled frames | Behind threading.RLock for set_classes; exposes `detect(frame) → list[Bbox]` |
| HandTracker | MediaPipe 21-landmark hand detection | Stateless per-frame; exposes `detect_hands(frame) → list[HandLandmarks]` |
| StateMachine (new) | Holds current mode (IDLE/LOCOMOTION/MANIPULATION); enforces guard conditions and timeout transitions | Owns mode state; exposes `transition(event)`, `current_mode` |
| VoiceLoop / ptt_loop | Keyboard or MQTT PTT → record → Whisper → intent → dispatch | Owns MicSource lifecycle; calls dispatch table keyed on Intent |
| VisionRouter | Cloud/local routing with circuit-breaker (60s offline window) | Single entry point `describe_scene(frame, prompt) → str`; hides all retry logic |
| IntentHandlers | One function per Intent value; reads perception state, commands haptic, drives state machine | No direct I/O — call sonar/haptic/tts/vision through injected references |
| HapticPublisher | Publishes `{"d": duty, "ms": ms}` JSON to `helios/haptic/<channel>` | Owns MQTT publish client; exposes `pulse(channel, duty, ms)` |
| sonar_esp32 firmware | HC-SR04 read loop → MQTT int-mm publish | No Python; independent embedded process |
| haptic_esp32 firmware | MQTT subscribe → PWM drive | No Python; independent embedded process |

## Architectural Patterns

### Pattern 1: Single-Slot Frame Buffer (latest-wins, no backlog)

**What:** Background camera thread continuously overwrites a single `_latest` slot under a lock. The perception loop always reads the most-recent frame with no FIFO queue, so processing latency never accumulates a backlog.

**When to use:** Any pipeline where inference (YOLO, MediaPipe) runs slower than camera capture, and freshness of data matters more than completeness. Standard for 10-20 Hz CV on wearables.

**Confirmed in existing code:** `WebcamSource` already implements this. Do not replace with a Queue — a bounded Queue of depth 1 would be equivalent, but adds overhead and complexity with no benefit.

**Trade-offs:** Frames are dropped (expected and desirable). Observer cannot distinguish "nothing new" from "camera froze" without a timestamp — add `_latest_ts` to detect camera hang.

```python
# Correct pattern (already in WebcamSource)
with self._lock:
    self._latest = frame.copy()  # overwrite; old frame discarded

def get_latest_frame(self) -> np.ndarray | None:
    with self._lock:
        return self._latest.copy() if self._latest is not None else None
```

### Pattern 2: Thread-per-I/O-Boundary, Synchronous Core Loop

**What:** Each external I/O source (camera, MQTT net, keyboard/pynput) lives in its own daemon thread. The main processing loop (perception iter, voice pipeline) runs synchronously in the main or a dedicated thread. No asyncio event loop at the core.

**Why this beats full asyncio for this project:** YOLO inference, MediaPipe, and cv2 are all C-extension-backed with GIL release during their heavy paths, but they are not awaitable. Wrapping them in `run_in_executor` would require an asyncio event loop at the top and add conversion complexity for zero throughput gain at 10-20 Hz. The existing threading model is correct and validated at 17.6 Hz p95 78ms.

**The 2026 ecosystem position on this:** Asyncio is preferred for high-concurrency I/O-bound tasks (1000s of concurrent connections). For a 10-20 Hz pipeline with 4 hardware sources, threading with explicit locks is simpler, debuggable, and proven. The GIL is not a bottleneck when the heavy work is in C extensions.

**Where asyncio could add value (future consideration):** A thin asyncio wrapper around the MQTT client (via `aiomqtt` or `asyncio-paho`) would allow `await haptic.pulse(...)` syntax and better structured concurrency if intent handlers ever become long coroutines. This is LOW priority for the demo but worth noting for Phase 4+.

**Trade-offs:** macOS requires cv2 windows on the main thread (enforced in ptt_loop.py). Keep this invariant — it is not a bug, it is a macOS constraint.

### Pattern 3: Finite State Machine with Guard Conditions

**What:** A dedicated `ModeStateMachine` object holds `current_mode: Mode` (enum: IDLE / LOCOMOTION / MANIPULATION). Transitions fire only when guard conditions pass. Timeout transitions fire from a watchdog timer or checked at each loop iteration.

**Why explicit FSM beats ad-hoc flags:** The GRAB_GUIDANCE handler spans two modes (LOCOMOTION → compass-to-bbox, MANIPULATION → hand-to-object) with a 30s timeout and an overlap-exit condition. Without an FSM, this becomes a nest of `if` flags that are hard to reason about and easy to corrupt across concurrent callbacks.

**Recommended library:** `python-statemachine` (PyPI: `python-statemachine`, actively maintained 2025-2026). Supports guard conditions, entry/exit actions, and both sync and async codebases. No dependency conflict with existing stack.

**Alternative if no library preferred:** A plain dataclass with `Mode` enum and `transition(event, **ctx)` method. Sufficient for 3-state machine; use library only if adding compound states or history.

**Build position:** The state machine must exist before HDLR-02 (DISTANCE_QUERY) and HDLR-03 (GRAB_GUIDANCE) are implemented. HDLR-01 (SCENE_DESC) does not require mode switching and can be built first.

```python
from enum import Enum, auto

class Mode(Enum):
    IDLE = auto()
    LOCOMOTION = auto()
    MANIPULATION = auto()

class ModeStateMachine:
    def __init__(self):
        self._mode = Mode.IDLE
        self._deadline: float | None = None  # monotonic timeout

    @property
    def mode(self) -> Mode:
        return self._mode

    def transition(self, event: str, *, now: float) -> Mode:
        """Guard conditions enforced here. Returns new mode."""
        match (self._mode, event):
            case (Mode.IDLE, "GRAB_START"):
                self._mode = Mode.LOCOMOTION
                self._deadline = now + 30.0
            case (Mode.LOCOMOTION, "OVERLAP") | (Mode.MANIPULATION, "OVERLAP"):
                self._mode = Mode.IDLE
                self._deadline = None
            case (Mode.LOCOMOTION, "HAND_CENTERED"):
                self._mode = Mode.MANIPULATION
            case _:
                pass  # invalid transition ignored
        return self._mode

    def tick(self, now: float) -> Mode:
        """Call each perception iteration to fire timeout transitions."""
        if self._deadline and now >= self._deadline:
            self._mode = Mode.IDLE
            self._deadline = None
        return self._mode
```

### Pattern 4: Circuit Breaker for Cloud LLM Fallback

**What:** VisionRouter tracks `_offline_until` (monotonic deadline). On any APIError/Timeout/RateLimit, it enters a 60s offline window during which all calls route directly to LLaVA without attempting cloud. After the window expires, one cloud probe is attempted. One-time "switching to offline mode" TTS announcement per outage window.

**This is already implemented correctly.** Confirmed against 2025 production patterns for LLM fallback. The carry-over code matches the circuit-breaker pattern recommended in production LLM reliability guides.

**One gap to address:** The existing fallback triggers on `APIError`, `APITimeoutError`, `RateLimitError` — but not on Python `TimeoutError` (connection-level timeout set via `httpx` timeout, distinct from the API-level timeout). Verify the OpenAI SDK client is configured with an explicit `timeout=` argument so connection hangs don't block the voice loop.

### Pattern 5: MQTT as Transport Boundary (not internal bus)

**What:** MQTT is exclusively the wire protocol between Python host and ESP32 firmware. It is NOT used as an internal event bus between Python components. Python components communicate via direct method calls, shared state under locks, or threading.Event / threading.Queue.

**Why this matters:** Using MQTT for Python-to-Python communication (e.g., routing PTT events through MQTT topics) adds broker round-trip latency (~1-5ms local), makes the broker a single point of failure for internal routing, and complicates testing. The PTT button publishes over MQTT only because it is on an ESP32. On the Python side, a threading.Event is sufficient.

**Topic schema (confirmed from firmware layer):**
- `helios/sonar/distance_mm` — int payload, published by sonar ESP32
- `helios/sonar/status` — status string, published by sonar ESP32
- `helios/haptic/<channel>` — JSON `{"d": duty, "ms": ms}`, subscribed by haptic ESP32
- `helios/button/ptt` — payload `"press"` / `"release"`, published by PTT ESP32 (net-new)

## Data Flow

### Flow 1: Frame Acquisition (10-20 Hz continuous)

```
USB-C cam (hardware)
    → cv2.VideoCapture.read() [camera bg thread, blocks on V4L2]
    → _latest slot overwrite [threading.Lock]
    → get_latest_frame() [perception loop, 10 Hz poll]
    → downsample 320×240
    → YoloWorld.detect() [C ext, GIL released during inference]
    → HandTracker.detect_hands() [C ext, GIL released]
    → PerceptionTick{bboxes, hands, frame_ref}
    → StateMachine.tick(now)  [timeout check]
    → optional: cv2 display overlay [main thread on macOS]
```

### Flow 2: Voice PTT → Intent Dispatch

```
[PTT source: keyboard spacebar OR MQTT helios/button/ptt]
    → threading.Event set (press)
    → MicSource.start_recording()  [sounddevice bg thread]
    → threading.Event set (release)
    → MicSource.stop_recording() → wav_bytes
    → cloud.whisper.transcribe(wav_bytes) → transcript  [blocking, ~1-4s]
    → cloud.intent.classify(transcript) → IntentResult{intent, target}
    → DISPATCH[intent](state) → handler
        SCENE_DESC:
            → VisionRouter.describe_scene(frame, prompt)
                → GPT-4o vision [cloud, <3s] OR LLaVA [local, ~5-15s]
            → cloud.tts.speak(description)
        DISTANCE_QUERY:
            → YoloWorld.detect() for target bbox
            → HapticPublisher.pulse(wrist_quadrant) [center head]
            → MQTTSonarSource.get_distance_mm() [cached latest]
            → cloud.tts.speak(f"{distance_mm // 10} centimeters")
        GRAB_GUIDANCE:
            → StateMachine.transition("GRAB_START")
            → LOCOMOTION: HapticPublisher wrist compass to bbox centroid
            → on overlap OR manual "HAND_CENTERED": transition to MANIPULATION
            → MANIPULATION: mute belt, HapticPublisher wrist compass to hand→object
            → on bbox overlap OR 30s timeout: StateMachine.transition("OVERLAP")
        UNKNOWN:
            → cloud.tts.speak("Sorry, please repeat.")
```

### Flow 3: Haptic Publish (sub-150ms latency target)

```
IntentHandler calls HapticPublisher.pulse(channel, duty, ms)
    → paho-mqtt publish() to helios/haptic/<channel>
    → mosquitto broker (localhost, loopback)
    → haptic_esp32 on_message callback
    → PWM drive to motor [ERM motor response ~10-50ms]
```

Latency budget: Python→broker ~1ms loopback, broker→ESP32 WiFi ~5-20ms, motor response ~10-50ms. Total ~150ms worst-case — within HAPT-01 spec.

### Flow 4: Sonar Cache Update (async, decoupled)

```
sonar_esp32 HC-SR04 read loop (~10 Hz)
    → MQTT publish helios/sonar/distance_mm [int mm]
    → mosquitto broker
    → MQTTSonarSource.on_message() [MQTT net thread]
    → _latest_mm update [threading.Lock]

Separately, when DISTANCE_QUERY fires:
    → MQTTSonarSource.get_distance_mm() [reads cached value, non-blocking]
```

### State Transitions (HDLR-03 sequence)

```
IDLE
  │ intent=GRAB_GUIDANCE
  ▼
LOCOMOTION
  │ Wrist compass: pulse quadrant of bbox centroid each frame
  │ Belt sonar: still active (proximity alerts)
  │ guard: hand bbox overlaps target bbox → OVERLAP event
  │ guard: 30s elapsed → timeout event
  ▼
MANIPULATION (on HAND_CENTERED guard)
  │ Belt sonar: muted (operator close to object)
  │ Wrist compass: pulse quadrant of hand→target vector
  │ guard: landmark overlap → OVERLAP event
  │ guard: 30s total elapsed → timeout event
  ▼
IDLE (on OVERLAP or timeout)
```

## Component Boundaries (Confirm vs Challenge)

### Confirmed Seams

| Seam | Verdict | Evidence |
|------|---------|----------|
| FrameSource Protocol | Confirmed | Standard pattern; WebcamSource well-implemented; enables mock for tests |
| DistanceSource Protocol | Confirmed | Allows MockDistanceSource in tests; MQTTSonarSource in production |
| VisionRouter as strategy switch | Confirmed | Circuit-breaker pattern matches 2025 production LLM reliability patterns |
| MQTT as external transport only | Confirmed | Consistent with domain pattern; no internal Python components should use MQTT |
| HapticPublisher as MQTT client wrapper | Confirmed | Correct boundary; publish-only, no state |

### Challenged / Gaps

| Gap | Issue | Recommendation |
|-----|-------|----------------|
| StateMachine does not exist yet | GRAB_GUIDANCE (HDLR-03) and mode-dependent haptic routing cannot be implemented without it | Build ModeStateMachine before HDLR-02 and HDLR-03 |
| PTT source is currently keyboard only | MQTT PTT button (PTT-01) not wired into ptt_loop.py | Add `MQTTButtonSource` that converts MQTT `helios/button/ptt` payload into the same threading.Event that keyboard press uses; share dispatch path |
| ptt_loop.py owns too many concerns | It currently owns: keyboard listener, mic recording, STT, intent, dispatch, cv2 window, VisionRouter reference | Consider extracting `VoicePipeline(mic, whisper, intent, dispatch)` from the loop control; allows unit testing the pipeline without the keyboard listener or cv2 |
| No PerceptionTick shared across loops | perception_loop.py and ptt_loop.py are separate entry points, both acquire frames independently | For HDLR-02/03 the intent handler needs fresh bboxes; the voice loop must hold a reference to a shared `PerceptionState` or call the perception layer directly; add a `SharedPerceptionState` dataclass updated by the perception loop and read by intent handlers |
| cv2 window must own main thread (macOS) | Documented correctly; no fix needed | Keep cv2 on main thread; move perception and voice loops to threads if needed; never call cv2.waitKey from non-main |

## Concurrency Model

### Current Model (threads)

```
Main thread        → cv2.waitKey(), pynput listener (macOS requirement)
Camera bg thread   → WebcamSource frame pump
MQTT net thread    → paho loop_start() for sonar + haptic clients
Voice pipeline     → Runs in main or dedicated thread on PTT event
YOLO + MediaPipe   → Called from perception loop thread (GIL released in C ext)
```

This model is correct and validated. The 17.6 Hz perception throughput confirms the threading model has no bottleneck at this scale.

### Why Not Asyncio at the Core

- YOLO and MediaPipe are not awaitable; wrapping in `run_in_executor` adds a ThreadPoolExecutor any way — no gain over direct threading
- cv2 windows require the main thread on macOS; asyncio runs its event loop on the main thread, which conflicts unless cv2 is pushed to a worker thread (complicates the design)
- Whisper transcription blocks for 1-4s and is not CPU-bound in Python (it calls OpenAI HTTP); this is I/O-bound and already acceptable in a sequential PTT pipeline — no parallelism needed
- paho-mqtt's `loop_start()` spawns its own thread internally; it integrates naturally with the threading model

### Where Asyncio Could Add Value (not recommended for demo sprint)

- If haptic pulses need precise timing relative to frame events, an asyncio Task per motor channel with proper cancellation semantics would be cleaner than threading.Timer
- If multiple concurrent cloud calls are needed (e.g., simultaneous scene description + intent classification), `asyncio.gather()` would parallelize them elegantly
- Post-demo: migrating the outer shell to asyncio with `aiomqtt` and `asyncio.to_thread()` for blocking calls is a viable refactor path; it does not require changes to the Protocol abstractions

**Recommendation for this milestone:** Keep the threading model. Add asyncio only if a specific concurrency problem manifests (e.g., haptic timing jitter, MQTT reconnect racing). Do not introduce asyncio speculatively.

## Build Order (Component Dependencies)

The dependency graph determines which components must work before others can be built:

```
Tier 0 (already working — verify on new hardware only)
  WebcamSource + YoloWorld + HandTracker + perception_loop
  VisionRouter (GPT-4o + LLaVA fallback)
  ptt_loop keyboard PTT → Whisper → intent → HDLR-01

Tier 1 (MQTT infrastructure — needed by Tier 2 and Tier 3)
  mosquitto broker install + topic schema smoke test
  sonar_esp32 firmware flash → helios/sonar/distance_mm publishes
  haptic_esp32 firmware flash → helios/haptic/<ch> drives motors
  MQTTSonarSource wired into Python (already coded, needs broker)
  HapticPublisher wired into Python (already coded, needs broker)

Tier 2 (PTT hardware + SharedPerceptionState)
  MQTT PTT button → MQTTButtonSource → threading.Event → same dispatch path
  SharedPerceptionState (dataclass updated by perception loop, read by handlers)

Tier 3 (StateMachine — required before Tier 4)
  ModeStateMachine: IDLE/LOCOMOTION/MANIPULATION + guard conditions + timeout

Tier 4 (Intent handlers — depend on Tier 1 + Tier 2 + Tier 3)
  HDLR-02 (DISTANCE_QUERY): needs MQTTSonarSource + HapticPublisher + SharedPerceptionState
  HDLR-03 (GRAB_GUIDANCE): needs all of Tier 3 + HDLR-02 verified

Tier 5 (Demo hardening)
  DEMO-01: 3 back-to-back runs on real hardware
  DEMO-02: panic fallback video + Wizard-of-Oz keyboard override
```

Do not start Tier 4 without Tier 3 complete. The state machine is the critical dependency for HDLR-03 and is the highest architectural risk because it was "Phase 4 incomplete" in the robotics fork.

## Anti-Patterns

### Anti-Pattern 1: MQTT as Python Internal Event Bus

**What people do:** Publish Python-to-Python events (e.g., intent results, state changes) to MQTT topics and subscribe within the same Python process.

**Why it's wrong:** Adds broker round-trip latency to internal calls, makes the broker a SPOF for internal routing, and prevents running unit tests without a live broker. The 150ms haptic latency budget leaves no room for unnecessary hops.

**Do this instead:** Use threading.Event, threading.Queue, or direct method calls for Python-to-Python communication. MQTT is only for the Python↔ESP32 boundary.

### Anti-Pattern 2: Sharing a Single YOLO Model Instance Across Threads

**What people do:** Instantiate YoloWorld once at module level, call `detect()` from multiple threads concurrently.

**Why it's wrong:** Even though Ultralytics' Predictor has an internal lock, the set_classes() + forward sequence requires an RLock to prevent head-shape races. The existing code uses `threading.RLock()` around this sequence (documented in codebase ARCHITECTURE.md as D-12) — this is correct. Do not bypass it.

**Do this instead:** If a second thread needs YOLO inference (e.g., a dedicated "describe scene" thread), create a second YoloWorld instance in that thread. Do not share the existing instance without its RLock.

### Anti-Pattern 3: Blocking the cv2 Main Thread with Long Operations

**What people do:** Call cloud APIs (VisionRouter, Whisper) directly in the main thread's event loop where cv2.waitKey() is also called.

**Why it's wrong:** cv2.waitKey() must be called at ~30fps to keep the window responsive. A 3-5s cloud call in the same thread freezes the display and may trigger macOS "application not responding" state.

**Do this instead:** Spawn a daemon thread for any blocking cloud call. The existing code already does this for the 'd'-key describe call in perception_loop. Apply the same pattern to intent handler calls — run them in a background thread and update a shared result slot that the main thread reads on next tick.

### Anti-Pattern 4: Queue Backlog in the Perception Loop

**What people do:** Use a FIFO queue between camera thread and perception loop, processing every frame.

**Why it's wrong:** At 30fps camera rate with a 10-20fps processing rate, the queue grows unboundedly. By the time the queue drains, the system is responding to frames from the past.

**Do this instead:** The single-slot overwrite pattern already in WebcamSource. Always process the freshest available frame.

### Anti-Pattern 5: Hard-coding YOLO Classes at Import Time

**What people do:** Call `set_classes()` once at startup with a fixed vocabulary, never update it.

**Why this is a moderate risk:** In HDLR-02 (DISTANCE_QUERY) the user may ask about a target that is not in the current YOLO vocabulary. If set_classes() is called at runtime with a new target, the existing RLock must be held to prevent the head-shape race (D-12). Forgetting the lock causes intermittent GPU crashes.

**Do this instead:** Always use the `with yolo._lock:` context manager (or the provided `set_classes_safe()` wrapper if one exists) when updating classes at runtime.

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| OpenAI GPT-4o vision | Blocking HTTP via openai SDK, daemon thread for long calls | Singleton client (D-05); 10s timeout → fallback window |
| OpenAI Whisper | Blocking HTTP, called in PTT release handler | 1-4s acceptable in PTT sequential pipeline |
| OpenAI TTS-1 | Blocking HTTP + audio playback | Chain after Whisper in same PTT thread; no parallelism needed |
| OpenAI GPT-4o-mini intent | Blocking HTTP, strict JSON mode | Must return in <2s; low token count; fast in practice |
| Ollama LLaVA (fallback) | Local HTTP to localhost:11434 | Cold-start 13-46s; pre-warm with warmup_llava.py before demo |
| mosquitto broker | paho-mqtt loop_start() network thread | Local only; hotspot isolated from venue WiFi |
| sonar_esp32 | MQTT sub on helios/sonar/distance_mm | int payload in mm; firmware filters <20mm and >4000mm |
| haptic_esp32 | MQTT pub to helios/haptic/<channel> | JSON {"d": duty, "ms": ms}; 7 channels |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Camera thread ↔ Perception loop | threading.Lock on `_latest` slot | Single-slot overwrite; no queue |
| MQTT net thread ↔ Distance query | threading.Lock on `_latest_mm` | MQTTSonarSource already correct |
| Perception loop ↔ Intent handlers | SharedPerceptionState dataclass under lock (to build) | Handlers need fresh bboxes without re-running YOLO |
| PTT source ↔ Voice pipeline | threading.Event press/release | Same Event whether from keyboard or MQTT button |
| Intent handlers ↔ StateMachine | Direct method call `fsm.transition(event)` | FSM must be thread-safe if handlers run in threads |
| Voice pipeline ↔ HapticPublisher | Direct method call `haptic.pulse(ch, d, ms)` | HapticPublisher is stateless publish-only; no lock needed |
| Intent handlers ↔ VisionRouter | Direct method call `router.describe_scene(frame, prompt)` | Must run in daemon thread to not block cv2 main thread |

## Sources

- Ultralytics YOLO thread-safe inference docs (confirmed 2026): https://docs.ultralytics.com/guides/yolo-thread-safe-inference/
- asyncio vs threading for Python 2026: https://docs.bswen.com/blog/2026-04-20-asyncio-vs-threading-vs-multiprocessing/
- paho-mqtt asyncio bridge patterns: https://github.com/eclipse-paho/paho.mqtt.python/blob/master/examples/loop_asyncio.py
- aiomqtt (asyncio-native MQTT wrapper): https://github.com/mossblaser/aiomqtt
- python-statemachine (FSM library, sync+async): https://pypi.org/project/python-statemachine/
- transitions FSM library with asyncio: https://github.com/pytransitions/transitions
- LLM retries, fallbacks, circuit breakers production guide: https://www.getmaxim.ai/articles/retries-fallbacks-and-circuit-breakers-in-llm-apps-a-production-guide/
- LLM-Glasses wearable vision+haptic architecture (2026): https://arxiv.org/html/2503.16475
- Event-driven asyncio pubsub systems: https://johal.in/event-driven-programming-custom-pub-sub-systems-with-pythons-asyncio-queues/
- OpenCV single-slot frame skipping pattern: https://answers.opencv.org/question/81702/skipping-all-but-the-latest-frame-in-videocapture/
- Chained voice agent pipeline architectures: https://brain.co/blog/chained-voice-agent-architectures-speech-to-speech-vs-chained-pipeline-vs-hybrid-approaches
- Multimodal navigation wearable for BVI (MDPI 2025): https://www.mdpi.com/1424-8220/25/13/4223
- asyncio run_in_executor for blocking inference: https://superfastpython.com/asyncio-blocking-tasks/

---
*Architecture research for: wearable assistive AI (CV + voice + MQTT + cloud LLM)*
*Researched: 2026-05-09*
