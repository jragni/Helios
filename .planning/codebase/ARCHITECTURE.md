# Architecture

**Analysis Date:** 2026-05-09

## Pattern Overview

**Overall:** Layered event-driven system with clear separation of concerns: perception (camera & sonar inputs) → reasoning (vision & voice intents) → output (haptic feedback & audio). Entry points are `perception_loop.py` (visual awareness) and `voice/ptt_loop.py` (voice interaction), both decoupled through Protocol abstractions.

**Key Characteristics:**
- Protocol-driven abstraction layer (`FrameSource`, `DistanceSource`) enabling pluggable hardware and mock implementations
- Strategy pattern for vision backends (`VisionRouter`) switching between local Ollama (offline) and cloud GPT-4o (with fallback)
- MQTT for distributed I/O: sonar (distance) and haptic (vibration motors) communicate via pub/sub with embedded ESP32 firmware
- Threading for I/O isolation: background webcam/MJPEG pump, MQTT network loop, and optional PTT voice pipeline run concurrently
- Synchronous core loops (perception and voice dispatch) with asynchronous I/O at boundaries (camera threads, MQTT clients, cloud API calls)

## Layers

**Perception Layer:**
- Purpose: Acquire and process sensor inputs — frames from camera/MJPEG and distance from sonar
- Location: `src/helios/perception/`
- Contains: `FrameSource` protocol, `WebcamSource` (local camera via cv2), `MJPEGSource` (ESP32-CAM stream), `DistanceSource` protocol, `MQTTSonarSource` (MQTT distance), `HandTracker` (MediaPipe hand landmarks), `YoloWorld` (open-vocabulary object detection)
- Depends on: cv2 (OpenCV), numpy, mediapipe, ultralytics (YOLO), paho-mqtt
- Used by: `perception_loop.py` and `voice/ptt_loop.py` for frame capture and distance queries

**Vision Layer:**
- Purpose: Scene understanding through image captioning and object detection
- Location: `src/helios/vision/`
- Contains: `LLavaClient` (local Ollama llava:7b), `VisionRouter` (strategy switch local vs. cloud + fallback), `DEFAULT_SCENE_PROMPT`
- Depends on: ollama Python package, openai package (GPT-4o), numpy, cv2
- Used by: `perception_loop.py` (on-demand describe via 'd' key) and `voice/ptt_loop.py` (SCENE_DESC intent handler)

**Voice Layer:**
- Purpose: Speech input, transcription, intent classification, and audio output
- Location: `src/helios/voice/` and `src/helios/cloud/`
- Contains: `MicSource` (sounddevice-backed recorder, 16 kHz mono WAV), `ptt_loop.py` (push-to-talk main loop), intent classifier, Whisper transcriber, TTS speaker
- Depends on: pynput (keyboard listener), sounddevice (audio I/O), openai (Whisper, intent, TTS), pyttsx3 (TTS fallback)
- Used by: Voice interaction workflows; dispatches intents (SCENE_DESC, DISTANCE_QUERY, GRAB_GUIDANCE, UNKNOWN)

**Output Layer:**
- Purpose: Haptic feedback to wearable vibration motors
- Location: `src/helios/output/`
- Contains: `HapticPublisher` (MQTT client publishing pulse commands to ESP32 haptic firmware), channel definitions (wrist/belt zones)
- Depends on: paho-mqtt
- Used by: Intent handlers to provide tactile feedback (planned Phase 4)

**Cloud Layer:**
- Purpose: External API clients for GPT-4o, Whisper, TTS, and intent classification
- Location: `src/helios/cloud/`
- Contains: `openai_client.py` (singleton OpenAI client per D-05), `intent.py` (GPT-4o-mini strict JSON classifier), `whisper.py` (transcriber), `tts.py` (TTS via API)
- Depends on: openai package
- Used by: `vision/router.py`, `voice/ptt_loop.py`, perception loop describe handlers

