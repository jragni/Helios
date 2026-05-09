# Codebase Structure

**Analysis Date:** 2026-05-09

## Directory Layout

```
/Users/ragglesoft/Desktop/helios/
├── src/helios/                      # Main Python package (installed via setup.py or uv)
│   ├── __init__.py
│   ├── perception_loop.py            # Main entry point: camera → YOLO + MediaPipe
│   ├── perception/                   # Sensor abstraction & low-level detection
│   │   ├── __init__.py
│   │   ├── protocol.py               # FrameSource, DistanceSource protocols
│   │   ├── yolo_world.py             # YoloWorld wrapper (open-vocabulary detection)
│   │   ├── hands.py                  # MediaPipe hand landmark tracker
│   │   ├── mqtt_sonar_source.py      # MQTTSonarSource (sonar distance via MQTT)
│   │   └── mjpeg_source.py           # MJPEGSource (ESP32-CAM HTTP stream)
│   ├── mocks/                        # Test doubles & local camera implementations
│   │   ├── __init__.py
│   │   └── webcam_source.py          # WebcamSource (cv2.VideoCapture threaded pump)
│   ├── vision/                       # Scene understanding (local Ollama + cloud GPT-4o)
│   │   ├── __init__.py
│   │   ├── router.py                 # VisionRouter (strategy switch + fallback)
│   │   ├── llava_client.py           # LLavaClient (local Ollama llava:7b wrapper)
│   │   ├── prompts.py                # DEFAULT_SCENE_PROMPT
│   │   └── _fallback_audio.py        # Offline mode audio cues
│   ├── voice/                        # Voice I/O (mic, transcription, intent, TTS)
│   │   ├── __init__.py
│   │   ├── ptt_loop.py               # Main entry point: push-to-talk voice interaction
│   │   └── mic.py                    # MicSource (sounddevice recorder, 16 kHz mono)
│   ├── cloud/                        # External API clients (OpenAI)
│   │   ├── __init__.py
│   │   ├── openai_client.py          # Singleton OpenAI() instance (D-05)
│   │   ├── intent.py                 # Intent classifier (gpt-4o-mini strict JSON)
│   │   ├── whisper.py                # Speech-to-text
│   │   └── tts.py                    # Text-to-speech
│   ├── output/                       # Output transports (haptic, future display)
│   │   ├── __init__.py
│   │   └── haptic.py                 # HapticPublisher (MQTT vibration motor commands)
│   └── bench/                        # Benchmarking & performance testing
│       ├── __init__.py
│       ├── perception_loop.py        # Bench wrapper for perception
│       ├── yolo.py                   # YOLO detection benchmarks
│       ├── mediapipe.py              # MediaPipe hand tracking benchmarks
│       └── llava.py                  # LLaVA description benchmarks
├── firmware/                         # Embedded systems (ESP32 PlatformIO projects)
│   ├── sonar_esp32/                  # HC-SR04 sonar → MQTT distance publisher
│   │   ├── platformio.ini
│   │   ├── src/
│   │   │   └── main.cpp              # WiFi + MQTT + sonar read loop
│   │   └── include/
│   │       └── secrets.h             # WiFi SSID/pass (git-ignored)
│   └── haptic_esp32/                 # Vibration motor controller
│       ├── platformio.ini
│       ├── src/
│       │   └── main.cpp              # MQTT subscriber → PWM motor drive
│       └── include/
│           └── secrets.h             # WiFi credentials (git-ignored)
├── scripts/                          # Standalone utilities & smoke tests
│   ├── smoke_openai.py               # Verify OPENAI_API_KEY works
│   └── warmup_llava.py               # Pre-load Ollama model cache
├── docs/                             # Documentation (design decisions, architecture)
├── tests/                            # (If added) Unit & integration test suite
├── bench/                            # (Symlink or copy) Benchmark runner
├── pyproject.toml                    # uv/pip project config, dependencies
├── uv.lock                           # uv lock file (if using uv)
├── .gitignore                        # Excludes .env, hand_landmarker.task, etc.
├── README.md                         # Project overview
└── .planning/codebase/               # GSD documentation (this file)
    ├── ARCHITECTURE.md               # Layer definitions, data flow, patterns
    └── STRUCTURE.md                  # (This file) Directory layout & file locations
```

## Directory Purposes

**src/helios/:**
- Purpose: Main Python package root
- Contains: All executable code (perception, vision, voice, output, cloud)
- Key files: `perception_loop.py` and `voice/ppt_loop.py` are user-facing entry points

