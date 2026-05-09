# Codebase Concerns

**Analysis Date:** 2026-05-09

## Tech Debt

**No Test Suite**
- Issue: `pytest` listed in `pyproject.toml` dev dependencies but zero `.py` test files in the codebase. Codebase has ~2,800 LOC Python with no automated coverage; benches exist (`src/helios/bench/`) but do not replace unit/integration tests.
- Files affected: All Python modules under `src/helios/`
- Impact: Regressions go undetected until runtime. Hardware-dependent code paths (MQTT subscribers, webcam, Ollama) cannot be regression-tested in CI. High risk during refactoring.
- Fix approach: 
  1. Start with unit tests for core logic (vision/router.py, perception/yolo_world.py, cloud/openai_client.py) that mock external dependencies.
  2. Add integration tests for synchronous paths (e.g., LLavaClient.describe_scene with local Ollama).
  3. Use pytest fixtures and mocks to test hardware-dependent code (WebcamSource, MQTTSonarSource) without requiring hardware.

**Missing Ollama Auto-Start**
- Issue: `vision/llava_client.py` depends on `ollama serve` running locally, but the service is not auto-started by the application. If Ollama is not running, `ollama.chat()` blocks indefinitely or fails with a cryptic network error.
- Files affected: `src/helios/vision/llava_client.py` (lines 56–68), `src/helios/vision/router.py` (fallback logic)
- Impact: User must manually start `ollama serve` before any local vision path works. This is an accessibility UX blocker — visually impaired users may not discover this requirement until the app hangs.
- Fix approach:
  1. Detect Ollama availability at startup with a fast health-check (e.g., `requests.get("http://127.0.0.1:11434/api/tags", timeout=1)`).
  2. If unavailable, either:
     - Raise a clear error message: "Ollama server not running. Start it with: `ollama serve`"
     - Auto-spawn the `ollama serve` process in a background subprocess (requires macOS/Linux `brew install ollama` path).
  3. Log the startup state clearly.

**No Camera Auto-Detection**
- Issue: `src/helios/mocks/webcam_source.py` and `src/helios/perception_loop.py` hard-code `device=0` (built-in) or require user to pass `--device 1` (USB-C). Both built-in and USB cam may occupy indices 0 and 1; no detection logic determines which is which or handles missing cameras gracefully.
- Files affected: `src/helios/mocks/webcam_source.py` (lines 27–49), `src/helios/perception_loop.py` (line 137–144)
- Impact: If the user's setup has cameras at different indices (e.g., USB cam at index 2), the app fails silently with "camera permission denied" message even though permission is granted. Multi-camera setups break unless the user knows to pass the right `--device` flag.
- Fix approach:
  1. Enumerate available camera indices at startup: `for i in range(5): cap = cv2.VideoCapture(i); print(f"Device {i}: {cap.isOpened()}")`
  2. Detect which camera is USB vs. built-in (if possible via `cv2.CAP_PROP_FRAME_WIDTH` or metadata).
  3. Prefer USB camera (higher quality) if available; fall back to built-in.
  4. Log the selected device clearly.