**Firmware Layer:**
- Purpose: Embedded sensors and actuators on ESP32 microcontrollers
- Location: `firmware/sonar_esp32/` (HC-SR04 sonar) and `firmware/haptic_esp32/` (vibration motors)
- Contains: Arduino sketches publishing distance to `helios/sonar/distance_mm` and subscribing to `helios/haptic/<channel>`
- Protocol: MQTT topics with int-mm payloads and JSON PWM commands
- Used by: Python host via MQTT; runs independently

## Data Flow

**Perception Loop (perception_loop.py):**

1. WebcamSource/MJPEGSource.start() spawns background thread pumping frames → `_latest` slot
2. perception_loop.run() calls perception_iter() which:
   - Polls source.get_latest_frame() (returns freshest copy, no backlog)
   - Downsamples to 320x240 (D-08)
   - Runs YoloWorld.detect() with prompts → list[Bbox]
   - Runs HandTracker.detect_hands() → list[HandLandmarks]
   - Yields PerceptionTick (bboxes, hands, placeholder distance, latency)
3. Optional cv2 display overlay with YOLO/hand annotations
4. On 'd' key: spawns daemon thread → VisionRouter.describe_scene() → TTS playback (future)
5. Loop runs for duration_s, collects latency stats (median, p95, Hz)

**Voice PTT Loop (voice/ppt_loop.py):**

1. pynput.keyboard.Listener starts background threads monitoring keyboard
2. On spacebar press: MicSource.start_recording() (16 kHz mono int16)
3. On spacebar release → _process_release() pipeline:
   - wav_bytes = MicSource.stop_recording()
   - transcript = Whisper.transcribe(wav_bytes)
   - result = intent.classify(transcript) → IntentResult {intent, target}
   - dispatch_intent(state, intent) → handler lookup
4. Handler execution (dispatch table D-22):
   - SCENE_DESC → VisionRouter.describe_scene(frame, custom_prompt) → speak()
   - DISTANCE_QUERY → speak("Distance handler not wired yet")
   - GRAB_GUIDANCE → speak("Grab guidance not wired yet")
   - UNKNOWN → speak("Sorry, please repeat.")
5. Optional cv2 preview window with 5 Hz detection updates (cv2 window must own main thread on macOS)

**VisionRouter Strategy (vision/router.py):**

1. If use_openai=False:
   - Route directly to LLavaClient.describe_scene() (local Ollama)
2. If use_openai=True (D-26..D-28):
   - Check if now < _offline_until (D-28 60s offline window)
   - If in window: fall back to LLavaClient silently
   - If exiting window: reset flags, retry cloud
   - Try GPT-4o vision call (D-07, reuse JPEG Q=80 encoding)
   - On success: return response
   - On APIError/APITimeoutError/RateLimitError:
     - Set _offline_until = now + 60s
     - If first fallback (D-27): play_offline_cue() (silent if already announced)
     - Return LLavaClient.describe_scene() (offline fallback)

**State Management:**

- Perception: stateless iterator; fresh frames pulled on-demand
- MQTT sources: Behind threading.Lock() for concurrent reads (e.g., distance read from main loop while MQTT network thread writes)
- YOLO: threading.RLock() around set_classes()+forward to prevent head-shape races (D-12)
- Voice PTT: PTTState dataclass carries mic, webcam, router, last_intent, transcript across pipeline stages

## Key Abstractions

**FrameSource (Protocol):**
- Purpose: Abstract camera/MJPEG acquisition
- Examples: `WebcamSource` (cv2.VideoCapture), `MJPEGSource` (HTTP multipart stream)
- Pattern: start()/get_latest_frame()/stop() lifecycle; background thread returns freshest frame copy, drops old ones (no queue backlog)

**DistanceSource (Protocol):**
- Purpose: Abstract sonar/ToF distance sensor
- Examples: `MockDistanceSource` (constant), `MQTTSonarSource` (MQTT subscriber)
- Pattern: get_distance_mm() → int|None; thread-safe behind lock if asynchronous

