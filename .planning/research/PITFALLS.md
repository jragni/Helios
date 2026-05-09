# Pitfalls Research

**Domain:** Wearable AI accessibility demo — multi-zone sonar, USB-C webcam, hardware PTT, one-engineer sprint
**Researched:** 2026-05-09
**Confidence:** HIGH (hardware pitfalls from datasheets + community post-mortems; MQTT/firmware from issue trackers; camera pitfalls from OpenCV issue tracker)

> **Scope note:** This file documents NEW pitfalls specific to helios scope changes.
> Lessons L-01 through L-15 and bugs B-01 through B-14 in `docs/LESSONS_LEARNED.md` are
> validated below under "Prior Lessons Scope Validation" — do NOT re-document them here.
> CONCERNS.md items carry forward as-is.

---

## Prior Lessons Scope Validation

The following L-XX / B-XX / W-XX entries from blind-assist remain **fully applicable** to helios
and need no modification:

| ID | Applies to helios? | Notes |
|----|--------------------|-------|
| L-01 | YES | Same git workflow, same `.env` risk. Gitignore scope drop (user override) makes this MORE relevant. |
| L-02 | YES | macOS BSD `timeout` absence unchanged. |
| L-03 | YES | ESP32 serial monitoring via pyserial pattern still needed for firmware debug. |
| L-04 | YES | `uv run python` mandate unchanged. |
| L-05 | YES | Ollama Metal shader risk persists on macOS 25.3 if Ollama regresses; verify Ollama version at demo setup. |
| L-06 | YES | macOS TCC Camera permission re-applies to USB-C webcam (same API path, new device). |
| L-07 | YES | MediaPipe Tasks API still required; no API change. |
| L-08 | YES | CLIP dependency still undeclared by ultralytics. |
| L-09 | YES | YOLO-World thread race still exists; lock pattern preserved in helios fork. |
| L-10 | YES | CLIP-twins + discriminative prompts still matter for YOLO on USB-C cam. |
| L-11 | YES | SCENE_DESC transcript-as-prompt pattern still required. |
| L-12 | YES | Intent classifier few-shot breadth still required. |
| L-13 | YES | Canonical key-error string still required. |
| L-14 | YES | Sensor-agnostic JSON contract was the HC-SR04 adoption rationale; preserved. |
| L-15a | YES | HC-SR04 bench-or-swap-within-15min rule applies to 4+ sensors in helios. |
| L-15 | YES | TTS cost accumulates faster in helios (3 scenes × multiple runs). |
| B-14 | YES | `LED_BUILTIN` guard still needed for any new ESP32 firmware (haptic/PTT). |
| W-01 | YES | LLaVA 3.5 s fallback latency still exceeds 3 s target — GPT-4o primary path must be stable. |
| W-05 | YES | Camera permission re-prompts per-terminal-app; document launch shell at demo setup. |

**Entries that do NOT apply:**

| ID | Reason |
|----|--------|
| B-03 / L-05 resolved | Ollama 0.23.2 fix already applied; only watch for regression, not active bug. |
| B-04 | Resolved by permission grant; document in demo checklist but not an active concern. |
| Any ESP32-CAM reference | ESP32-CAM is not used in helios; USB-C webcam replaces it entirely. |

---

## Critical Pitfalls

### PITFALL-01: HC-SR04 Cross-Talk on Round-Robin Without Adequate Inter-Trigger Delay

**What goes wrong:**
With 4+ HC-SR04 units firing in round-robin, the ultrasonic burst from sensor N is still
reverberating in the room when sensor N+1 fires. Sensor N+1's echo receiver picks up the
leftover energy from N's burst, returning a distance reading that is shorter than reality
(or wildly wrong). In a wearable worn near the body, room surfaces are close and reverberation
decay is slow. The Python `MQTTSonarSource` caches the latest reading per zone — a corrupt
reading poisons the cache for that zone until the next valid reading arrives.

**Why it happens:**
The HC-SR04 datasheet specifies a minimum 60 ms measurement cycle to prevent trigger signal
from bleeding into the echo window. Under time pressure, engineers shorten this to squeeze
update rate, not realizing 4 sensors at 60 ms each is already 240 ms full-cycle (4.2 Hz
per zone), and cheating to 30 ms per sensor causes cross-talk on the first sensor after
the rotation resets.

