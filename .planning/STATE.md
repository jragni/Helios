# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-09)

**Core value:** A reliable 2-minute live demo of three scenes — describe / distance / grab — running on real hardware, end to end, every time. Reliability beats new features.
**Current focus:** Phase 1 — Hardware Bench

## Current Position

Phase: 1 of 6 (Hardware Bench)
Plan: 0 of TBD in current phase
Status: Ready to plan
Last activity: 2026-05-09 — Roadmap created; all 25 v1 requirements mapped to 6 phases

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: —
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: —
- Trend: —

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: 6-phase structure locked. Phase order is non-negotiable: hardware before Python, MQTT wiring before FSM, FSM before HDLR-02/03. Coarse granularity honored — 6 phases retained because the hardware-first dependency chain cannot be collapsed.
- Roadmap: PTT-01, HAPT-01, SONR-01 assigned exclusively to Phase 3 (MQTT Wiring, end-to-end integration). SONR-02 (voltage divider physical check) assigned exclusively to Phase 1 (Hardware Bench, pre-flash safety).
- Roadmap: MODE-01 split across Phase 4 (skeleton + guard conditions) and Phase 6 (full 3-scene exercise). Requirement assigned to Phase 4 as the build target; Phase 6 exercises it in the full demo loop.
- Roadmap: DEMO-03 (TTS cue dedup) assigned to Phase 5 because it is a HDLR-02 dependency (grab-guidance cue spam risk). DEMO-04 (runbook) assigned to Phase 6 as final demo integration artifact.

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 1 entry: Mosquitto not yet installed (`brew install mosquitto` needed). PlatformIO CLI not yet installed. Both must be ready before firmware flash.
- Phase 1 critical: SONR-02 voltage divider must be verified with multimeter on ALL 4 ECHO lines before any ESP32 GPIO is used. Do not skip even one sensor.
- Phase 4 critical: Intent classifier (VOIC-02) is currently untested (CONCERNS.md). Must smoke-test with ≥10 representative phrases before any demo rehearsal.
- Phase 4 critical: Firmware `haptic_esp32` and `sonar_esp32` are in design-only state (CONCERNS.md). Phase 1 is the only gate before these are depended upon.
- All phases: One-engineer sprint. Hardware debug serializes with software. Apply L-15a 15-minute swap rule aggressively.

## Session Continuity

Last session: 2026-05-09
Stopped at: Roadmap created and written to .planning/ROADMAP.md; REQUIREMENTS.md traceability updated; STATE.md initialized
Resume file: None
