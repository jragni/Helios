# External Integrations

**Analysis Date:** 2026-05-09

## APIs & External Services

**OpenAI Platform:**
- GPT-4o vision - Scene description from camera frames (Phase 3)
  - SDK/Client: `openai>=2.36.0`
  - Used by: `src/helios/vision/router.py` (VisionRouter.describe_scene when use_openai=True)
  - Call: `client.chat.completions.create(model="gpt-4o", messages=[text + base64 image], max_tokens=80, timeout=10.0)`
  - Auth: `OPENAI_API_KEY` env var, loaded by `src/helios/cloud/openai_client.get_client()`

- GPT-4o-mini - Intent classification (SCENE_DESC | DISTANCE_QUERY | GRAB_GUIDANCE | UNKNOWN)
  - SDK/Client: `openai>=2.36.0`
  - Used by: `src/helios/cloud/intent.py` (classify function)
  - Call: `client.chat.completions.create(model="gpt-4o-mini", response_format={"type": "json_object"}, messages=[system + user transcript], max_tokens=60, timeout=10.0)`
  - Auth: `OPENAI_API_KEY`

- Whisper STT - Speech-to-text transcription
  - SDK/Client: `openai>=2.36.0`
  - Used by: `src/helios/cloud/whisper.py` (transcribe function)
  - Input: 16 kHz mono 16-bit PCM WAV (bytes or Path)
  - Call: `client.audio.transcriptions.create(model="whisper-1", file=(filename, wav_bytes, "audio/wav"), timeout=15.0)`
  - Auth: `OPENAI_API_KEY`

- TTS-1 Text-to-Speech - Synthesis and caching
  - SDK/Client: `openai>=2.36.0`
  - Used by: `src/helios/cloud/tts.py` (speak and synthesize_to_cache functions)
  - Call: `client.audio.speech.create(model="tts-1", voice="alloy", input=text, response_format="wav", timeout=10.0)`
  - Output: 24 kHz WAV, cached at `src/helios/cache/tts/<sha1>.wav`
  - Auth: `OPENAI_API_KEY`

## Data Storage

**Databases:**
- Not applicable - no persistent database

**File Storage:**
- Local filesystem only
  - TTS cache: `src/helios/cache/tts/<sha1>.wav` (one SHA1-keyed file per unique voice+text pair)
  - Model files: `yolov8m-worldv2.pt` and `hand_landmarker.task` in repo root
  - Test fixtures: `tests/fixtures/say_what_do_you_see.wav` (seeded by TTS smoke test)

**Caching:**
- In-memory: LLavaClient singleton (module-level _default_client)
- In-memory: OpenAI client singleton via lru_cache in `src/helios/cloud/openai_client.get_client()`
- On-disk: TTS output WAV files (SHA1-keyed by voice+text)

## Authentication & Identity

**Auth Provider:**
- Custom (API key file)
  - Implementation: `OPENAI_API_KEY` environment variable loaded by `src/helios/cloud/openai_client.get_client()`
  - Failure mode: RuntimeError with message "OPENAI_API_KEY not set. Add it to .env at repo root." (D-03)
  - Required for: GPT-4o vision, GPT-4o-mini intent, Whisper STT, TTS-1

**Local Service Auth:**
- None - Ollama (localhost:11434) and MQTT (helios.local:1883) are assumed to be on trusted local network

## Monitoring & Observability

**Error Tracking:**
- None detected - errors are logged locally via Python logging module

**Logs:**
- Python logging module
  - Loggers named after modules (e.g., `helios.vision.router`, `helios.cloud.openai_client`)
  - DEBUG: Frame/response metadata (character counts, model choices)
  - INFO: Cache hits, connection status, model downloads
  - WARNING: Cloud failures, fallback activations, malformed inputs

## CI/CD & Deployment

**Hosting:**
- Not detected - this is a client-side accessibility app (runs on user's device)

**CI Pipeline:**
- Not detected - no GitHub Actions or CI config found

## Environment Configuration

**Required env vars:**
- `OPENAI_API_KEY` - Mandatory for cloud path (GPT-4o, Whisper, TTS-1, GPT-4o-mini). No default. Raises RuntimeError if unset and cloud features are accessed.

**Optional env vars:**
- `MQTT_BROKER` - Defaults to `"helios.local"` (sonar distance subscriber and haptic motor publisher)
- `BLIND_ASSIST_TTS_PLAY` - Set to `"0"` to skip playback during TTS smoke test (defaults to `"1"`)

**Secrets location:**
- `.env` file at repo root (template: `.env.example`)
- Never commit actual `.env` (already in `.gitignore`)

## Webhooks & Callbacks

**Incoming:**
- None detected

**Outgoing:**
- MQTT publish (sonar updates): MQTTSonarSource subscribes to `helios/sonar/distance_mm` (int payload) and `helios/sonar/status` (enum)
- MQTT publish (haptic commands): HapticPublisher publishes to `helios/haptic/<channel>` with JSON payload `{"d": duty, "ms": ms}`

## Model Downloads

**MediaPipe Hand Landmarker:**
- URL: `https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task`
- Size: 7.5 MB
- Location: `hand_landmarker.task` in repo root
- Cached: Yes - checked at startup, downloaded once on first run
- Fallback: If download fails, HandTracker initialization fails (no stub)

**YOLO-World v2:**
- File: `yolov8m-worldv2.pt` (55 MB)
- Bundled: Yes - pre-packaged in repo root (not downloaded; assumed present)
- Source: Ultralytics Model Zoo
- Loaded by: YoloWorld class at initialization

**Ollama LLaVA-1.6 7B:**
- Model: `llava:7b`
- Source: Ollama Model Zoo
- Download: Manual (user runs `ollama pull llava:7b`)
- Location: Ollama cache (default ~/.ollama/models/)
- Endpoint: `http://localhost:11434/api/chat` (paho-mqtt client handles requests)
- Fallback: VisionRouter falls back to LLavaClient.describe_scene() on GPT-4o cloud errors (RESI-04)

## MQTT Broker Topology

**Broker:**
- Address: Environment variable `MQTT_BROKER`, defaults to `"helios.local"`
- Port: 1883
- Protocol: MQTT 3.1.1 (paho-mqtt v2.1 default)

**Topics (Sonar - Subscriber):**
- `helios/sonar/distance_mm` - Integer distance in millimetres (QoS 0, retained=false)
- `helios/sonar/status` - Status enum: "ok" | "weak_signal" | "out_of_range" | "offline" (QoS 1, retained=true, Last Will Testament)
- Firmware: `firmware/sonar_esp32/` (publishes both topics)

**Topics (Haptic - Publisher):**
- `helios/haptic/wrist_top` - JSON {"d": duty, "ms": ms}
- `helios/haptic/wrist_bot`
- `helios/haptic/wrist_l`
- `helios/haptic/wrist_r`
- `helios/haptic/belt_l`
- `helios/haptic/belt_c`
- `helios/haptic/belt_r`
- All QoS 0, retained=false
- Firmware: `firmware/sonar_esp32/` (subscribes to all channels)

---

*Integration audit: 2026-05-09*
