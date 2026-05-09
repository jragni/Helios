# Stack Research

**Domain:** Wearable AI-driven accessibility system — real-time computer vision + cloud/offline LLM vision backend + push-to-talk voice loop + MQTT hardware (ESP32 sonar + haptics)
**Researched:** 2026-05-09
**Confidence:** HIGH (core carry-over stack verified against current PyPI/official docs; model landscape verified via WebSearch)

---

## Carry-Over Stack Verdict (Keep / Swap / Augment)

| Component | Carry-Over Choice | Verdict | Rationale |
|-----------|------------------|---------|-----------|
| Python 3.11 + uv | Python 3.11.14 / uv 0.9.27 | **Keep** | 3.11 is the locked runtime; current uv is 0.11.12 but 0.9.27 works fine and the lockfile is the stable artifact — no forced upgrade |
| YOLO-World v2 | yolov8m-worldv2.pt via ultralytics ≥8.3 | **Keep** | ultralytics 8.4.47 is current; YOLO-World v2 is still documented and actively recommended for open-vocabulary detection; YOLO26 exists but is not yet available in YOLO-World open-vocab variant — no drop-in swap |
| MediaPipe Tasks API | mediapipe ≥0.10.14 | **Augment** | Latest is 0.10.35 (2026-04-27); bump pin to ≥0.10.21 minimum — the numpy<2 constraint is still declared by official wheels through at least 0.10.21; keep numpy pinned to `<2.2` as the project already does; do NOT use legacy `mp.solutions` API |
| OpenCV | opencv-python ≥4.10 | **Augment** | Latest is 4.13.0.92 (2026-02-05) with ARM64 macOS wheels. Pin ≥4.11 for Apple Silicon stability; use `opencv-python-headless` variant if display windows are not needed in prod to reduce bundle size |
| openai SDK | openai ≥2.36.0 | **Keep** | 2.36.0 is the current stable release (2026-05-07); the cloud integration is correct |
| GPT-4o vision | gpt-4o (model string) | **Keep** | GPT-4o vision remains the right model for <3 s scene description at $2.50/M input tokens. GPT-4.1 has improved benchmark scores and larger context but adds no practical benefit for a one-sentence scene cue; keep gpt-4o for cost/latency parity |
| GPT-4o-mini intent | gpt-4o-mini | **Keep** | Correct model for lightweight intent classification |
| Whisper STT | whisper-1 via openai SDK | **Keep** | Still fastest path for hackathon. Local alternatives (faster-whisper, whisper.cpp) reduce cost and latency but add an integration surface; only swap if OpenAI budget runs out mid-demo |
| TTS-1 | tts-1, voice "alloy" | **Augment** | `gpt-4o-mini-tts` is now OpenAI's primary TTS recommendation; it matches tts-1-hd quality, streams first audio chunk in ~300–600 ms, and is ~50% cheaper than ElevenLabs. For a pre-recorded-cue-heavy demo the cost difference is trivial; either works. If naturalness matters for judges, swap model string to `gpt-4o-mini-tts` — single line change in `tts.py` |
| Ollama + LLaVA-1.6 7B | ollama ≥0.4 + llava:7b | **Augment** | ollama Python client is now 0.6.2 (2026-04-29). LLaVA 1.6 7B is outclassed by `qwen2.5-vl:7b` on general scene understanding in 2026 benchmarks. **Augment: pull `qwen2.5-vl:7b` as the offline fallback model** instead — single line change in `LLavaClient`. LLaVA still works; Qwen2.5-VL is meaningfully better on spatial/object queries |
| paho-mqtt | paho-mqtt ≥2.1 | **Keep** | 2.1.0 is current stable. CallbackAPIVersion.VERSION2 is already the recommended API pattern |
| sounddevice + soundfile | sounddevice ≥0.5.5 / soundfile ≥0.13.1 | **Keep** | sounddevice 0.5.5 released 2026-01-23 is current; no replacement warranted for PTT playback/record |
| pynput | pynput ≥1.8.1 | **Keep** | Correct for keyboard PTT fallback on macOS |
| Mosquitto broker | mosquitto (brew, version TBD) | **Keep** | Standard MQTT broker for local network; install via `brew install mosquitto`; no version concerns |
| PlatformIO + Arduino/ESP32 | PlatformIO + espressif32 + PubSubClient | **Keep** | Standard path for ESP32 MQTT firmware; espMqttClient (bertmelis) is a modern alternative if PubSubClient instability is encountered |
| numpy | numpy ≥1.26,<2.2 | **Keep** | Hard constraint: mediapipe wheels through 0.10.35 still declare numpy<2 dependency in PyPI metadata. Pin to `<2.2` as the lockfile already does; do not chase numpy 2.x until mediapipe officially drops the constraint |