**Cloud Fallback Timeout Too Long**
- Issue: `src/helios/vision/router.py` line 128 sets `timeout=10.0` seconds for GPT-4o vision requests. On flaky/slow networks, a 10-second block degrades UX significantly — accessibility users cannot wait that long for a scene description.
- Files affected: `src/helios/vision/router.py` (line 128)
- Impact: Any network hiccup causes a 10-second hang before the offline fallback kicks in (which also triggers a spoken cue, adding ~1 second). Total perceived latency: ~11 seconds.
- Fix approach:
  1. Lower timeout to 5–6 seconds (still allows for occasional network latency; matches OpenAI's typical response time of 2–3 seconds).
  2. Measure and log actual round-trip time; if it regularly exceeds 8 seconds, warn the user to check network quality.
  3. Consider adaptive timeout (e.g., use 90th percentile of recent latencies) if network stability is inconsistent.

## Known Bugs

**L-01: Vim Swap File with OpenAI Key Committed**
- Symptom: `.env` file was open in vim; swap file `.env.swp` and a stray `env` file got staged with `git add -A` and committed. API key bytes ended in git history.
- Root cause: Original `.gitignore` only had `/.env`, not vim swap patterns (`.*.swp`, `*.swp`).
- Fix: Commit `329045c` — amend excised the leak; `.gitignore` broadened per `docs/LESSONS_LEARNED.md` L-01 (lines 10–32). Key was rotated.
- Status: Resolved. Validate that `.gitignore` still covers `*.swp`, `.*.swp`, `*.swo`, `.*.swo` before any future `git add -A`.

**L-09: YoloWorld Thread Race (set_classes + forward)**
- Symptom: Concurrent calls to `YoloWorld.detect()` from different threads with different prompt lists cause `RuntimeError: shape '[1, 84, -1]' is invalid for input of size 312000` (B-07).
- Root cause: `set_classes(prompts)` reshapes the model head AND updates `self.no` (class count). If thread B calls `set_classes` while thread A is mid-forward, the head shape no longer matches the feature tensor.
- Fix: Commit `9d169de` — wrapped entire `(set_classes + forward)` sequence in `threading.RLock()` at `src/helios/perception/yolo_world.py` lines 37–48, 50–74.
- Status: Resolved. The lock is correctly held during both `_ensure_prompts_locked()` (line 51) and `model(frame, ...)` (line 59).

## Security Considerations

**OpenAI API Key Handling**
- Risk: API key leakage via:
  1. Vim swap file committed (L-01 — already happened once).
  2. Frame captures + voice transcripts passed to OpenAI in the clear (PII risk).
  3. Environment variable typos (`OPEN_API_KEY` instead of `OPENAI_API_KEY`) silently fail with cryptic error (L-02, L-13).
- Files affected: `.env` (sensitive), `src/helios/cloud/openai_client.py` (lines 19–35), `src/helios/vision/router.py` (lines 59–134), `src/helios/cloud/whisper.py`, `src/helios/cloud/tts.py`
- Current mitigation:
  - `.gitignore` broadened to cover `/.env`, `.env.*`, `*.pem`, `*.key`, secrets (lines 13–24 of `.gitignore`).
  - `get_client()` raises `RuntimeError(KEY_MISSING_MSG)` with the exact variable name on startup (D-03).
  - Key was rotated after L-01 incident.
- Recommendations:
  1. Keep the canonical error string `OPENAI_API_KEY not set. Add it to .env at repo root.` (line 19) in every module that calls `get_client()`.
  2. Document that voice transcripts and frame captures sent to OpenAI are retained per OpenAI's data retention policy; users must opt-in or understand the privacy surface.
  3. Add a `--offline-only` flag to `ptt_loop.py` that disables cloud vision + TTS and forces Ollama + local TTS fallback.
  4. Audit `.gitignore` before any `git add -A` (prefer explicit `git add path/a path/b`).

**PII in Voice & Frame Captures**
- Risk: User's audio (voice queries) and camera frames (visual environment) pass through OpenAI's API. OpenAI retains this data per their policy; accessibility use case means frames may contain faces, documents, personal spaces.
- Files affected: `src/helios/cloud/whisper.py` (audio), `src/helios/vision/router.py` (frames), `src/helios/cloud/tts.py` (cached audio on disk)
- Current mitigation: `.gitignore` excludes local cache of captured frames and audio (`cache/`, `audio/`, `frames/`, `*.wav`, `*.mp3`, lines 26–48 of `.gitignore`).
- Recommendations:
  1. Add a privacy disclosure in `README.md` or startup: "Voice and video data are sent to OpenAI for processing. See OpenAI's privacy policy."
  2. Implement local-only mode (`--mock` or `--offline-only` flag) so blind users can opt out of cloud processing.
  3. Do not log or store user transcripts on disk by default; if logging is needed for debugging, require explicit `--log-transcripts` flag.
  4. Clean up TTS cache periodically or on user request (`rm -rf src/helios/cache/tts/`).

**Gitignore Narrower Than Original**
- Risk: User request removed vim swap patterns from `.gitignore` after L-01 incident. Current `.gitignore` includes them again (lines 25–27), but if future edits remove them again, risk re-introduced.
- Files affected: `.gitignore` (lines 1–83)
- Current mitigation: `.gitignore` explicitly includes `*.swp`, `*.swo`, `.*.swp`, `.*.swo` (implicit in vim pattern matching).
- Recommendations:
  1. Add comments near sensitive patterns (e.g., "# --- Secrets — never relax") to signal importance.
  2. In CI/CD, add a pre-commit hook that rejects commits with `.env` or `.swp` files.

## Performance Bottlenecks

**LLaVA-1.6 7B Latency Exceeds 3 s Target (W-01)**
- Problem: Median latency 3.5 s for `llava:7b` on macOS (documented in `docs/LESSONS_LEARNED.md` W-01, line 229). Target SCENE_DESC response time is <3 s.
- Files affected: `src/helios/vision/llava_client.py` (lines 50–68), `src/helios/bench/llava.py` (benchmarks)
- Cause: Model size (7B parameters) + inference cost on CPU/GPU; Metal acceleration (macOS GPU) has known issues (L-05, resolved in Ollama 0.23.2+).
- Impact: Users experience 3.5+ second latency for every "describe scene" request. For accessibility, this is a long silence and poor UX.
- Improvement path:
  1. Use a smaller model variant (e.g., `llava:3.8b` or `llava-phi:2.7b`) — trade accuracy for speed.
  2. Benchmark on target hardware (M1/M2/M3 Mac) and publish latency targets.
  3. Make LLaVA the fallback-only path; prioritize cloud GPT-4o (typically 1.5–2 s).
  4. Cache scene descriptions for identical/similar frames to avoid re-inference.

**YOLO-World Class Projection Lock Contention**
- Problem: `threading.RLock()` in `src/helios/perception/yolo_world.py` (lines 37–48) serializes all `detect()` calls when prompts change. Protects against race but may reduce parallelism if multiple threads request detections with different prompts.
- Files affected: `src/helios/perception/yolo_world.py` (lines 37–74)
- Cause: YOLO-World's `set_classes()` mutates global model state; no way to run multiple detections with different prompt lists in parallel.
- Impact: If the perception loop and the vision dispatch handler both call `detect()` concurrently with different prompts, one blocks waiting for the lock. Latency increases if contention is high.
- Improvement path:
  1. Measure lock contention in production (add `logging.debug()` before/after lock acquisition).
  2. If contention is low, accept current design (lock is correct and safe).
  3. If contention is high, refactor to have a single YOLO thread that queues detection requests and caches results (eliminates concurrent `detect()` calls).

## Fragile Areas

**Firmware Not Flashed/Tested (sonar_esp32, haptic_esp32)**
- Files affected: `firmware/sonar_esp32/src/main.cpp`, `firmware/haptic_esp32/src/main.cpp`
- Why fragile: Both ESP32 firmware projects are in design-only state — no hardware flashed, no integration tested. Code compiles (presumably) but runtime behavior unknown. The Python side (`src/helios/perception/mqtt_sonar_source.py`, `src/helios/output/haptic.py`) depends on these services being available.
- Safe modification:
  1. Before editing firmware, flash a working build to hardware and verify MQTT topics/payloads match the Python contract.
  2. Add unit tests to the firmware (e.g., Arduino unit test framework) if feasible.
  3. Document the exact ESP32 board variant, baud rate, and GPIO pinout required (avoid L-14 re-occurrence).
- Test coverage: None. High risk of integration failures when hardware is first deployed.

**MQTT Broker Dependency Untested**
- Files affected: `src/helios/perception/mqtt_sonar_source.py` (all methods), `src/helios/output/haptic.py` (publish calls)
- Why fragile: Code assumes an MQTT broker is running at `MQTT_BROKER` env var (default `helios.local`). No broker = silent failure (connection timeout after 60 s keepalive). Python code has no health check before using MQTT.
- Safe modification:
  1. Add a startup health check: attempt to connect with a 5-second timeout and fail loudly if broker is unreachable.
  2. Provide a `MockMQTTSonarSource` (already exists at line 45–56) as a drop-in for testing.
  3. Log broker connection state at startup.
- Test coverage: MQTT code must be tested with a real broker (e.g., Docker `mosquitto`) or a mock (paho provides a mock server for testing).

**Webcam Permission Tied to Shell Launch (L-06, W-05)**
- Files affected: `src/helios/mocks/webcam_source.py` (lines 38–49), `src/helios/perception_loop.py` (lines 137–156)
- Why fragile: macOS TCC (Transparency, Consent & Control) permission is granted *per launching app*. If user launches from Terminal.app, they grant permission to Terminal. If they later launch from iTerm or VSCode, it re-prompts. `cv2.VideoCapture(0)` silently returns `isOpened()=False` on permission denial with no exception.
- Safe modification:
  1. Wrap the first `cv2.VideoCapture(device)` call in explicit error handling that detects permission denial vs. hardware missing.
  2. Log the exact permission error and the exact remediation (already done at lines 45–49).
  3. Test on fresh macOS with revoked camera permission to ensure error message is clear.
- Test coverage: Cannot test macOS TCC remotely; must test locally on a machine with revoked camera permission.

**ESP32 Firmware Hard-coded GPIO Pins**
- Files affected: `firmware/sonar_esp32/src/main.cpp` (lines 28–29), `firmware/haptic_esp32/src/main.cpp` (lines 33–38)
- Why fragile: Sonar: TRIG=14, ECHO=15 (with level shifter divider). Haptic: wrist_top=25, wrist_bot=26, wrist_l=27, wrist_r=33. If any wire is swapped or board variant uses different pins, firmware must be recompiled. No pin conflict detection.
- Safe modification:
  1. Document the exact ESP32 board variant (e.g., "NodeMCU-32S" or "ESP32-WROOM-32D") and pin assignments in a hardware.md file.
  2. Add `#ifdef` guards for different board variants if multiple boards are in use.
  3. Verify GPIO availability with a multimeter before flashing.

## Scaling Limits

**Cloud Vision Timeout vs. Flaky Networks**
- Current capacity: 10-second timeout for GPT-4o; offline fallback triggers after 1 failure.
- Limit: Networks with >20% packet loss or latency spikes >8 seconds will cause frequent fallbacks and poor UX.
- Scaling path:
  1. Implement exponential backoff: if 3 consecutive requests timeout, pause cloud for 30 seconds before retrying.
  2. Add circuit-breaker pattern: if error rate >50% in the last 60 seconds, automatically switch to offline mode until network stabilizes.

**TTS API Call Volume (L-15)**
- Current capacity: Each grab-guidance cue ("left and up") is one TTS call. 30-second session = ~18 calls. Cost: ~0.018 USD per session; stacked sessions accumulate.
- Limit: If running 10 concurrent user sessions, cost becomes ~0.18 USD per 30 seconds.
- Scaling path:
  1. TTS caching is implemented for repeated phrases (line 29–51 of `src/helios/cloud/tts.py`).
  2. Add dedup logic: track last N spoken phrases and skip if within the last 30 seconds.
  3. Implement `--mock` flag (mentioned in L-15) to force LLaVA-only path and avoid TTS entirely.

**Ollama Model Download & Memory**
- Current capacity: `llava:7b` weights ~4 GB on disk; Ollama downloads on-demand.
- Limit: Devices with <8 GB RAM or <10 GB disk will fail silently if `ollama serve` tries to load `llava:7b`.
- Scaling path:
  1. Check available disk/RAM before attempting to download weights.
  2. Document minimum hardware requirements (8 GB RAM, 10 GB free disk recommended).
  3. Provide alternative smaller models in a config file (`ollama.model: "llava:3.8b"`).

## Dependencies at Risk

**OpenAI API Breaking Changes**
- Risk: OpenAI's API is stable but occasionally introduces new endpoints or deprecates old ones. Vision API (`gpt-4o` with image_url) is relatively new (2024); no guarantee of long-term stability.
- Files affected: `src/helios/vision/router.py` (lines 104–134), `src/helios/cloud/whisper.py`, `src/helios/cloud/tts.py`
- Impact: If OpenAI deprecates the vision API, all cloud fallback paths fail.
- Migration plan:
  1. Monitor OpenAI's API changelog for deprecation notices.
  2. Implement an abstract `VisionProvider` interface so switching to Claude, Gemini, or another provider is possible.
  3. Keep LLaVA as a free fallback; ensure it can run fully offline.

**Ollama Model Availability**
- Risk: `llava:7b` is maintained by the Ollama community. If it's abandoned or moved, the model no longer auto-downloads.
- Files affected: `src/helios/vision/llava_client.py` (line 26: hardcoded `llava:7b`), `src/helios/bench/llava.py`
- Impact: New deployments fail to download the model; existing deployments have the model cached and continue to work.
- Migration plan:
  1. Pin the model version in code (e.g., `llava:7b-v1.5` if versioning becomes available).
  2. Implement a fallback model selection: if `llava:7b` is unavailable, try `llava-phi:2.7b` (smaller, faster).
  3. Document how to manually download and import alternative models.

**MediaPipe API Instability (L-07)**
- Risk: MediaPipe removed `solutions.hands` API in version 0.10.35 (resolved by switching to Tasks API at commit `1d3fc25`). Future releases may introduce more breaking changes.
- Files affected: `src/helios/perception/hands.py` (uses Tasks API), `pyproject.toml` (line 13: `mediapipe>=0.10.14`)
- Impact: Next major version of MediaPipe may require code changes.
- Migration plan:
  1. Pin MediaPipe to a known stable version (e.g., `mediapipe==0.10.14`) rather than `>=0.10.14`.
  2. Write integration tests for hand detection so version changes are caught in CI.
  3. Monitor MediaPipe releases and test new versions in a sandbox before upgrading.

**ultralytics YOLO-World Undeclared Dependencies (L-08)**
- Risk: ultralytics doesn't declare `openai-clip` as a hard dependency; it's imported dynamically inside `set_classes()`. Future versions may add or change dependencies.
- Files affected: `pyproject.toml` (line 17: `openai-clip>=1.0.1` — added explicitly), `src/helios/perception/yolo_world.py`
- Impact: Dependency upgrades may fail silently if transitive dependencies are not declared.
- Migration plan:
  1. Keep `openai-clip` explicitly in `pyproject.toml`.
  2. Write a smoke test that calls `YoloWorld().detect()` with prompts to ensure CLIP is available.
  3. Monitor ultralytics releases for breaking changes.

## Missing Critical Features

**No Distance Integration (LAPT-07)**
- Problem: Distance is hardcoded to 100 cm placeholder in `src/helios/perception_loop.py` line 35 (and bench version line 55 of `src/helios/bench/perception_loop.py`). Phase 2/3 was to wire `/tof` HTTP poll but it's not implemented.
- Blocks: GrabGuide cannot provide accurate "left 15 cm, up 30 cm" directions without real distance data.
- Fix approach:
  1. Implement `GET /distance` endpoint on the ESP32 (or use MQTT `helios/sonar/distance_mm` which is already wired in `mqtt_sonar_source.py`).
  2. Wire `perception_loop()` to use a real `DistanceSource` instead of placeholder.
  3. Add integration test that verifies distance readings are used in GrabGuide output.

**GrabGuide & DISTANCE_QUERY Stubs (D-22)**
- Problem: `src/helios/voice/ptt_loop.py` lines 89–91 (GRAB_GUIDANCE) and 84–86 (DISTANCE_QUERY) both stub with "not wired yet — Phase 4" messages.
- Blocks: Cannot demo full accessibility workflow (grab/guidance).
- Fix approach:
  1. DISTANCE_QUERY: Route to a handler that queries real distance and returns "Object at 45 cm, slightly to the right."
  2. GrabGuide: Integrate with perception_loop's YOLO results to compute hand position delta and emit "left 10 cm, up 5 cm" cues.

## Test Coverage Gaps

**Vision Router (Fallback Logic) Untested**
- What's not tested: The fallback path in `src/helios/vision/router.py` (lines 104–149) — timeout handling, offline window transitions (D-27, D-28), and TTS cue playback.
- Files: `src/helios/vision/router.py`
- Risk: If the offline window logic breaks, users won't know until cloud fails in production (e.g., `_offline_until` is not reset correctly, causing infinite offline mode).
- Priority: High — fallback logic is critical for UX.

**YOLO-World Thread Safety Untested**
- What's not tested: The `threading.RLock()` at `src/helios/perception/yolo_world.py` prevents races, but there are no concurrent test cases that verify it actually works.
- Files: `src/helios/perception/yolo_world.py`
- Risk: If the lock is removed by mistake or a new method bypasses it, the race condition re-appears silently.
- Priority: High — the bug (B-07) was hard to diagnose.

**Perception Loop Edge Cases Untested**
- What's not tested: Frame drops, camera disconnection mid-loop, latency spikes, YOLO batch processing with zero detections (lines 64–65 of `src/helios/perception/yolo_world.py`).
- Files: `src/helios/perception_loop.py`, `src/helios/perception/yolo_world.py`, `src/helios/mocks/webcam_source.py`
- Risk: Loop hangs, crashes, or silently stops emitting frames.
- Priority: Medium.

**Intent Classifier Untested**
- What's not tested: Few-shot examples, intent classification correctness, edge cases (silence, noise, out-of-distribution queries).
- Files: `src/helios/cloud/intent.py`
- Risk: Intent misclassification causes wrong handler dispatch (e.g., "what do you see?" classified as GRAB_GUIDANCE instead of SCENE_DESC).
- Priority: High — affects user experience directly.

**Whisper Transcription Untested**
- What's not tested: Transcription accuracy, empty audio handling, noise robustness.
- Files: `src/helios/cloud/whisper.py`
- Risk: If Whisper transcription is incorrect, downstream handlers receive wrong input.
- Priority: Medium — dependent on OpenAI's model quality, but error handling can be tested.

## Safety Concerns (Accessibility UX)

**No Blind-User Testing**
- Issue: Helios is an accessibility system for visually impaired users, but no blind users have tested the UX. Design was reviewed by accessibility researchers but not validated with target users.
- Impact: Accessibility assumptions may be wrong (e.g., speech rate, audio cue clarity, latency tolerance). UX may exclude users (e.g., no keyboard-only mode, reliance on visual setup).
- Mitigation:
  1. Recruit blind users for early testing (even informal usability sessions).
  2. Collect feedback on:
     - Speech rate and clarity (TTS voice + speed).
     - Latency tolerance (how long can users wait for "describe scene"?).
     - Audio cue distinctness (is "left and up" clear without visual reference?).
     - Keyboard accessibility (can users navigate without a mouse?).
  3. Document findings and iterate on design.

**Latency Sensitivity Not Validated**
- Issue: Accessibility users are latency-sensitive — a 3-second delay between button press and audio response feels like a hang. Helios targets <3 s for SCENE_DESC but uses a 3.5 s LLaVA fallback (W-01).
- Impact: Users may give up on the app if response times are perceived as broken.
- Mitigation:
  1. Measure end-to-end latency (button press → TTS playback) in user testing.
  2. Set a hard <2 s target for critical paths (SCENE_DESC, GrabGuide).
  3. Add latency telemetry (log round-trip times) so you can diagnose slowdowns in the field.

**No Fallback for No-Cloud Scenario**
- Issue: If OpenAI key is missing or network is down, the app falls back to LLaVA. But LLaVA requires Ollama running locally — if it's not, the app crashes or hangs silently.
- Impact: Users in low-connectivity environments (rural, no WiFi) cannot use the app if they don't have Ollama set up.
- Mitigation:
  1. Add startup validation: check Ollama health before starting the PTT loop.
  2. Offer a `--offline-only` mode that assumes Ollama is available and disables cloud paths entirely.
  3. Document the offline setup clearly (installation, model download time, hardware requirements).

---

*Concerns audit: 2026-05-09*
*Cross-references to docs/LESSONS_LEARNED.md: L-01, L-05, L-06, L-07, L-08, L-09, L-11, L-12, L-13, L-14, L-15; B-01, B-07, B-14; W-01, W-05; RESI-04, D-03, D-05, D-07, D-08, D-11, D-12, D-13, D-14, D-15, D-16, D-18, D-19, D-20, D-21, D-22, D-26, D-27, D-28; LAPT-07, VOIC-04, VOIC-05*
