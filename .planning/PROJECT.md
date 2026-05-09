# Helios

## What This Is

Helios is a wearable AI-driven accessibility system for people with visual
impairment — a USB-C webcam, vibration-motor wrist compass, and multi-zone
HC-SR04 sonar driven by a backpack laptop running OpenAI vision/voice +
local Ollama LLaVA fallback. v1 is a demo prototype: a sighted operator
wears the rig and runs three scenes — scene description, distance query,
and grab guidance — for a hackathon/expo audience.

## Core Value

A reliable 2-minute live demo of three scenes — describe / distance /
grab — running on real hardware, end to end, every time. Reliability of
the demo beats new features. If only one thing works, it must be the
3-scene loop.

## Requirements

### Validated

(None yet — ship to validate)

### Active

#### Carried-over capabilities (helios pkg already implements; need verification on USB-C cam + new transport)

- [ ] **PERC-01**: USB-C webcam frames flow through `WebcamSource` at ≥15 fps for 60 s sustained
- [ ] **PERC-02**: YOLO-World detects expanded prompt set on USB-C frames at ≥10 fps for 60 s sustained (helios baseline bench)
- [ ] **PERC-03**: MediaPipe Hands returns landmarks on USB-C frames at ≥15 fps for 60 s sustained
- [ ] **PERC-04**: Combined perception loop (`perception_loop.py`) runs YOLO + Hands at ≥10 Hz for 60 s on USB-C cam
- [ ] **VISN-01**: GPT-4o vision via `VisionRouter(use_openai=True)` returns coherent 1-sentence scene description in <3 s
- [ ] **VISN-02**: Cloud→offline fallback flips to Ollama LLaVA on 10 s OpenAI timeout, speaks "switching to offline mode" cue once
- [ ] **VOIC-01**: Whisper STT transcribes 5 s clip in <4 s
- [ ] **VOIC-02**: GPT-4o-mini intent classifier dispatches `SCENE_DESC`, `DISTANCE_QUERY`, `GRAB_GUIDANCE`; returns `UNKNOWN` for unclear input with "please repeat" cue
- [ ] **VOIC-03**: TTS-1 plays response through laptop speaker

#### Net-new for helios (not implemented in carry-over)

- [ ] **PTT-01**: Hardware push-to-talk button on ESP32 publishes `helios/button/ptt` over MQTT; voice loop subscribes and triggers record start/stop
- [ ] **HAPT-01**: 4-motor wrist compass (TOP/BOT/L/R) drives directional cues via `HapticPublisher` over MQTT; motors fire correct quadrant within 150 ms of publish
- [ ] **SONR-01**: HC-SR04 sonar (4+ units) publishes multi-zone distance over MQTT (`helios/sonar/distance_mm` for forward, plus belt L/C/R or equivalent zones); host caches latest reading via `MQTTSonarSource`
- [ ] **MODE-01**: State machine `IDLE → LOCOMOTION → MANIPULATION → IDLE` transitions correctly per scene 3 design (was Phase 4 incomplete in robotics)
- [ ] **HDLR-01**: `SCENE_DESC` handler — frame → GPT-4o vision → TTS in <3 s; uses user transcript as prompt (per L-11)
- [ ] **HDLR-02**: `DISTANCE_QUERY` handler — YOLO target → wrist haptic centers head → sonar reads → spoken distance
- [ ] **HDLR-03**: `GRAB_GUIDANCE` handler — LOCOMOTION (wrist compass to bbox) → MANIPULATION (mute belt, hand-to-object compass on wrist) → IDLE on overlap or 30 s timeout
- [ ] **DEMO-01**: 3 back-to-back full demo runs pass end-to-end on real hardware with no manual intervention
- [ ] **DEMO-02**: Panic fallback — pre-recorded video + Wizard-of-Oz keyboard override available if rig fails mid-demo

### Out of Scope