---

## Recommended Stack (Full Picture)

### Core Technologies

| Technology | Current Version | Purpose | Status |
|------------|-----------------|---------|--------|
| Python | 3.11.14 (locked) | Primary runtime | Keep |
| uv | 0.9.27 (project lock) / 0.11.12 (latest) | Package manager + lockfile | Keep |
| ultralytics | 8.4.47 | YOLO-World open-vocab object detection | Keep (already pinned ≥8.3) |
| mediapipe | 0.10.35 (latest) | Hand landmark detection via Tasks API | Augment pin to ≥0.10.21 |
| opencv-python | 4.13.0.92 (latest) | Frame capture, JPEG encode, image ops | Augment pin to ≥4.11 |
| openai | 2.36.0 (current) | GPT-4o vision, Whisper STT, TTS, intent | Keep |
| ollama (Python client) | 0.6.2 (latest) | Offline vision fallback via local Ollama daemon | Augment client version |
| paho-mqtt | 2.1.0 (current) | MQTT pub/sub for sonar + haptic channels | Keep |
| sounddevice | 0.5.5 (current) | PTT mic recording + TTS playback | Keep |
| soundfile | 0.13.1 (current) | WAV I/O for Whisper input + TTS output | Keep |
| pynput | 1.8.1+ | Keyboard PTT fallback binding | Keep |
| mosquitto | latest via brew | MQTT broker on laptop hotspot | Keep |
| PlatformIO (CLI) | latest | ESP32 firmware build + flash | Keep |

### AI Models in Use

| Model | Served By | Role | Status |
|-------|-----------|------|--------|
| yolov8m-worldv2.pt (55 MB) | ultralytics | Open-vocabulary object detection | Keep |
| hand_landmarker.task (7.5 MB) | mediapipe | Hand landmark detection | Keep |
| gpt-4o | OpenAI API | Scene description vision | Keep |
| gpt-4o-mini | OpenAI API | Intent classification | Keep |
| whisper-1 | OpenAI API | STT for PTT loop | Keep |
| tts-1 | OpenAI API | TTS synthesis | Keep (gpt-4o-mini-tts is a viable upgrade) |
| llava:7b (local) OR qwen2.5-vl:7b | Ollama 0.23+ | Offline vision fallback | Augment: prefer qwen2.5-vl:7b |

### ESP32 Firmware Stack

| Component | Library/Framework | Notes |
|-----------|------------------|-------|
| Build system | PlatformIO CLI | `pio run`, `pio device upload` |
| Framework | Arduino (espressif32) | Proven for ESP32 MQTT + GPIO |
| MQTT client | PubSubClient (knolleary) | MQTT 3.1.1; matches paho-mqtt broker protocol |
| MQTT alt | espMqttClient (bertmelis) | Drop-in if PubSubClient instability found |
| Sonar driver | Custom HC-SR04 GPIO timing | In `firmware/sonar_esp32/` |
| PWM haptic | `ledcWrite()` / PWM channel | ERM vibration motors, duty + duration pattern |

### Supporting Python Libraries