**How to avoid:**
- Enforce `>= 60 ms` delay between consecutive sensor triggers in firmware (not Python — the
  ESP32 controls timing). Never reduce below 60 ms to chase Hz.
- Stagger sensors physically: aim adjacent sonar cones so they do not share the same
  reflective plane (e.g., forward sensor points straight ahead, belt-left sensor points 45°
  outboard from body axis).
- In firmware, after each `pulseIn(ECHO)` call, add `delay(60)` before the next `digitalWrite(TRIG, HIGH)`.
- On the Python side, add a `last_updated` timestamp to each zone cache entry. If a zone
  has not updated in > 500 ms, treat it as stale and do not use it for distance queries.

**Warning signs:**
- Distance readings from one zone suddenly drop to 2–5 cm when no object is present.
- Sonar zone reads drop to zero then spike to 400 cm alternating — classic cross-talk.
- MQTT topic `helios/sonar/distance_mm` shows different zones publishing near-simultaneously
  (inter-message gap < 30 ms) in the Mosquitto log.

**Phase to address:** Sonar firmware phase (before PTT-01 integration). Write a bench that
logs raw zone readings to console for 30 seconds before wiring to Python.

---

### PITFALL-02: HC-SR04 Ground-Bounce and Short-Range Blind Spot on Wearable

**What goes wrong:**
The HC-SR04 has a hardware blind spot below ~2 cm and unreliable readings below 10–15 cm.
When worn on a belt or chest harness, the floor is often 70–120 cm away — within range — but
the 15-degree beam cone also catches the operator's own torso, clothing folds, and arms
during gesture. At < 30 cm to own body, readings jump erratically (reported: 5 cm reads as
7 cm; 2 cm reads 17–380 cm). A belt-forward sensor during GRAB_GUIDANCE will constantly see
the operator's arm reaching forward and return distance = arm length, not object distance.

**Why it happens:**
HC-SR04 is designed for flat-surface ranging on a stationary robot, not a torso-mounted
wearable where the sensor's own "host" body enters the beam cone constantly.

**How to avoid:**
- Mount forward sonar on the sternum or shoulder strap to maximize clearance from arms.
- In firmware, apply a median filter: collect 3 readings per measurement cycle and discard
  the outlier (min/max drop, take middle). This eliminates single-bounce spikes.
- Set a minimum valid distance threshold at the Python consumer: discard any reading < 15 cm
  as a self-echo artifact.
- For GRAB_GUIDANCE: when the hand is in MANIPULATION mode, mute belt sonar and switch to
  YOLO bounding-box + MediaPipe hand landmark distance instead of sonar (this is the
  intended design per `HDLR-03` — make sure sonar mute actually executes).

**Warning signs:**
- Sonar consistently reads 10–25 cm even when no object is in front of operator.
- Distance readings fluctuate ±50 cm during arm movement.
- TTS says "Object at 15 cm" when object is clearly 60 cm away.

**Phase to address:** Sonar firmware + HDLR-02/03 handler integration phase. Validate with
a 60-second wear test before demo rehearsal.

---

### PITFALL-03: ESP32 ECHO Pin Voltage — 5V HC-SR04 Destroys 3.3V ESP32 GPIO

**What goes wrong:**
The standard HC-SR04 (without "+") is a 5V device. The ECHO pin outputs 5V logic. The ESP32
GPIO inputs are 3.3V-tolerant only. Connecting ECHO directly to an ESP32 GPIO without a
level shifter or voltage divider immediately stresses the pin; repeated use causes latent
silicon damage that manifests as intermittent misreads before the pin fails entirely. This
does not fail on first connection — it fails mid-demo after thermal stress.

**Why it happens:**
Many tutorials and quickstart guides show direct ECHO → ESP32 GPIO wiring, noting "it works
in practice." The ESP32 does not immediately reject 5V (there is some ESD protection), so
the circuit appears functional during bench validation but degrades over hours of use.

The helios codebase note in CONCERNS.md already flags `firmware/sonar_esp32/src/main.cpp`
line 29 (ECHO=15 with level shifter divider) — this pattern must be preserved and verified
on all 4 sonar units, not just the first one wired.

**How to avoid:**
- Use the voltage divider already documented in firmware: 1 kΩ + 680 Ω (or 2.7 kΩ + 4.7 kΩ)
  on every ECHO line before it reaches ESP32 GPIO.
- Alternatively: use HC-SR04+ (3.3V variant) for all 4 units — eliminates level shift
  entirely. Confirm part marking before wiring.