- **Real BVI user testing** — sighted-operator demo only; no claim of fitness for actual blind users (carried over from blind-assist, L-15a context)
- **Outdoor navigation, traffic safety, curb detection** — sonar is indoor only; outdoor use is unsafe without proper review
- **Multi-room mapping, GPS, path planning** — out of scope for demo prototype
- **Always-on listening / VAD** — push-to-talk sidesteps real-time audio expertise gap
- **Bone-conduction audio (Shokz) or BT speakers** — laptop speaker only (no pairing risk, judges hear what user hears)
- **Multi-user / multi-session memory** — one operator, one session
- **Mobility-aid replacement** — cane/dog still required
- **Production-grade auto-recovery** — demo includes manual panic fallback (DEMO-02), not seamless self-healing
- **Mobile/phone form factor** — backpack laptop locked
- **Soldering / through-hole-only constraint relaxed** — no rule against it; if the build needs it, do it

## Context

- **Forked from** the 2026-05-09 `blind-assist` hackathon scaffold at `~/Desktop/robotics`. CV + voice + cloud carried over verbatim with package rename `blind_assist → helios`. Sonar/haptic/firmware net-new for helios.
- **Carried perf baseline** (blind-assist bench, USB-C cam re-bench pending): combined perception loop 17.6 Hz, p95 78 ms, YOLO-World hit rate 99.9% with `yolov8m-worldv2.pt` + 21-prompt default vocabulary.
- **Pre-existing concern documentation**: `docs/LESSONS_LEARNED.md` carries 14 lessons (L-01..L-15) and 14 bugs (B-01..B-14) from the hackathon — pruned to drop ESP32-TFmini-only entries; HC-SR04 swap reasoning preserved. Watch items W-01 (LLaVA latency) and W-05 (macOS Camera permission) still apply.
- **Codebase map** at `.planning/codebase/` (mapped 2026-05-09): 7 docs, 1690 lines covering stack, integrations, architecture, structure, conventions, testing, concerns. Read these before planning each phase.
- **Hardware on hand**: USB-C cam (confirmed both indices 0 + 1 open), 3-4 ERM vibration motors, 4+ HC-SR04 sonar units, ESP32 boards. No ESP32-CAM; perception runs on USB-C cam.
- **Toolchain**: Python 3.11.14 + uv 0.9.27 (locked), ultralytics YOLO-World, MediaPipe Tasks API, OpenAI SDK ≥2.36, paho-mqtt ≥2.1, sounddevice + pynput. Mosquitto + PlatformIO not yet installed.
- **Helios is a one-engineer project** (vs blind-assist's 2-eng split). All hardware + software work on a single timeline.
- **Today is 2026-05-09**. Demo target: this week.

## Constraints

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

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fork blind-assist scaffold rather than greenfield | CV stack proven (17.6 Hz, p95 78 ms); 14 lessons already absorbed; rebuilding from scratch wastes the bench-validated path | — Pending |
| HC-SR04 sonar instead of TFmini/VL53L1X | TFmini failed bench (B-15) on prior run; HC-SR04 already worked; sonar JSON contract is sensor-agnostic per L-14 | — Pending |
| ESP32 + MQTT transport (not USB-serial direct) | Matches existing `firmware/` PlatformIO scaffold and `MQTTSonarSource` / `HapticPublisher` Python contracts; wearable-friendly (no laptop tether for sensor wires) | — Pending |
| 4-motor wrist compass (TOP/BOT/L/R) | Quadrant compass = directional cues for both DISTANCE_QUERY (head centering) and GRAB_GUIDANCE (hand→object). Matches `firmware/haptic_esp32/` 4-channel scaffold | — Pending |
| Hardware PTT button on ESP32 over MQTT | Demo realism (no keyboard reach mid-demo); keyboard substitute available as fallback | — Pending |
| Multi-zone sonar (4+ units) | Forward + belt L/C/R proximity zones — supports DISTANCE_QUERY + future belt cues | — Pending |
| Vim swap gitignore patterns dropped per user request | User explicit override; partial L-01 protection survives via `.env.*` glob; full risk re-enabled for non-`.env` files | — Pending |
| `.example` allowlists dropped from gitignore | User explicit override; `secrets.h.example` files still tracked (no rule blocks them); `.env.example` collaterally ignored via `.env.*` | — Pending |
| Run via GSD workflow | User chose `/gsd:new-project` for structure; planning docs tracked in git | ✓ Good |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-09 after initialization*