| Library | Version | Purpose | Notes |
|---------|---------|---------|-------|
| python-dotenv | ≥1.2.2 | Load `OPENAI_API_KEY` from `.env` | Keep |
| numpy | ≥1.26, <2.2 | Array ops for frames | CRITICAL pin; do not remove upper bound |
| pillow | ≥10.4 | Image fallback utilities | Keep |
| pytest | ≥9.0.3 | Test runner | Keep |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `mp.solutions.*` (legacy MediaPipe) | Deprecated; not maintained; Tasks API is the only forward path | MediaPipe Tasks API (`HandLandmarker`) |
| numpy ≥2.2 | MediaPipe wheels still declare `numpy<2` through at least 0.10.35; will produce import errors at runtime | numpy ≥1.26, <2.2 |
| ultralytics 8.3.41–8.3.42 | These versions were compromised in a supply-chain attack (XMRig miner injected); PyPI removed them | ultralytics ≥8.3.43 or ≥8.4.x |
| LLaVA 1.6 7B as primary offline model | Outclassed in 2026 by Qwen2.5-VL 7B on scene understanding and spatial queries | `qwen2.5-vl:7b` via Ollama |
| GPT-5 / GPT-4.1 for scene description | Higher cost and no latency benefit for a single-sentence scene cue; GPT-4o is the right price/latency point | gpt-4o |
| Always-on VAD / real-time streaming STT | Adds complexity; push-to-talk is deliberately chosen to sidestep real-time audio expertise gap (PROJECT.md Out of Scope) | whisper-1 on PTT-gated clips |
| opencv-python 4.10 on Apple Silicon | 4.11+ migrated ARM64 macOS to a newer minimum; 4.10 may have ARM wheel gaps | opencv-python ≥4.11 |
| MQTT 5.0 features (paho or broker) | PubSubClient on ESP32 is MQTT 3.1.1 only; mismatched protocol adds bridge complexity with no demo benefit | MQTT 3.1.1 throughout |
| USB-serial direct wiring for sonar | Binds laptop to physical cable; ESP32 + MQTT is already implemented and wearable-friendly | MQTT transport via laptop hotspot |

---

## Version Compatibility Matrix

| Package | Compatible With | Constraint | Notes |
|---------|-----------------|-----------|-------|
| mediapipe ≥0.10.21 | numpy <2.2 | Hard (PyPI metadata) | Do not bump numpy to 2.x until mediapipe officially drops constraint |
| ultralytics ≥8.3.43 | numpy <2.2, opencv-python ≥4.11 | Avoid 8.3.41–8.3.42 (supply chain) | 8.4.x series is clean |
| openai 2.36.0 | Python 3.9+ | — | Matches project Python 3.11 |
| ollama 0.6.2 | Python 3.8+ | — | Async client available but sync is fine for demo |
| paho-mqtt 2.1 | Python 3.7+ | Use CallbackAPIVersion.VERSION2 | Already in codebase |
| sounddevice 0.5.5 | Python 3.8+, PortAudio (auto-bundled on macOS pip) | — | No manual PortAudio install needed |

---

## Installation Snapshot

```bash
# Python deps (uv manages all of this via uv.lock)
uv sync

# Ollama daemon (must be running before LLavaClient import)
# Install: brew install ollama  OR  https://ollama.com/download
ollama pull qwen2.5-vl:7b     # preferred offline fallback (2026)
ollama pull llava:7b           # keep as secondary if Qwen pull fails

# MQTT broker (laptop hotspot)
brew install mosquitto
brew services start mosquitto

# PlatformIO CLI (ESP32 firmware)
pip install platformio          # or via brew
pio pkg install                 # in firmware/ directory

# Mosquitto version check
mosquitto --version
```

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Open-vocab detection | YOLO-World v2 (yolov8m-worldv2.pt) | YOLO26 open-vocab | YOLO26 open-vocab variant not yet in ultralytics stable package as of 2026-05; no benefit for existing bench-validated weights |
| Offline vision LLM | qwen2.5-vl:7b | llava:7b | LLaVA 1.6 7B is outperformed on object/scene understanding in 2026; Qwen2.5-VL is the current default recommendation |
| Offline vision LLM | qwen2.5-vl:7b | llama3.2-vision:11b | 11B requires more VRAM; 7B Qwen2.5-VL matches/exceeds it on most benchmarks; fits in 16 GB unified memory alongside perception stack |
| TTS | tts-1 | gpt-4o-mini-tts | gpt-4o-mini-tts is higher quality and now OpenAI's primary recommendation; tts-1 works fine for demo and is a single `model=` string change to upgrade |
| STT | whisper-1 (cloud) | faster-whisper / whisper.cpp (local) | Local alternatives cut cost and latency but add integration surface; only worth the swap if OpenAI budget is exhausted |
| MQTT client (ESP32) | PubSubClient | espMqttClient (bertmelis) | PubSubClient is simpler and already scaffolded; espMqttClient is the drop-in if instability is encountered |
| Package manager | uv | pip / poetry | uv is faster and the lockfile is already committed; no reason to switch |