**VisionRouter (Strategy):**
- Purpose: Switch between local (free, always-on) and cloud (better quality, online-only) vision backends with transparent fallback
- Pattern: Single entry point describe_scene(frame, prompt) → str; handles retry logic internally
- Decision tracking: _offline_until (monotonic deadline), _fallback_announced (one-time announcement per outage window)

**HandLandmarks & Bbox (NamedTuple):**
- Purpose: Standardized perception output contracts
- Pattern: HandLandmarks bundles 21-point hand skeleton + pixel bbox; Bbox bundles label, xyxy coords, confidence score

## Entry Points

**perception_loop.py:**
- Location: `src/helios/perception_loop.py`
- Triggers: `uv run python -m helios.perception_loop --duration 60 --show` (dev mode with display)
- Responsibilities: 
  - Acquire frames from camera/MJPEG (FrameSource abstraction)
  - Run YOLO object detection and MediaPipe hand tracking at full rate
  - Optionally display annotated live feed (cv2 window)
  - On 'd' key: spawn thread to describe scene (VisionRouter) and speak result
  - Collect and report latency stats (median, p95, FPS)

**voice/ppt_loop.py:**
- Location: `src/helios/voice/ppt_loop.py`
- Triggers: `uv run python -m helios.voice.ppt_loop [--show] [--mjpeg URL]`
- Responsibilities:
  - Listen for spacebar press/release via pynput keyboard.Listener
  - Record audio on press (MicSource), pipeline on release (transcribe → classify → dispatch)
  - Execute intent handler (SCENE_DESC calls describe_scene, others call speak stubs or TTS)
  - Optional cv2 preview (5 Hz detection, caption overlay for transcript)
  - macOS: requires Accessibility permission for keyboard events

## Error Handling

**Strategy:** Fail-loud for secrets (D-03), graceful degradation for cloud services (D-26..D-28), warnings for recoverable I/O errors.

**Patterns:**

- **Secret validation:** `openai_client.get_client()` raises RuntimeError immediately if OPENAI_API_KEY not set (D-03, canonical KEY_MISSING_MSG); called early in main() to fail before user-facing setup
- **Cloud fallback:** VisionRouter catches APIError/APITimeoutError/RateLimitError, enters offline window (60s), retries cloud after deadline; all logic centralized (D-26)
- **MQTT:** Paho's on_connect callback re-subscribes automatically on broker reconnects (idiom); on_message updates cache under lock; network errors logged at WARNING, next retry on loop iteration
- **Camera/MJPEG:** WebcamSource.start() raises RuntimeError if cv2.VideoCapture fails (permission denied or device missing); MJPEGSource.start() logs failures and exponential backoff (0.5–5s)
- **Audio:** MicSource catches sounddevice errors; pynput keyboard listener wrapped in try/except to prevent crashes
- **Intent classify:** On parse error or API exception, returns IntentResult("UNKNOWN", None) and logs WARNING; voice loop's UNKNOWN handler plays "Sorry, please repeat." (VOIC-05)

## Cross-Cutting Concerns

**Logging:** 
- Framework: Python logging module with root logger configured to INFO in main()
- Named loggers per module (e.g., `log = logging.getLogger(__name__)`)
- Key events: source startup/shutdown, MQTT connect/disconnect, cloud fallback, perception iterations (every N frames)

**Validation:**
- Secret presence (OPENAI_API_KEY checked in get_client())
- Frame shape (LLavaClient._encode validates HxWx3 BGR)
- Distance range (HC-SR04 firmware filters <20 mm or >4000 mm as out_of_range)
- Intent values constrained to Literal["SCENE_DESC", "DISTANCE_QUERY", "GRAB_GUIDANCE", "UNKNOWN"]

**Authentication:**
- OpenAI: API key from OPENAI_API_KEY env var (no other auth methods)
- MQTT: Broker address from MQTT_BROKER env var or default "helios.local" (no credentials)
- Keyboard: macOS Accessibility permission required for pynput listener (checked via 5s timeout probe in ppt_loop main())

---

*Architecture analysis: 2026-05-09*
