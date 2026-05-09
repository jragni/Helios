<!-- GSD:project-start source:PROJECT.md -->
## Project

**Helios**

Helios is a wearable AI-driven accessibility system for people with visual
impairment — a USB-C webcam, vibration-motor wrist compass, and multi-zone
HC-SR04 sonar driven by a backpack laptop running OpenAI vision/voice +
local Ollama LLaVA fallback. v1 is a demo prototype: a sighted operator
wears the rig and runs three scenes — scene description, distance query,
and grab guidance — for a hackathon/expo audience.

**Core Value:** A reliable 2-minute live demo of three scenes — describe / distance /
grab — running on real hardware, end to end, every time. Reliability of
the demo beats new features. If only one thing works, it must be the
3-scene loop.

### Constraints

- **Timeline**: This week. Hackathon-style sprint. Demo readiness over feature breadth.
- **Form factor**: Backpack laptop rig — locked. macOS M5, 16 GB unified, Metal 4.
- **Compute**: Laptop only. No Pi 5, no phone, no cloud GPU.
- **Power**: USB power banks for ESP32 nodes (no LiPo charging risk).
- **Audio**: Laptop speaker + USB lavalier mic — no BT pairing risk, judges hear user output.
- **Network**: Laptop hotspot for ESP32 nodes (deterministic vs venue wifi).
- **Tech stack**: Python 3.11 + uv (locked), OpenCV, ultralytics YOLO-World, MediaPipe, paho-mqtt, sounddevice, OpenAI APIs, Ollama+LLaVA fallback, mosquitto, PlatformIO.
- **Budget**: ~$200 hardware (mostly already on-hand from blind-assist run), ~$20 OpenAI prepaid.
- **Safety**: No blind-user testing — sighted-operator-driven demo, documented in Out of Scope.
- **Privacy**: User audio + camera frames pass through OpenAI API in cloud path. Local Ollama path available as offline fallback.
- **Inherited debt**: 14 lessons learned + 14 bugs catalogued in `docs/LESSONS_LEARNED.md` — read before designing.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

## Languages
- Python 3.11 - Core accessibility system, all perception, vision, voice, and output modules
- Embedded C/C++ (ESP32 firmware) - Sonar sensor reading and vibration motor control over MQTT (located at `firmware/sonar_esp32/`)
## Runtime
- CPython 3.11.14 - Reference version in `.python-version`
- uv 0.9.27+ - Fast Python resolver and installer
- Lockfile: `uv.lock` (2600+ dependencies resolved)
## Frameworks
- ultralytics>=8.3 - YOLO-World object detection (yolov8m-worldv2 model)
- mediapipe>=0.10.14 - Hand landmark detection (Tasks API, not legacy mp.solutions)
- opencv-python>=4.10 - Image processing, frame capture, JPEG encoding
- openai>=2.36.0 - GPT-4o vision, GPT-4o-mini intent, Whisper STT, TTS-1 text-to-speech
- ollama>=0.4 - Local LLaVA-1.6 7B offline fallback for vision (sync Python client)
- sounddevice>=0.5.5 - Microphone recording and speaker playback
- soundfile>=0.13.1 - WAV file I/O (read/write with sounddevice)
- paho-mqtt>=2.1 - MQTT pub/sub for sonar distance (MQTTSonarSource subscriber) and haptic motors (HapticPublisher)
- pynput>=1.8.1 - Push-to-talk key binding (monitor keyboard globally)
- python-dotenv>=1.2.2 - Load OPENAI_API_KEY from .env file
- numpy>=1.26,<2.2 - Numerical arrays for frame processing
- pillow>=10.4 - Image utilities (fallback image operations)
- openai-clip>=1.0.1 - CLIP model dependency (transitive from openai SDK)
- pytest>=9.0.3 - Test runner (dev group)
## Key Dependencies
- openai>=2.36.0 - Why: Gateway to all cloud vision, voice, and intent inference. GPT-4o vision calls include base64-encoded JPEG frames; Whisper handles 16 kHz mono PCM WAV; TTS-1 synthesizes speech; GPT-4o-mini classifies intents. Required for Phase 3+.
- ultralytics>=8.3 - Why: Provides pre-trained YOLOWorld model (yolov8m-worldv2.pt, 55 MB) for open-vocabulary object detection. No inference depends on it but it loads and runs the bundled weights.
- ollama>=0.4 - Why: Pure-Python sync client for local Ollama daemon (localhost:11434). LLavaClient wraps ollama.chat() for offline vision fallback. Required to be running before importing LLavaClient.
- mediapipe>=0.10.14 - Why: Tasks API (v0.10+) for real-time hand landmark detection. Auto-downloads hand_landmarker.task (7.5 MB) on first use.
- paho-mqtt>=2.1 - Why: Both sonar distance reading (MQTTSonarSource subscriber) and haptic motor control (HapticPublisher publisher) depend on paho for network I/O to broker (default: helios.local:1883).
- sounddevice + soundfile - Why: Together enable PTT loop's blocking audio playback (TTS output) and recording (Whisper input). sounddevice wraps PortAudio; soundfile wraps libsndfile.
## Configuration
- `.env` file required at repo root (see `.env.example`)
- `pyproject.toml` - Project metadata and dependency declaration
- `uv.lock` - Pinned versions (all 2600+ transitive dependencies)
- `yolov8m-worldv2.pt` (55 MB) - Pre-trained weights in repo root, loaded by YoloWorld at initialization
- `hand_landmarker.task` (7.5 MB) - MediaPipe model in repo root, auto-downloaded from `https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task` if missing
## Platform Requirements
- macOS 25.3+ (tested on Intel/Apple Silicon)
- Ollama 0.23+ (0.15.4 has Metal GPU bug on macOS 25.3 — see README)
- Python 3.11.14 via pyenv/brew/uv
- Linux (ESP32 MQTT firmware setup) or macOS (with Ollama for fallback)
- MQTT broker (mosquitto or equivalent) on local network
- ESP32 with HC-SR04 sonar and ERM vibration motors (firmware at `firmware/sonar_esp32/`)
- Optional: OpenAI API key for cloud vision path
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