**src/helios/perception/:**
- Purpose: Sensor acquisition and low-level computer vision
- Contains: `FrameSource` protocol + implementations (WebcamSource, MJPEGSource), `DistanceSource` protocol + implementations (MockDistanceSource, MQTTSonarSource), object detection (YoloWorld), hand tracking (MediaPipe)
- Key abstraction: FrameSource protocol allows swapping camera source without changing callers

**src/helios/mocks/:**
- Purpose: Test doubles and local implementations
- Contains: WebcamSource (cv2 local camera), MockDistanceSource (placeholder)
- Naming: Classes follow Mock* pattern for test doubles (e.g., MockDistanceSource)

**src/helios/vision/:**
- Purpose: Image captioning and scene understanding
- Contains: VisionRouter (strategy switch local vs. cloud), LLavaClient (Ollama), prompts
- Key logic: VisionRouter centralizes cloud fallback (D-26..D-28) in one location

**src/helios/voice/:**
- Purpose: Audio I/O and voice interaction
- Contains: MicSource (recording), ppt_loop.py (main voice loop)
- Threading: pynput.Listener spawns threads for keyboard events; ppt_loop manages cv2 window on main thread (macOS requirement)

**src/helios/cloud/:**
- Purpose: OpenAI API clients
- Contains: Singleton openai_client.py, intent classifier, Whisper transcriber, TTS speaker
- Dependency: All cloud modules import get_client() from openai_client.py (D-05)

**src/helios/output/:**
- Purpose: Output transports (haptic feedback, future display)
- Contains: HapticPublisher (MQTT vibration motor commands)
- Expansion: Add display.py or other output drivers here

**src/helios/bench/:**
- Purpose: Performance benchmarking and profiling
- Contains: Wrappers around perception, vision, and voice components to measure latency/throughput
- Usage: `uv run python -m helios.bench.perception_loop --duration 60`

**firmware/sonar_esp32/:**
- Purpose: HC-SR04 sonar range sensor on ESP32
- Contains: PlatformIO project structure (platformio.ini, src/main.cpp, include/)
- Topic schema: Publishes int-mm to `helios/sonar/distance_mm`, status to `helios/sonar/status`
- Wiring: TRIG→GPIO 14, ECHO→GPIO 15 (with 1k/2k voltage divider for 5V→3.3V level shift)

**firmware/haptic_esp32/:**
- Purpose: Vibration motor controller on ESP32
- Contains: PlatformIO project structure
- Topic schema: Subscribes to `helios/haptic/<channel>` for JSON `{"d": duty, "ms": ms}` commands
- Channels: WRIST_TOP, WRIST_BOT, WRIST_L, WRIST_R, BELT_L, BELT_C, BELT_R

**scripts/:**
- Purpose: Standalone utilities and smoke tests
- smoke_openai.py: Verify OPENAI_API_KEY environment variable and API connectivity
- warmup_llava.py: Pre-load Ollama llava:7b model into GPU cache

## Key File Locations

**Entry Points:**
- `src/helios/perception_loop.py`: Visual perception main loop (YOLO + MediaPipe + describe-on-key)
- `src/helios/voice/ppt_loop.py`: Voice interaction main loop (push-to-talk keyboard → Whisper → intent → dispatch)

**Configuration:**
- `pyproject.toml`: Python project metadata, dependencies, build config
- `.env`: (Not committed) OpenAI API key, MQTT broker address
- `firmware/sonar_esp32/include/secrets.h`: (Not committed) WiFi SSID/pass for sonar ESP32
- `firmware/haptic_esp32/include/secrets.h`: (Not committed) WiFi SSID/pass for haptic ESP32

**Core Logic:**
- `src/helios/perception/protocol.py`: FrameSource and DistanceSource protocol definitions
- `src/helios/perception/yolo_world.py`: YoloWorld wrapper with thread-safe set_classes + detect
- `src/helios/vision/router.py`: VisionRouter strategy switch + cloud fallback logic
- `src/helios/cloud/openai_client.py`: Singleton OpenAI client (D-05)
- `src/helios/voice/ppt_loop.py`: Main voice loop with keyboard listener and intent dispatch

**Testing:**
- `src/helios/bench/perception_loop.py`: Benchmark wrapper for perception loop latency
- `scripts/smoke_openai.py`: Test OpenAI API key and connectivity