---

## Stack Patterns for Helios Demo

**Cloud path (nominal):**
- USB-C cam → OpenCV → YOLO-World + MediaPipe → GPT-4o vision → TTS-1 → sounddevice playback
- PTT button (ESP32 MQTT) → sounddevice record → whisper-1 → GPT-4o-mini intent → handler dispatch

**Offline fallback path (GPT-4o timeout >10 s):**
- Same perception stack → Ollama qwen2.5-vl:7b → sounddevice playback
- VisionRouter flips on cloud error; speaks "switching to offline mode" once (VISN-02)

**Hardware path:**
- ESP32 sonar firmware → mosquitto broker → paho-mqtt MQTTSonarSource
- HapticPublisher → paho-mqtt → mosquitto → ESP32 haptic firmware → ERM motors

---

## Sources

- ultralytics PyPI / GitHub releases — ultralytics 8.4.47 confirmed current; YOLO-World v2 still documented in official 2026 Ultralytics YOLO Docs. MEDIUM confidence (WebSearch).
- mediapipe PyPI releases — 0.10.35 released 2026-04-27 confirmed; numpy<2 constraint confirmed still present through 0.10.21+ via GitHub issues. HIGH confidence (multiple sources).
- openai PyPI — 2.36.0 confirmed current (released 2026-05-07). HIGH confidence (WebSearch).
- ollama PyPI — 0.6.2 confirmed current (released 2026-04-29). HIGH confidence (WebSearch).
- opencv-python PyPI — 4.13.0.92 confirmed current (released 2026-02-05). HIGH confidence (WebSearch).
- sounddevice docs — 0.5.5 confirmed current (released 2026-01-23). HIGH confidence (WebSearch + readthedocs).
- paho-mqtt PyPI — 2.1.0 confirmed current stable. HIGH confidence (WebSearch + Eclipse Paho).
- gpt-4o-mini-tts — Confirmed as OpenAI's current primary TTS recommendation; first audio chunk ~300–600 ms. MEDIUM confidence (WebSearch, OpenAI developer docs referenced).
- LLaVA vs Qwen2.5-VL comparison — Qwen2.5-VL 7B outperforms LLaVA 1.6 on scene/object benchmarks in 2026. MEDIUM confidence (multiple ML community sources).
- YOLO26 open-vocab status — YOLO26 exists but open-vocab variant not confirmed available as a YOLO-World-style drop-in; YOLO-World v2 still the safe choice. LOW-MEDIUM confidence (WebSearch; official docs unclear on YOLO26 open-vocab API parity).
- PubSubClient / espMqttClient — PlatformIO registry confirmed; PubSubClient MQTT 3.1.1 limitation confirmed. MEDIUM confidence (WebSearch).
- numpy<2 hard constraint — Confirmed persists in mediapipe wheels; mediated via patching tools but not officially resolved. HIGH confidence (GitHub issues, PyPI metadata).
- uv 0.11.12 — Confirmed current; project can stay on 0.9.27 lockfile without issues. HIGH confidence (WebSearch + astral-sh releases).

---
*Stack research for: Helios wearable AI accessibility system*
*Researched: 2026-05-09*