- Before any sonar bench: confirm sensor VCC, GND, TRIG, ECHO routing with multimeter.
  Per L-15a rule: if any sensor refuses to produce readings after 15 minutes, suspect
  pin damage and substitute.

**Warning signs:**
- One sonar zone stops responding after 30+ minutes of bench testing but worked at start.
- ESP32 GPIO input reads permanently HIGH or permanently LOW after a session.
- Other GPIO pins on the same ESP32 begin misbehaving (voltage stress can damage
  neighboring pin's ESD cells).

**Phase to address:** Hardware wiring phase (before any firmware flash). Verify with
multimeter before and after first 30-minute bench run.

---

### PITFALL-04: Camera Index Reassignment on Reconnect or Reboot Breaks Demo Mid-Run

**What goes wrong:**
macOS AVFoundation assigns camera indices dynamically. Index 0 may be the built-in FaceTime
camera at cold boot, but after a USB-C webcam is unplugged and replugged (or after a USB hub
power cycle during demo setup), the indices can swap: the USB-C cam becomes 0, the built-in
becomes 1. The helios code currently hard-codes `device=0` or requires `--device 1` (per
CONCERNS.md "No Camera Auto-Detection"). If the index swaps mid-setup, the operator
silently gets the built-in FaceTime cam feeding YOLO — wrong FOV, wrong framing,
noticeably lower quality. YOLO detections degrade. The operator does not notice until
someone in the audience says "the camera is pointing at the ceiling."

**Why it happens:**
On macOS, `cv2.VideoCapture(N)` uses AVFoundation which re-enumerates devices at each
`VideoCapture()` construction call. There is a known OpenCV issue (opencv/opencv#26554)
where camera indices change across sessions and after USB reconnect events.

**How to avoid:**
- At demo-day startup: run `python -c "import cv2; [print(i, cv2.VideoCapture(i).isOpened()) for i in range(5)]"` and verify which index is the USB-C cam before launching the main loop.
- Add a startup camera enumeration step to `WebcamSource.__init__`: log the resolution and
  frame size of the opened device. USB-C webcam will typically report a different resolution
  than the built-in (e.g., 1920×1080 vs. 1280×720). Alert if resolution is below expected.
- Never unplug/replug the USB-C cam between demo runs. If a USB hub is used, confirm hub
  power is maintained throughout.
- Keep `--device` flag documented in the demo runbook with the correct index value verified
  that morning.

**Warning signs:**
- YOLO bounding boxes look vertically narrow or centered on a face/ceiling.
- Frame resolution logged at startup is 1280×720 instead of the USB-C cam's native res.
- `cv2.VideoCapture(device).get(cv2.CAP_PROP_FRAME_WIDTH)` returns 1280 — the built-in
  FaceTime cam default.

**Phase to address:** PERC-01 verification phase. Add a resolution check assertion
to the bench script.

---

### PITFALL-05: OpenCV AVFoundation Frame Buffer Stale Frames Under Load

**What goes wrong:**
OpenCV's `VideoCapture.read()` on macOS AVFoundation buffers multiple frames internally.
When the perception loop is busy (YOLO forward pass + MediaPipe in series), `read()` returns
a frame from several hundred milliseconds ago, not the current frame. During GRAB_GUIDANCE,
this means the hand-tracking landmarks and YOLO bounding box are temporally misaligned —
the system thinks the hand is where it was 300–500 ms ago, not where it is now, causing
incorrect directional cues ("move right" when hand is already aligned).

**Why it happens:**
AVFoundation queues frames at the camera frame rate (30 fps) regardless of whether the
consumer reads them. When the consumer runs at 10–17 Hz (helios bench target), the buffer
holds 2–5 stale frames. `read()` returns FIFO order, so you are always N frames behind.

**How to avoid:**
- Run camera reads in a dedicated thread that continuously drains the buffer and stores
  only the latest frame. The main perception loop reads from the stored-latest, not from
  `VideoCapture.read()` directly.
- Pattern (already known from OpenCV community):
  ```python
  class CameraThread(threading.Thread):
      def run(self):
          while self._running:
              ret, frame = self.cap.read()  # drains buffer
              if ret:
                  with self.lock:
                      self.latest = frame
  ```
- Set `cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)` on `VideoCapture` init to minimize the OS
  buffer. Note: AVFoundation may not honor this on macOS, but it reduces the impact.

**Warning signs:**
- GRAB_GUIDANCE haptic cues feel consistently "one step behind" the hand position.
- Visible lag between hand movement and detection response at demo walk-through.
- Adding `time.sleep(0)` to the loop worsens apparent detection lag (buffer drain falls
  further behind).

**Phase to address:** PERC-04 combined loop verification phase, before HDLR-03 integration.

---

### PITFALL-06: Hardware PTT Button Debounce Omission Causes Double-Trigger on Voice Loop

**What goes wrong:**
A mechanical button press generates multiple electrical transitions in the first 5–20 ms
(contact bounce). Without software debounce in the ESP32 firmware, a single button press
fires `helios/button/ptt` 2–5 times. The Python voice loop receives the first MQTT message,
starts recording, then receives the second message and interprets it as "stop recording" (or
starts a second concurrent recording), corrupting the Whisper transcription buffer.
At demo time, the operator presses PTT and gets either silence (double-trigger cancelled
recording) or two overlapping recordings submitted to Whisper.

**Why it happens:**
Firmware developers new to ESP32 button handling often read the GPIO pin state directly in
an interrupt service routine without debounce logic, because the bench test works fine with
slow deliberate presses. Fast or firm presses at demo stress reveal the bounce.

**How to avoid:**
- In ESP32 firmware: implement software debounce with a 50 ms lockout window.
  After the first edge-triggered interrupt fires and publishes `PTT_PRESSED`, ignore all
  subsequent transitions for 50 ms before re-arming the interrupt.
  ```cpp
  volatile unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  
  void IRAM_ATTR buttonISR() {
    unsigned long now = millis();
    if (now - lastDebounceTime > debounceDelay) {
      lastDebounceTime = now;
      pttPressed = true;  // set flag; handle in loop(), not in ISR
    }
  }
  ```
- Publish MQTT message from `loop()` where the flag is checked, not from within the ISR.
  ISRs + MQTT publish calls are unstable (WiFi stack is not ISR-safe on ESP32).
- On the Python side: add a 100 ms dedup window — if `PTT_PRESSED` arrives within 100 ms
  of the previous `PTT_PRESSED`, discard the second message.

**Warning signs:**
- Serial monitor shows button ISR firing 2–4 times per physical press.
- Voice loop logs show `start_recording` followed immediately by `stop_recording` within
  < 200 ms.
- Whisper receives a 0.1-second audio clip and returns empty transcription.

**Phase to address:** PTT-01 firmware + voice loop integration phase. Verify with a
button-mash bench (20 fast presses, verify exactly 20 MQTT messages received by Python).

---

### PITFALL-07: MQTT PTT Message Delivery Latency Exceeds Voice Loop Responsiveness Expectation

**What goes wrong:**
The voice loop's start-record trigger depends on an MQTT message arriving from the ESP32 via
the Mosquitto broker running on the laptop. Under venue WiFi stress (crowded channel, hotspot
contention), the ESP32 → broker → Python subscriber path introduces 50–200 ms of jitter.
The demo operator presses the button and hears a 150–300 ms delay before the recording beep
plays. At 10+ people standing around asking "did it work?", this gap feels like a system
failure and the operator presses again — causing the double-trigger scenario in PITFALL-06.

Additionally: if the Mosquitto broker on the laptop is not running when the ESP32 connects,
the ESP32 will not automatically retry the MQTT connection in many default firmware setups.
The ESP32 appears connected to WiFi (hotspot) but MQTT messages are silently dropped.

**Why it happens:**
- macOS hotspot + ESP32 STA mode has documented reconnection edge cases (forum reports of
  40+ second reconnection delays when hotspot think-device-is-still-connected state persists).
- Default paho-mqtt `loop_start()` reconnection behavior does not immediately re-subscribe
  after a disconnect — subscriptions must be renewed in `on_connect()` callback, not once at
  startup.
- QoS 0 (fire-and-forget) MQTT messages are silently dropped on any network interruption,
  with no retry. PTT button events are short-lived and non-retryable — use QoS 1 for PTT.

**How to avoid:**
- On ESP32 firmware: implement MQTT reconnection loop in `loop()`. Check `client.connected()`
  every iteration; if false, call `client.connect()` before attempting any publish.
- Use QoS 1 for `helios/button/ptt` topic on both publisher (ESP32) and subscriber (Python).
  The ≤150 ms delivery target is achievable on a local broker even with QoS 1 overhead.
- On Python side: subscribe to `helios/button/ptt` inside the `on_connect()` callback, not
  at startup. This ensures subscription survives broker restarts.
- Mosquitto broker startup: add `mosquitto -d` to the demo startup runbook as step 1.
  Include a health check: `mosquitto_pub -t test -m ping -q 1` before launching voice loop.
- Demo button visual feedback: add a LED or buzzer to the PTT button ESP32 that lights on
  MQTT publish confirmation (QoS 1 PUBACK received). Operator has hardware confirmation
  the message was sent.

**Warning signs:**
- MQTT subscribe callback fires > 200 ms after button press (measure with timestamps on
  both ESP32 serial and Python log).
- Python voice loop logs show no `PTT_PRESSED` event despite button press visible on ESP32
  serial (broker unreachable).
- `mosquitto_sub -t helios/#` shows no traffic from ESP32 despite ESP32 being on hotspot.

**Phase to address:** PTT-01 + MQTT broker setup phase. Bench separately before connecting
to voice loop.

---

### PITFALL-08: One-Engineer Parallelism Trap — Hardware Debug Blocks Software Milestones

**What goes wrong:**
Helios is explicitly a one-engineer project (PROJECT.md context). Blind-assist used a 2-eng
split. In blind-assist, one engineer could debug sonar firmware while the other integrated
voice. In helios, a 2-hour hardware debug session (e.g., sonar cross-talk, pin damage) burns
time from software milestones that cannot run without the hardware. With a this-week timeline,
a single unexpected hardware failure — one ESP32 bricked, one sonar unit dead, one motor
driver unstable — can make the difference between a passing DEMO-01 and a WoZ fallback.

The specific risk: the firmware is in "design-only state" with no hardware validation
(CONCERNS.md "Firmware Not Flashed/Tested"). This entire validation budget must fit within
the sprint alongside software integration.

**Why it happens:**
Single-engineer sprints naturally serialize hardware and software. The instinct is to finish
all firmware, then integrate software — but this leaves integration testing until the end,
when there is no time buffer.

**How to avoid:**
- Hardware-first ordering within the sprint: flash and bench each ESP32 board in isolation
  before any Python integration. Goal: by end of day 1, every sensor produces valid MQTT
  messages. Software can be mocked until then.
- Keep a second ESP32 board as a cold spare. If the primary board develops a flaky GPIO
  (see PITFALL-03), swap in the spare immediately rather than diagnosing.
- Time-box hardware diagnosis per L-15a: if any sonar unit, motor driver, or PTT circuit
  takes > 15 minutes to produce expected behavior, swap component and continue.
- Define a software-only mock path for every hardware dependency: `MockMQTTSonarSource`
  and keyboard PTT substitute already exist (PROJECT.md). If hardware day bleeds into
  software day, run the demo loop against mocks, then swap in real hardware for final
  integration.
- Demo rehearsal schedule: reserve 4 hours the day before demo for 3 back-to-back runs
  (DEMO-01). Do not start this rehearsal without all hardware working.

**Warning signs:**
- It is day 3 of the sprint and firmware is not yet producing MQTT traffic.
- More than 2 hardware components are unvalidated simultaneously.
- Python integration is being developed without a running MQTT broker + firmware.

**Phase to address:** Sprint planning / phase ordering. Hardware bench must precede all
software integration phases.

---

### PITFALL-09: 3 Back-to-Back Demo Runs — State Not Reset Between Runs Causes Cumulative Failure

**What goes wrong:**
DEMO-01 requires 3 back-to-back 2-minute runs. Each run transitions through IDLE →
LOCOMOTION → MANIPULATION → IDLE. If the state machine does not cleanly reset to IDLE after
run N, run N+1 starts in MANIPULATION mode — haptic cues fire immediately, sonar mutes
unexpectedly, and voice loop enters wrong handler dispatch. The audience sees the second run
fail in the first 10 seconds.

Beyond state machine: TTS audio cache from run N may play in run N+1 (duplicate cue
suppression dedup window). YOLO bounding box from the previous object target may linger in
the perception snapshot used by HDLR-02.

**Why it happens:**
Demo prototypes are usually tested as a single run. Multi-run testing is skipped under time
pressure. The state machine's IDLE reset path (30 s timeout or manual override) may not be
exercised in single-run bench.

**How to avoid:**
- Add an explicit `reset_demo()` function that returns state machine to IDLE, clears YOLO
  target lock, clears TTS dedup cache, and re-zeros haptic motor state. Call between runs.
- Test 3-run sequence explicitly during rehearsal: run 1 → reset → run 2 → reset → run 3.
  Do not only test run 1.
- Between runs: verbally announce "reset" and confirm ESP32 haptic motors are off, sonar
  readings look nominal on the debug display, state machine is `IDLE`.
- GRAB_GUIDANCE timeout (30 s per HDLR-03): verify this timeout fires correctly and
  transitions to IDLE even if no object overlap is detected.

**Warning signs:**
- Haptic motors remain vibrating after run 1 ends.
- YOLO detects last run's target object in run 2's scene (stale bounding box).
- Voice loop speaks "switching to offline mode" in run 2 (offline window from run 1 not reset).

**Phase to address:** DEMO-01 integration phase. Multi-run test is a required acceptance
criterion, not optional.

---

### PITFALL-10: Audio Device Index Slippage — USB Lavalier Mic Replaced by Built-In at Demo

**What goes wrong:**
Like the camera (PITFALL-04), `sounddevice` on macOS assigns audio device indices
dynamically at system enumeration. The USB lavalier mic may be device index 1 at home bench
but device index 3 at the demo venue after the venue's audio interface or a judge's laptop
USB hub shares the USB bus. If the voice loop opens the wrong audio input device, it records
from the built-in laptop mic — distant, reverberant, catching ambient crowd noise instead
of the operator's voice. Whisper transcription quality drops from ~95% to ~60% in noisy
environments, causing UNKNOWN intent classifications and "please repeat" cue loops.

**Why it happens:**
`sounddevice.default.device` is set at import time and depends on OS audio routing, which
can shift when the venue's AV equipment is connected. Engineers set the device index at
home bench and assume it persists.

**How to avoid:**
- At demo setup: run `python -c "import sounddevice as sd; print(sd.query_devices())"` and
  confirm the lavalier mic index before launching. Hard-code the device name (not index)
  in the config or do a name-match enumeration:
  ```python
  mic = next(d for d in sd.query_devices() if 'lavalier' in d['name'].lower())
  ```
- In the demo runbook: step 2 is "verify mic device index and update config."
- Test at the demo venue at least 30 minutes before the audience arrives, with the venue
  AV equipment plugged in, to surface index shifts before they matter.

**Warning signs:**
- Whisper transcriptions contain crowd noise transcribed as word salad.
- `sd.query_devices()` shows an unfamiliar device at the expected index.
- Intent classifier returns UNKNOWN more than 1 in 3 attempts during rehearsal.

**Phase to address:** VOIC-01 verification phase. Include device name logging at startup.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Hard-code camera device=0 | Simpler code | Demo fails silently on index shift | Never for demo day — add enumeration check |
| QoS 0 for all MQTT topics | Lower latency, simpler firmware | PTT messages silently dropped on any WiFi hiccup | OK for sonar (high-frequency, staleness acceptable); NOT OK for PTT events |
| Skip median filter on sonar | Fewer LOC in firmware | Single-bounce spikes corrupt DISTANCE_QUERY cues | Never — add filter before integration |
| Run demo from same terminal session used for development | Convenient | Wrong Python env, wrong camera permission shell, cached env vars | Never for demo day — fresh terminal, `uv run`, verified env |
| Single demo rehearsal run | Saves 20 min | Multi-run state bugs not caught | Never — DEMO-01 requires 3 runs, test 3 runs |
| Mock sonar for voice loop development | Unblocks software | Hardware integration surprises emerge day-of | Acceptable during software-only phases; must be removed before DEMO-01 |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| HC-SR04 → ESP32 ECHO pin | Direct 5V ECHO to 3.3V GPIO | Voltage divider (1kΩ + 680Ω) or use HC-SR04+ 3.3V variant |
| ESP32 MQTT button publish | Publish from ISR | Set flag in ISR, publish from `loop()` (WiFi stack not ISR-safe) |
| paho-mqtt Python subscriber | Subscribe once at startup | Subscribe inside `on_connect()` callback to survive broker restart |
| OpenCV USB cam on macOS | Trust `VideoCapture(0)` | Enumerate devices, verify resolution, log index at startup |
| sounddevice mic on macOS | Trust device index from bench | Match by device name, not index |
| Mosquitto broker on macOS | Start manually, forget to check | Make broker start step 1 of demo runbook; health-check before voice loop |
| HC-SR04 multi-sensor round-robin | Shorten inter-trigger delay for higher Hz | Enforce ≥60 ms between triggers; accept 4.2 Hz per zone |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Sonar reading too fast (< 60 ms cycle) | Cross-talk false readings on adjacent zones | Enforce 60 ms inter-trigger in firmware | Immediately on 2+ concurrent sensors |
| OpenCV buffer accumulation under YOLO load | GRAB_GUIDANCE direction lags hand position by 300–500 ms | Dedicated camera-read thread draining buffer | At perception loop < 20 Hz (helios target is 10 Hz — this WILL occur) |
| YOLO RLock contention during GRAB_GUIDANCE | Detection latency spikes block haptic update cadence | Single YOLO owner thread; GrabGuide reads snapshot | When GrabGuide + SCENE_DESC handlers run concurrently |
| TTS cache miss on new phrases | API call per directional cue; accumulates cost + 0.5–1 s latency per cue | Pre-warm TTS cache for all compass cues (8 directions) before demo | During live GRAB_GUIDANCE where cues change every 0.5–1 s |
| LLaVA fallback during demo (W-01) | 3.5 s SCENE_DESC latency breaks <3 s target | Keep OpenAI GPT-4o primary; verify API key and network at setup | Any OpenAI timeout; bad venue WiFi forces fallback |

---

## "Looks Done But Isn't" Checklist

- [ ] **Sonar MQTT wired**: ESP32 publishes `helios/sonar/distance_mm` — verify MQTT topic name matches Python `MQTTSonarSource` subscriber exactly (case-sensitive, no trailing slash).
- [ ] **PTT debounce active**: button-mash bench (20 presses) shows exactly 20 MQTT messages — not 21–40.
- [ ] **State machine IDLE reset**: after GRAB_GUIDANCE 30 s timeout, state is IDLE, haptics off, sonar re-enabled.
- [ ] **DEMO-02 panic fallback ready**: pre-recorded video file queued, keyboard override hotkey tested, WoZ operator knows the key sequence before demo starts.
- [ ] **Voltage divider on all 4 ECHO lines**: not just the one that was wired first.
- [ ] **Camera and mic device indices verified at venue**: run enumeration check on-site after plugging in all hardware.
- [ ] **Mosquitto broker autostart or manual start documented**: confirm `mosquitto -d` is in step 1 of demo runbook, and that it is running before ESP32 boards are powered.
- [ ] **Ollama running before demo**: `ollama serve` started and LLaVA model loaded (model load takes 15–30 s on first pull from RAM) — `curl http://127.0.0.1:11434/api/tags` returns model list.
- [ ] **TTS compass cues pre-cached**: run a cache warm pass through all 8 directional phrases before demo to avoid first-use API latency.
- [ ] **3-run sequence tested end-to-end**: not just 1 run. Run 2 starts from a clean IDLE state.
- [ ] **OpenAI API key in `.env` correct**: run `uv run python -c "from helios.cloud.openai_client import get_client; get_client()"` as smoke test.
- [ ] **Hotspot SSID and password on ESP32 firmware match demo laptop hotspot**: re-flash is not an option 10 minutes before demo.

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Camera index swap mid-demo | LOW | `Ctrl-C`, run `python -c "..."` enumerate, relaunch with `--device N` |
| Sonar cross-talk producing bad readings | LOW | Increase firmware inter-trigger delay (reflash takes ~30 s with PlatformIO) |
| ESP32 GPIO pin damage (sonar) | MEDIUM | Swap to backup pin or backup ESP32 board; update GPIO define, reflash |
| PTT double-trigger at demo | LOW | Add Python-side 100 ms dedup (no reflash needed); or use keyboard fallback |
| MQTT broker not running | LOW | `mosquitto -d` from terminal; ESP32 reconnects in < 10 s if firmware loop is correct |
| LLaVA fallback latency (W-01) | LOW | Acceptable for demo — announce "running in offline mode"; latency is 3.5 s, not infinite |
| Full hardware failure mid-demo | LOW (cost) HIGH (stress) | DEMO-02: keyboard WoZ override + pre-recorded video; activate without hesitation |
| State machine stuck in MANIPULATION | LOW | Keyboard override to force IDLE; add explicit `reset` hotkey to demo operator UI |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| PITFALL-01: Sonar cross-talk | Sonar firmware bench (before Python integration) | 30 s raw zone log shows no cross-talk spikes |
| PITFALL-02: Ground-bounce / wearable blind spot | Sonar firmware + HDLR-02 integration | 60 s wear test; min distance threshold enforced in Python |
| PITFALL-03: ECHO pin voltage damage | Hardware wiring (day 1) | Multimeter check before and after 30 min bench |
| PITFALL-04: Camera index slippage | PERC-01 bench + demo runbook | Resolution assertion at startup logs expected value |
| PITFALL-05: Stale frame buffer | PERC-04 combined loop | GrabGuide latency test: hand move → haptic response < 200 ms |
| PITFALL-06: PTT double-trigger | PTT-01 firmware + voice integration | Button-mash bench: 20 presses = 20 MQTT messages |
| PITFALL-07: MQTT PTT latency | PTT-01 + MQTT broker setup | Timestamped round-trip bench: button press → Python callback < 200 ms |
| PITFALL-08: One-engineer parallelism | Sprint planning / phase ordering | Hardware MQTT-producing by end of sprint day 1 |
| PITFALL-09: Multi-run state reset | DEMO-01 rehearsal phase | 3 consecutive runs pass without manual reset between runs |
| PITFALL-10: Audio device slippage | VOIC-01 bench + demo runbook | Name-based device selection; on-site enumeration check |

---

## Sources

- HC-SR04 cross-talk and minimum range: [Random Nerd Tutorials HC-SR04](https://randomnerdtutorials.com/complete-guide-for-ultrasonic-sensor-hc-sr04/), [Arduino Forum — ignore false readings](https://forum.arduino.cc/t/how-to-negelct-false-readings-using-an-hc-sr04-ultrasonic-sensor/550734), [Arduino Forum — short range bad reading](https://forum.arduino.cc/t/hc-sr04-bad-reading-at-close-range/646988)
- HC-SR04 with ESP32 level shifter: [Random Nerd Tutorials HC-SR04 + ESP32](https://randomnerdtutorials.com/esp32-hc-sr04-ultrasonic-arduino/), [Instructables 3.3V mod](https://www.instructables.com/Modify-Ultrasonic-Sensors-for-3-Volts-Logic-prepar/)
- OpenCV macOS camera index instability: [opencv/opencv#26554](https://github.com/opencv/opencv/issues/26554), [OpenCV forum: camera index prevention](https://forum.opencv.org/t/how-to-specify-exact-camera-by-id-to-prevent-two-usb-cameras-from-being-switched-around-macos/12351)
- OpenCV AVFoundation stale frame buffer: [opencv/opencv#13145](https://github.com/opencv/opencv/issues/13145), [OpenCV Q&A: buffer lag](https://answers.opencv.org/question/74255/time-delay-in-videocapture-opencv-due-to-capture-buffer/), [opencv/opencv#24170](https://github.com/opencv/opencv/issues/24170)
- ESP32 button debounce: [samgalope.dev ESP32 debounce 2024](https://www.samgalope.dev/2024/12/19/a-hardware-approach-esp32-button-debounce/)
- MQTT QoS and local broker latency: [HiveMQ MQTT essentials QoS](https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels/), [PMC livestock IoT MQTT QoS study](https://pmc.ncbi.nlm.nih.gov/articles/PMC12694545/)
- paho-mqtt reconnection issues: [paho.mqtt.python#331](https://github.com/eclipse-paho/paho.mqtt.python/issues/331), [paho.mqtt.python#737](https://github.com/eclipse-paho/paho.mqtt.python/issues/737)
- ESP32 WiFi/MQTT reconnect reliability: [ESP32 Forum: reliable WiFi MQTT](https://esp32.com/viewtopic.php?t=16109), [Random Nerd Tutorials: reconnect](https://randomnerdtutorials.com/solved-reconnect-esp32-to-wifi/)
- sounddevice macOS device issues: [python-sounddevice#505](https://github.com/spatialaudio/python-sounddevice/issues/505)
- Helios codebase-specific: `docs/LESSONS_LEARNED.md` (L-01..L-15, B-01..B-14, W-01, W-05), `.planning/codebase/CONCERNS.md`, `.planning/PROJECT.md`

---

*Pitfalls research for: wearable AI accessibility demo — helios*
*Researched: 2026-05-09*
*Scope: NEW pitfalls only; LESSONS_LEARNED.md entries validated separately above*
