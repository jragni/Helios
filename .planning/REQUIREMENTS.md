# Requirements: Helios

**Defined:** 2026-05-09
**Core Value:** A reliable 2-minute live demo of three scenes — describe / distance / grab — running on real hardware, end to end, every time.

## v1 Requirements

Requirements for initial demo. Each maps to roadmap phases.

### Perception (carry-over verification on USB-C hardware)

- [ ] **PERC-01**: USB-C webcam frames flow through `WebcamSource` at ≥15 fps for 60 s sustained
- [ ] **PERC-02**: YOLO-World detects expanded prompt set on USB-C frames at ≥10 fps for 60 s sustained (helios baseline bench)
- [ ] **PERC-03**: MediaPipe Hands returns landmarks on USB-C frames at ≥15 fps for 60 s sustained
- [ ] **PERC-04**: Combined perception loop runs YOLO + Hands at ≥10 Hz for 60 s on USB-C cam

### Vision (cloud + offline fallback)

- [ ] **VISN-01**: GPT-4o vision via `VisionRouter(use_openai=True)` returns coherent 1-sentence scene description in <3 s
- [ ] **VISN-02**: Cloud→offline fallback flips to Ollama LLaVA on 10 s OpenAI timeout, speaks "switching to offline mode" cue once, retries cloud after 60 s window
- [ ] **VISN-03**: OpenAI client has explicit `timeout=` kwarg to catch connection-level hangs (not just API errors) — ARCHITECTURE.md gap
- [ ] **VISN-04**: Ollama startup health check on `VisionRouter.__init__` — fail loud if `localhost:11434` unreachable (avoids silent fallback hang)

### Voice

- [ ] **VOIC-01**: Whisper STT transcribes 5 s clip in <4 s
- [ ] **VOIC-02**: GPT-4o-mini intent classifier dispatches `SCENE_DESC`, `DISTANCE_QUERY`, `GRAB_GUIDANCE`; returns `UNKNOWN` for unclear input with "please repeat" cue
- [ ] **VOIC-03**: TTS-1 plays response through laptop speaker

### Hardware transport (net-new)

- [ ] **PTT-01**: Hardware push-to-talk button on ESP32 publishes `helios/button/ptt` over MQTT; firmware-side debounce (≥20 ms) prevents double-trigger; voice loop subscribes via `MQTTButtonSource` and triggers record start/stop
- [ ] **HAPT-01**: 4-motor wrist compass (TOP/BOT/L/R) drives directional cues via `HapticPublisher` over MQTT; motors fire correct quadrant within 150 ms of publish on real hardware
- [ ] **SONR-01**: HC-SR04 sonar (4+ units) publishes multi-zone distance over MQTT (`helios/sonar/distance_mm` for forward, plus belt L/C/R or equivalent zones); host caches latest reading via `MQTTSonarSource`; firmware enforces ≥60 ms inter-trigger interval to prevent cross-talk
- [ ] **SONR-02**: All 4 sonar ECHO lines have 1 kΩ + 2 kΩ voltage divider verified before flash (5V→3.3V level-shift required for ESP32 GPIO survival)
- [ ] **CAM-01**: USB-C cam device index detection — auto-prefer external webcam over FaceTime cam by name/property check; fail loud if expected device missing (OpenCV index instability)
- [ ] **CAM-02**: Stale-frame drain thread on `WebcamSource` — single-slot latest-frame already implemented; verify under 10 Hz consumer that p95 frame age <100 ms

### State machine + handlers (net-new)

- [ ] **MODE-01**: State machine `IDLE → LOCOMOTION → MANIPULATION → IDLE` transitions correctly per scene 3 design; `reset_demo()` returns to clean IDLE between runs
- [ ] **PERC-05**: `SharedPerceptionState` exposes latest YOLO bboxes + hands to handlers without re-running YOLO (avoids duplicate inference + L-09 race)
- [ ] **HDLR-01**: `SCENE_DESC` handler — frame → GPT-4o vision → TTS in <3 s; uses user transcript as prompt (per L-11)
- [ ] **HDLR-02**: `DISTANCE_QUERY` handler — YOLO target → wrist haptic centers head → sonar reads → spoken distance
- [ ] **HDLR-03**: `GRAB_GUIDANCE` handler — LOCOMOTION (wrist compass to bbox) → MANIPULATION (mute belt, hand-to-object compass on wrist) → IDLE on overlap or 30 s timeout