## Module Structure
### Example from `src/helios/perception/mqtt_sonar_source.py`:
## Naming Patterns
- Lowercase with underscores: `mqtt_sonar_source.py`, `llava_client.py`, `perception_loop.py`
- Underscore prefix for internal modules: `_fallback_audio.py`
- PascalCase: `WebcamSource`, `HapticPublisher`, `YoloWorld`, `VisionRouter`, `MQTTSonarSource`
- Mock classes use `Mock` prefix: `MockDistanceSource`, `MockWebcamSource`
- Lowercase with underscores: `downsample()`, `perception_iter()`, `_ensure_model()`, `get_latest_frame()`
- Private/internal functions prefixed with single underscore: `_on_connect()`, `_on_message()`, `_callback()`, `_loop()`
- Module-level helpers use descriptive names: `describe_scene()`, `transcribe()`, `speak()`
- Lowercase with underscores: `frame_copy`, `describe_busy`, `caption_until`
- Private instance attributes prefixed: `self._lock`, `self._latest`, `self._client`, `self._thread`
- Public attributes (rare) use descriptive names: `self.device`, `self.model`, `self.use_openai`
- UPPER_SNAKE_CASE at module level: `DEFAULT_PROMPTS`, `DEFAULT_BROKER`, `DEFAULT_DUTY`, `PLACEHOLDER_DISTANCE_CM`, `TARGET_SIZE`, `WRIST_TOP`
- All defaults grouped near top: `DEFAULT_SCENE_PROMPT`, `DEFAULT_PULSE_MS`, `DEFAULT_SAMPLERATE`
- PascalCase when assigned: `FrameInput = Union[np.ndarray, bytes]`
## Type Hints
- Union syntax: `dict | None`, `str | int`, `list[str]`
- Built-in generics: `list[Bbox]`, `dict[int, str]`, `tuple[int, int, int, int]`
- Optional shorthand: `frame: np.ndarray | None`
- `NamedTuple` for lightweight data: `Bbox`, `HandLandmarks` in `src/helios/perception/protocol.py`
- `Protocol` with `@runtime_checkable` for contracts: `FrameSource`, `DistanceSource`
- Full type hints on parameters and return types
- Descriptive parameter names that self-document intent: `frame: np.ndarray`, `prompts: list[str]`, `duration_s: float`
- Use `-> None` explicitly; never omit return type
## Logging
- Every module declares `log = logging.getLogger(__name__)` after imports
- Never use bare `print()` except in `__main__` smoke tests
- Class-prefix style when logging from instance methods: `log.info("HapticPublisher: connected to %s:%d", ...)` (see `src/helios/output/haptic.py:133`)
- Class name prevents ambiguity when same module has multiple classes
- Use `%s` formatting (not f-strings) for lazy evaluation
- `log.info()` — operational events: connections, subscriptions, prompt changes, iterations
- `log.warning()` — recoverable issues: bad channels, connection failures, fallbacks
- `log.debug()` — detailed diagnostic: frame properties, response lengths
## Error Handling
- Raise `RuntimeError` with complete, literal information
- Include environment variable names in error text (D-03 from LESSONS_LEARNED.md)
- Never bare `except:` or `except Exception:`
## Threading
- Use `threading.Lock()` for simple mutual exclusion
- Use `threading.RLock()` when the same thread may re-acquire (example: `YoloWorld._lock` in `src/helios/perception/yolo_world.py:48`)
- Use `threading.Event()` for signaling stop/start conditions
## Strategy Pattern (Protocol-based)
- `WebcamSource` — `src/helios/mocks/webcam_source.py`
- `MJPEGSource` — `src/helios/perception/mjpeg_source.py`
- Both satisfy `FrameSource` contract
## Module __all__ Export Lists
## Data Classes
## Context Managers
## Comments
- Explain WHY, not WHAT (code already shows what)
- Reference decisions by code-review ID: `# D-13`, `# D-28`, `# L-14`
- Explain threading rationale and race conditions
- Clarify non-obvious performance tradeoffs
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

