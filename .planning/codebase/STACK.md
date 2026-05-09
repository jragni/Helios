# Technology Stack

**Analysis Date:** 2026-05-09

## Languages

**Primary:**
- Python 3.11 - Core accessibility system, all perception, vision, voice, and output modules
- Embedded C/C++ (ESP32 firmware) - Sonar sensor reading and vibration motor control over MQTT (located at `firmware/sonar_esp32/`)

## Runtime

**Environment:**
- CPython 3.11.14 - Reference version in `.python-version`

**Package Manager:**
- uv 0.9.27+ - Fast Python resolver and installer
- Lockfile: `uv.lock` (2600+ dependencies resolved)

## Frameworks

**Core Vision/ML:**
- ultralytics>=8.3 - YOLO-World object detection (yolov8m-worldv2 model)
- mediapipe>=0.10.14 - Hand landmark detection (Tasks API, not legacy mp.solutions)
- opencv-python>=4.10 - Image processing, frame capture, JPEG encoding

**AI/Voice/Cloud:**
- openai>=2.36.0 - GPT-4o vision, GPT-4o-mini intent, Whisper STT, TTS-1 text-to-speech
- ollama>=0.4 - Local LLaVA-1.6 7B offline fallback for vision (sync Python client)

**Audio I/O:**
- sounddevice>=0.5.5 - Microphone recording and speaker playback
- soundfile>=0.13.1 - WAV file I/O (read/write with sounddevice)

**Hardware I/O:**
- paho-mqtt>=2.1 - MQTT pub/sub for sonar distance (MQTTSonarSource subscriber) and haptic motors (HapticPublisher)
- pynput>=1.8.1 - Push-to-talk key binding (monitor keyboard globally)

**Utilities:**
- python-dotenv>=1.2.2 - Load OPENAI_API_KEY from .env file
- numpy>=1.26,<2.2 - Numerical arrays for frame processing
- pillow>=10.4 - Image utilities (fallback image operations)
- openai-clip>=1.0.1 - CLIP model dependency (transitive from openai SDK)

**Testing:**
- pytest>=9.0.3 - Test runner (dev group)

## Key Dependencies

**Critical:**
- openai>=2.36.0 - Why: Gateway to all cloud vision, voice, and intent inference. GPT-4o vision calls include base64-encoded JPEG frames; Whisper handles 16 kHz mono PCM WAV; TTS-1 synthesizes speech; GPT-4o-mini classifies intents. Required for Phase 3+.
- ultralytics>=8.3 - Why: Provides pre-trained YOLOWorld model (yolov8m-worldv2.pt, 55 MB) for open-vocabulary object detection. No inference depends on it but it loads and runs the bundled weights.
- ollama>=0.4 - Why: Pure-Python sync client for local Ollama daemon (localhost:11434). LLavaClient wraps ollama.chat() for offline vision fallback. Required to be running before importing LLavaClient.
- mediapipe>=0.10.14 - Why: Tasks API (v0.10+) for real-time hand landmark detection. Auto-downloads hand_landmarker.task (7.5 MB) on first use.

**Infrastructure:**
- paho-mqtt>=2.1 - Why: Both sonar distance reading (MQTTSonarSource subscriber) and haptic motor control (HapticPublisher publisher) depend on paho for network I/O to broker (default: helios.local:1883).
- sounddevice + soundfile - Why: Together enable PTT loop's blocking audio playback (TTS output) and recording (Whisper input). sounddevice wraps PortAudio; soundfile wraps libsndfile.

## Configuration

**Environment:**
- `.env` file required at repo root (see `.env.example`)
  - `OPENAI_API_KEY` - Mandatory for cloud path (GPT-4o vision, TTS-1, Whisper, GPT-4o-mini). Loaded by `helios.cloud.openai_client.get_client()`
  - `MQTT_BROKER` - Optional; defaults to `"helios.local"` for both sonar (MQTTSonarSource) and haptic (HapticPublisher)

**Build:**
- `pyproject.toml` - Project metadata and dependency declaration
- `uv.lock` - Pinned versions (all 2600+ transitive dependencies)

**Models:**
- `yolov8m-worldv2.pt` (55 MB) - Pre-trained weights in repo root, loaded by YoloWorld at initialization
- `hand_landmarker.task` (7.5 MB) - MediaPipe model in repo root, auto-downloaded from `https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task` if missing

## Platform Requirements

**Development:**
- macOS 25.3+ (tested on Intel/Apple Silicon)
- Ollama 0.23+ (0.15.4 has Metal GPU bug on macOS 25.3 — see README)
- Python 3.11.14 via pyenv/brew/uv

**Production:**
- Linux (ESP32 MQTT firmware setup) or macOS (with Ollama for fallback)
- MQTT broker (mosquitto or equivalent) on local network
- ESP32 with HC-SR04 sonar and ERM vibration motors (firmware at `firmware/sonar_esp32/`)
- Optional: OpenAI API key for cloud vision path

---

*Stack analysis: 2026-05-09*