### Demo

- [ ] **DEMO-01**: 3 back-to-back full demo runs pass end-to-end on real hardware with no manual intervention
- [ ] **DEMO-02**: Panic fallback — pre-recorded video + Wizard-of-Oz keyboard override available if rig fails mid-demo (build early per FEATURES.md guidance)
- [ ] **DEMO-03**: TTS cue dedup + WAV cache for repeat phrases ("left", "right", "up", "down") — prevents OpenAI cost runaway under stacked grab-guidance sessions (L-15)
- [ ] **DEMO-04**: Demo runbook (`docs/DEMO_RUNBOOK.md`) — start sequence, kill sequence, panic fallback trigger, common-failure quick-fix table

## v2 Requirements

Deferred. Tracked but not in current roadmap.

### Vision

- **VISN-V2-01**: Swap Ollama LLaVA-1.6 7B → `qwen2.5-vl:7b` (research STACK.md recommends; better 7B benchmark, drop-in via `LLavaClient` model string)
- **VISN-V2-02**: Swap TTS-1 → `gpt-4o-mini-tts` (current OpenAI primary recommendation, comparable latency)

### Hardware

- **HW-V2-01**: Belt zone proximity (L/C/R) with hysteresis + EMA filter — full belt loop from blind-assist Phase 2 BELT-* requirements

### Demo

- **DEMO-V2-01**: Heartbeat watchdog with voice alerts on wrist/belt/sonar disconnect (graceful degradation alerts)

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Real BVI user testing | Sighted-operator demo only; no claim of fitness for blind users |
| Outdoor navigation, traffic safety, curb detection | Sonar is indoor only; outdoor unsafe without proper review |
| Multi-room mapping, GPS, path planning | Out of scope for demo prototype |
| Always-on listening / VAD | Push-to-talk sidesteps real-time audio expertise gap |
| Bone-conduction audio (Shokz) or BT speakers | Laptop speaker only — no pairing risk; judges hear what user hears |
| Multi-user / multi-session memory | One operator, one session |
| Mobility-aid replacement | Cane/dog still required |
| Mobile/phone form factor | Backpack laptop locked |
| Production-grade auto-recovery | Manual panic fallback (DEMO-02), not seamless self-healing |
| OCR / text reading | Separate pipeline; doesn't contribute to 3-scene loop |
| Face recognition | Separate pipeline + privacy concerns; not in 3-scene loop |
| Unit test suite (full coverage) | Smoke tests in `__main__` blocks suffice for demo scope; pytest scaffolded but not populated |

## Traceability

Filled by roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CAM-01, CAM-02 | Phase 2 | Pending |
| PERC-01, PERC-02, PERC-03, PERC-04, PERC-05 | Phase 2 | Pending |
| VISN-01, VISN-02, VISN-03, VISN-04 | Phase 4 | Pending |
| VOIC-01, VOIC-02, VOIC-03 | Phase 4 | Pending |
| PTT-01 | Phase 1, Phase 3 | Pending |
| HAPT-01 | Phase 1, Phase 3 | Pending |
| SONR-01, SONR-02 | Phase 1, Phase 3 | Pending |
| MODE-01 | Phase 4 | Pending |
| HDLR-01 | Phase 4 | Pending |
| HDLR-02 | Phase 5 | Pending |
| HDLR-03 | Phase 6 | Pending |
| DEMO-01 | Phase 6 | Pending |
| DEMO-02 | Phase 4 | Pending |
| DEMO-03 | Phase 6 | Pending |
| DEMO-04 | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 25 total
- Mapped to phases: 25
- Unmapped: 0 ✓

---
*Requirements defined: 2026-05-09*
*Last updated: 2026-05-09 after initial definition*
