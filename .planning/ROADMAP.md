# Roadmap: Helios

## Overview

Six phases deliver a reliable 2-minute 3-scene accessibility demo on real hardware. The order
is non-negotiable: hardware bench before Python integration, MQTT wiring before state machine,
state machine before HDLR-02/03. Each phase gates the next. Phase 1 establishes that every
ESP32 board produces correct MQTT traffic before a single Python consumer is written. Phase 2
verifies the carried-over perception stack on the USB-C cam. Phase 3 wires all MQTT channels
into Python. Phases 4–6 build and integrate the three demo scenes in dependency order, ending
with 3 back-to-back passing runs and a tested panic fallback.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Hardware Bench** - Flash ESP32 firmware, verify sonar + haptic + PTT produce correct MQTT traffic, wiring safety checks
- [ ] **Phase 2: Perception Verification** - Re-bench carried-over CV stack on USB-C cam; camera auto-detection; stale-frame drain; SharedPerceptionState
- [ ] **Phase 3: MQTT Wiring** - Integrate sonar, haptic, and PTT MQTT channels into Python; end-to-end broker path verified
- [ ] **Phase 4: Scene 1 + Voice Core + Panic Fallback** - Intent classifier, HDLR-01 scene description, VisionRouter hardening, state machine skeleton, DEMO-02 panic fallback
- [ ] **Phase 5: Scene 2 — Distance Query** - HDLR-02 distance query handler: YOLO target → wrist haptic centering → sonar → spoken distance
- [ ] **Phase 6: Scene 3 + Demo Integration** - HDLR-03 grab guidance, full IDLE→LOCOMOTION→MANIPULATION→IDLE exercise, 3-run end-to-end rehearsal, demo runbook

## Phase Details

### Phase 1: Hardware Bench
**Goal**: Every ESP32 board is flashed, wired safely, and produces correct MQTT traffic before any Python integration begins
**Depends on**: Nothing (first phase)
**Requirements**: SONR-02
**Success Criteria** (what must be TRUE):
  1. All 4 sonar ECHO lines have voltage divider verified with multimeter before any firmware flash (5V→3.3V; no GPIO at risk)
  2. Sonar ESP32 publishes integer millimeter readings to `helios/sonar/distance_mm` at ~10 Hz for 30 s sustained; `mosquitto_sub` shows no cross-talk spikes (no zone dropping to 2–5 cm when unobstructed)
  3. Haptic ESP32 responds to `helios/haptic/<channel>` JSON commands; each of the 4 wrist motors fires the correct quadrant within 150 ms of publish (verified by observation + MQTT log timestamps)
  4. PTT ESP32 publishes exactly one `helios/button/ptt` message per physical press in a 20-press button-mash bench (firmware debounce verified; no double-fires)
**Plans**: TBD

### Phase 2: Perception Verification
**Goal**: The carried-over CV pipeline runs at spec on the USB-C cam with reliable camera selection, fresh frames, and shared perception state available to handlers
**Depends on**: Phase 1
**Requirements**: PERC-01, PERC-02, PERC-03, PERC-04, PERC-05, CAM-01, CAM-02
**Success Criteria** (what must be TRUE):
  1. USB-C cam is auto-detected by name/property (not hard-coded index); startup logs correct device; fails loud if USB-C cam missing
  2. Combined perception loop (YOLO-World + MediaPipe Hands) sustains ≥10 Hz for 60 s on USB-C cam; p95 frame age <100 ms under 10 Hz consumer (stale-frame drain active)
  3. YOLO-World detects the 21-prompt default vocabulary on USB-C frames at ≥10 fps; MediaPipe Hands returns landmarks at ≥15 fps — both verified by bench output
  4. `SharedPerceptionState` exposes latest YOLO bboxes and hand landmarks to a reader without re-running inference; concurrent read during perception loop produces no race errors
**Plans**: TBD

### Phase 3: MQTT Wiring
**Goal**: All three hardware channels (sonar, haptic, PTT) are end-to-end verified through the Python host — broker running, topics match, Python consumers respond correctly
**Depends on**: Phase 1, Phase 2
**Requirements**: PTT-01, HAPT-01, SONR-01
**Success Criteria** (what must be TRUE):
  1. `MQTTSonarSource` caches zone readings from live ESP32 firmware; `get_distance_mm()` returns a fresh reading (stale threshold: <500 ms) during a 60 s wear test
  2. `HapticPublisher.pulse(channel, duty, ms)` drives the correct wrist motor within 150 ms end-to-end (Python publish → broker → ESP32 → motor) verified by timestamped log
  3. `MQTTButtonSource` converts `helios/button/ptt` MQTT message into the same `threading.Event` as the keyboard PTT fallback; voice loop triggers record start/stop correctly from hardware button press
  4. PTT round-trip from button press to Python `start_recording()` callback measured at <200 ms on laptop hotspot; QoS 1 used for PTT topic; broker restart tested (subscriptions survive via `on_connect()`)