## Naming Conventions

**Files:**
- Snake_case module names: `perception_loop.py`, `llava_client.py`, `mqtt_sonar_source.py`
- Protocol interfaces: `protocol.py` (singular file bundling related protocols)
- Test/mock implementations: `*_source.py` suffix for FrameSource/DistanceSource implementations (e.g., `WebcamSource`, `MQTTSonarSource`, `MJPEGSource`)

**Directories:**
- Lowercase plural (src/helios/perception, src/helios/cloud) or functional names (src/helios/bench)
- Layer names (perception, vision, voice, output, cloud) match architectural layers

**Classes:**
- PascalCase: `WebcamSource`, `YoloWorld`, `VisionRouter`, `HapticPublisher`, `MicSource`
- Mock/test double pattern: `MockDistanceSource` (Mock prefix)
- Protocol classes: Named with suffix (e.g., FrameSource, DistanceSource) per Python typing conventions

**Functions:**
- Snake_case: `get_latest_frame()`, `detect_hands()`, `describe_scene()`, `classify()`

**Types & Constants:**
- NamedTuple classes: `Bbox`, `HandLandmarks`, `IntentResult`
- Literal intent types: `Intent = Literal["SCENE_DESC", "DISTANCE_QUERY", "GRAB_GUIDANCE", "UNKNOWN"]`
- Module-level constants (UPPERCASE): `PLACEHOLDER_DISTANCE_CM`, `DEFAULT_PROMPTS`, `WRIST_TOP`, `BELT_L`

## Where to Add New Code

**New Feature (e.g., new intent handler):**
- Primary code: `src/helios/voice/ppt_loop.py` — add handler function and update DISPATCH dict (D-22)
- Test code: `tests/voice/test_ppt_loop.py` (if added)
- Example: New handler for "LIGHT_CONTROL" intent would live in ppt_loop.py and be added to the DISPATCH table

**New Perception Component (e.g., eye-gaze tracking):**
- Implementation: `src/helios/perception/gaze_tracker.py` (new file following *_tracker.py pattern)
- Protocol abstraction: If it's a new sensor type, add Protocol class to `src/helios/perception/protocol.py`
- Integration: Update `perception_loop.py` to instantiate and call the tracker in perception_iter()

**New Vision Backend (e.g., Claude vision instead of GPT-4o):**
- Implementation: `src/helios/vision/claude_client.py` (new file following *_client.py pattern)
- Integration: Add case in `VisionRouter.describe_scene()` or create new routing strategy class
- Cloud client: Add module `src/helios/cloud/claude_client.py` if API client is complex

**New Output Transport (e.g., LED display):**
- Implementation: `src/helios/output/led_display.py` (new file following *_publisher.py or *_display.py pattern)
- API: Follow HapticPublisher pattern: start()/stop() lifecycle methods, thread-safe public methods
- Integration: Call from intent handlers (e.g., _handle_scene_desc can call display.show_summary())

**Utilities & Helpers:**
- Shared helper functions: `src/helios/utils.py` (create if adding >3 shared utilities)
- Import pattern: `from helios.utils import function_name`

**Testing:**
- Unit tests: `tests/<layer>/<module>.py` (e.g., `tests/perception/test_yolo_world.py`)
- Integration tests: `tests/integration/test_perception_voice_flow.py`
- Fixtures: `tests/conftest.py` (pytest fixtures for mocks, test data)

## Special Directories

**hand_landmarker.task:**
- Purpose: Pre-trained MediaPipe hand landmark model (auto-downloaded on first run)
- Generated: Yes (urllib.request.urlretrieve in perception/hands.py)
- Committed: No (.gitignore entry)
- Location: Repository root, ~75 MB

**firmware/*/src/ and firmware/*/include/:**
- Purpose: PlatformIO project structure (mirrors Arduino IDE layout)
- Generated: No (hand-written Arduino sketches)
- Committed: Yes (except secrets.h which is git-ignored)

**.env and firmware/*/include/secrets.h:**
- Purpose: Environment variables and secrets
- Generated: No (user must create)
- Committed: No (.gitignore entries)
- Contents: OPENAI_API_KEY, MQTT_BROKER, WiFi credentials

**dist/ or build/ (if added):**
- Purpose: Build output for wheel distribution
- Generated: Yes (uv build or python -m build)
- Committed: No (.gitignore entry)

---

*Structure analysis: 2026-05-09*