## Pattern Overview
- Protocol-driven abstraction layer (`FrameSource`, `DistanceSource`) enabling pluggable hardware and mock implementations
- Strategy pattern for vision backends (`VisionRouter`) switching between local Ollama (offline) and cloud GPT-4o (with fallback)
- MQTT for distributed I/O: sonar (distance) and haptic (vibration motors) communicate via pub/sub with embedded ESP32 firmware
- Threading for I/O isolation: background webcam/MJPEG pump, MQTT network loop, and optional PTT voice pipeline run concurrently
- Synchronous core loops (perception and voice dispatch) with asynchronous I/O at boundaries (camera threads, MQTT clients, cloud API calls)
## Layers
- Purpose: Acquire and process sensor inputs — frames from camera/MJPEG and distance from sonar
- Location: `src/helios/perception/`
- Contains: `FrameSource` protocol, `WebcamSource` (local camera via cv2), `MJPEGSource` (ESP32-CAM stream), `DistanceSource` protocol, `MQTTSonarSource` (MQTT distance), `HandTracker` (MediaPipe hand landmarks), `YoloWorld` (open-vocabulary object detection)
- Depends on: cv2 (OpenCV), numpy, mediapipe, ultralytics (YOLO), paho-mqtt
- Used by: `perception_loop.py` and `voice/ptt_loop.py` for frame capture and distance queries
- Purpose: Scene understanding through image captioning and object detection
- Location: `src/helios/vision/`
- Contains: `LLavaClient` (local Ollama llava:7b), `VisionRouter` (strategy switch local vs. cloud + fallback), `DEFAULT_SCENE_PROMPT`
- Depends on: ollama Python package, openai package (GPT-4o), numpy, cv2
- Used by: `perception_loop.py` (on-demand describe via 'd' key) and `voice/ptt_loop.py` (SCENE_DESC intent handler)
- Purpose: Speech input, transcription, intent classification, and audio output
- Location: `src/helios/voice/` and `src/helios/cloud/`
- Contains: `MicSource` (sounddevice-backed recorder, 16 kHz mono WAV), `ptt_loop.py` (push-to-talk main loop), intent classifier, Whisper transcriber, TTS speaker
- Depends on: pynput (keyboard listener), sounddevice (audio I/O), openai (Whisper, intent, TTS), pyttsx3 (TTS fallback)
- Used by: Voice interaction workflows; dispatches intents (SCENE_DESC, DISTANCE_QUERY, GRAB_GUIDANCE, UNKNOWN)
- Purpose: Haptic feedback to wearable vibration motors
- Location: `src/helios/output/`
- Contains: `HapticPublisher` (MQTT client publishing pulse commands to ESP32 haptic firmware), channel definitions (wrist/belt zones)
- Depends on: paho-mqtt
- Used by: Intent handlers to provide tactile feedback (planned Phase 4)
- Purpose: External API clients for GPT-4o, Whisper, TTS, and intent classification
- Location: `src/helios/cloud/`
- Contains: `openai_client.py` (singleton OpenAI client per D-05), `intent.py` (GPT-4o-mini strict JSON classifier), `whisper.py` (transcriber), `tts.py` (TTS via API)
- Depends on: openai package
- Used by: `vision/router.py`, `voice/ptt_loop.py`, perception loop describe handlers
- Purpose: Embedded sensors and actuators on ESP32 microcontrollers
- Location: `firmware/sonar_esp32/` (HC-SR04 sonar) and `firmware/haptic_esp32/` (vibration motors)
- Contains: Arduino sketches publishing distance to `helios/sonar/distance_mm` and subscribing to `helios/haptic/<channel>`
- Protocol: MQTT topics with int-mm payloads and JSON PWM commands
- Used by: Python host via MQTT; runs independently
## Data Flow
- Perception: stateless iterator; fresh frames pulled on-demand
- MQTT sources: Behind threading.Lock() for concurrent reads (e.g., distance read from main loop while MQTT network thread writes)
- YOLO: threading.RLock() around set_classes()+forward to prevent head-shape races (D-12)
- Voice PTT: PTTState dataclass carries mic, webcam, router, last_intent, transcript across pipeline stages
## Key Abstractions
- Purpose: Abstract camera/MJPEG acquisition
- Examples: `WebcamSource` (cv2.VideoCapture), `MJPEGSource` (HTTP multipart stream)
- Pattern: start()/get_latest_frame()/stop() lifecycle; background thread returns freshest frame copy, drops old ones (no queue backlog)
- Purpose: Abstract sonar/ToF distance sensor
- Examples: `MockDistanceSource` (constant), `MQTTSonarSource` (MQTT subscriber)
- Pattern: get_distance_mm() → int|None; thread-safe behind lock if asynchronous
- Purpose: Switch between local (free, always-on) and cloud (better quality, online-only) vision backends with transparent fallback
- Pattern: Single entry point describe_scene(frame, prompt) → str; handles retry logic internally
- Decision tracking: _offline_until (monotonic deadline), _fallback_announced (one-time announcement per outage window)
- Purpose: Standardized perception output contracts
- Pattern: HandLandmarks bundles 21-point hand skeleton + pixel bbox; Bbox bundles label, xyxy coords, confidence score
## Entry Points
- Location: `src/helios/perception_loop.py`
- Triggers: `uv run python -m helios.perception_loop --duration 60 --show` (dev mode with display)
- Responsibilities: 
- Location: `src/helios/voice/ppt_loop.py`
- Triggers: `uv run python -m helios.voice.ppt_loop [--show] [--mjpeg URL]`
- Responsibilities:
## Error Handling
- **Secret validation:** `openai_client.get_client()` raises RuntimeError immediately if OPENAI_API_KEY not set (D-03, canonical KEY_MISSING_MSG); called early in main() to fail before user-facing setup
- **Cloud fallback:** VisionRouter catches APIError/APITimeoutError/RateLimitError, enters offline window (60s), retries cloud after deadline; all logic centralized (D-26)
- **MQTT:** Paho's on_connect callback re-subscribes automatically on broker reconnects (idiom); on_message updates cache under lock; network errors logged at WARNING, next retry on loop iteration
- **Camera/MJPEG:** WebcamSource.start() raises RuntimeError if cv2.VideoCapture fails (permission denied or device missing); MJPEGSource.start() logs failures and exponential backoff (0.5–5s)
- **Audio:** MicSource catches sounddevice errors; pynput keyboard listener wrapped in try/except to prevent crashes
- **Intent classify:** On parse error or API exception, returns IntentResult("UNKNOWN", None) and logs WARNING; voice loop's UNKNOWN handler plays "Sorry, please repeat." (VOIC-05)
## Cross-Cutting Concerns
- Framework: Python logging module with root logger configured to INFO in main()
- Named loggers per module (e.g., `log = logging.getLogger(__name__)`)
- Key events: source startup/shutdown, MQTT connect/disconnect, cloud fallback, perception iterations (every N frames)
- Secret presence (OPENAI_API_KEY checked in get_client())
- Frame shape (LLavaClient._encode validates HxWx3 BGR)
- Distance range (HC-SR04 firmware filters <20 mm or >4000 mm as out_of_range)
- Intent values constrained to Literal["SCENE_DESC", "DISTANCE_QUERY", "GRAB_GUIDANCE", "UNKNOWN"]
- OpenAI: API key from OPENAI_API_KEY env var (no other auth methods)
- MQTT: Broker address from MQTT_BROKER env var or default "helios.local" (no credentials)
- Keyboard: macOS Accessibility permission required for pynput listener (checked via 5s timeout probe in ppt_loop main())
<!-- GSD:architecture-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd:quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd:debug` for investigation and bug fixing
- `/gsd:execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd:profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