**Plans**: TBD

### Phase 4: Scene 1 + Voice Core + Panic Fallback
**Goal**: Scene 1 (describe scene) works end-to-end from hardware PTT button; the voice pipeline dispatches all three intents correctly; VisionRouter is hardened; state machine skeleton exists; panic fallback is ready before demo rehearsal
**Depends on**: Phase 3
**Requirements**: VISN-01, VISN-02, VISN-03, VISN-04, VOIC-01, VOIC-02, VOIC-03, MODE-01, HDLR-01, DEMO-02
**Success Criteria** (what must be TRUE):
  1. Pressing the hardware PTT button, speaking "describe the scene", and releasing triggers Whisper STT → intent classifier → GPT-4o vision → spoken scene description in <3 s on nominal WiFi
  2. GPT-4o-mini intent classifier correctly dispatches `SCENE_DESC`, `DISTANCE_QUERY`, `GRAB_GUIDANCE` for natural phrasing; returns `UNKNOWN` with "please repeat" cue for out-of-scope utterances; smoke-tested with at least 10 representative phrases
  3. VisionRouter falls back to Ollama on 10 s OpenAI timeout; speaks "switching to offline mode" once per outage window; Ollama startup health check (`localhost:11434`) fails loud if daemon not running; OpenAI client configured with explicit `timeout=` kwarg (connection-level hang caught)
  4. `ModeStateMachine` transitions IDLE → LOCOMOTION → MANIPULATION → IDLE with guard conditions and 30 s timeout; `reset_demo()` returns to clean IDLE; state machine verified before Phase 5 begins
  5. DEMO-02 panic fallback is ready: pre-recorded video is queued, Wizard-of-Oz keyboard override hotkey is tested, operator knows the activation sequence
**Plans**: TBD

### Phase 5: Scene 2 — Distance Query
**Goal**: Scene 2 (distance query) works end-to-end: named object is located by YOLO, wrist haptic centers operator's head, sonar reads distance, result is spoken
**Depends on**: Phase 4
**Requirements**: HDLR-02, DEMO-03
**Success Criteria** (what must be TRUE):
  1. Asking "how far is the [object]?" locates the target bounding box via YOLO-World, fires the correct wrist haptic quadrant to center the operator's view, then speaks the sonar distance (e.g., "45 centimeters") — all within one PTT press-and-release cycle
  2. TTS directional cue dedup is active: repeated compass phrases ("left", "right", "up", "down") play from WAV cache without triggering additional OpenAI TTS API calls; cache warm pass runs before demo
  3. If YOLO cannot find the named target, the handler speaks a clear "I can't find [object]" cue rather than hanging or throwing an exception
**Plans**: TBD

### Phase 6: Scene 3 + Demo Integration
**Goal**: Scene 3 (grab guidance) works end-to-end through the full LOCOMOTION→MANIPULATION state machine; 3 back-to-back demo runs pass on real hardware with no manual intervention; demo runbook is complete
**Depends on**: Phase 5
**Requirements**: HDLR-03, DEMO-01, DEMO-04
**Success Criteria** (what must be TRUE):
  1. Asking "help me grab the [object]" transitions the state machine to LOCOMOTION, fires wrist compass cues toward the bbox centroid each frame, transitions to MANIPULATION on hand overlap (belt sonar muted, wrist compass shifts to hand→object vector), and returns to IDLE on grip overlap or 30 s timeout
  2. `reset_demo()` between runs clears state machine to IDLE, resets YOLO target lock, clears TTS dedup cache, zeros haptic motor state — confirmed by starting run 2 immediately after run 1 ends with no residual state
  3. 3 consecutive full demo runs (describe → distance → grab) pass on real hardware with no manual intervention and no manual reset required between runs
  4. `docs/DEMO_RUNBOOK.md` exists and covers: start sequence, kill sequence, device index verification, MQTT broker health check, Ollama warmup, panic fallback trigger, and a common-failure quick-fix table

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Hardware Bench | 0/TBD | Not started | - |
| 2. Perception Verification | 0/TBD | Not started | - |
| 3. MQTT Wiring | 0/TBD | Not started | - |
| 4. Scene 1 + Voice Core + Panic Fallback | 0/TBD | Not started | - |
| 5. Scene 2 — Distance Query | 0/TBD | Not started | - |
| 6. Scene 3 + Demo Integration | 0/TBD | Not started | - |
