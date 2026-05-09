# Feature Research

**Domain:** Wearable AI accessibility system — demo prototype for sighted-operator expo/hackathon presentation
**Researched:** 2026-05-09
**Confidence:** MEDIUM-HIGH (ecosystem well-documented; demo-specific tradeoffs drawn from hackathon literature + PROJECT.md constraints)

---

## Context: Demo Prototype vs Production Assistive Device

This research frames features specifically for a **one-week, sighted-operator demo** targeting a hackathon/expo audience — not a production assistive device for BVI users. That distinction drives every categorization below.

- Table stakes = what the demo audience expects to see to believe the concept works
- Differentiators = what separates this from a slide deck or a phone app demo
- Anti-features = features that look appealing but consume sprint capacity without improving demo impact

The three v1 scenes are fixed by PROJECT.md: **(1) scene description, (2) distance query, (3) grab guidance.** All feature analysis anchors to those three scenes.

---

## Table Stakes (Demo Audience Expects These)

Features the audience assumes exist. Missing any of these and the demo concept doesn't land.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Scene description via natural language | Core premise of "AI eyes" — the first thing any observer asks for | MEDIUM | Already carried over: GPT-4o vision + VISN-01. Target <3 s. Critical path. |
| Spoken audio output (TTS) | Without voice output, the system looks broken; accessibility implies audio | LOW | Already implemented: TTS-1 via OpenAI. Caching partially done. |
| Voice input / push-to-talk trigger | Observers expect "you talk to it" — PTT is the demo-safe implementation | MEDIUM | PTT-01 (ESP32 over MQTT). Keyboard fallback already exists. |
| Real-time object detection in frame | Audience expects the system to "know what it's looking at" | MEDIUM | PERC-02: YOLO-World at ≥10 fps. Already benched on prior scaffold. |
| Spoken distance estimate for named object | Completing the "how far is the mug" scene — without a number, scene 2 is hollow | MEDIUM | SONR-01 + HDLR-02. Sonar wired to MQTT; handler is the gap (D-22). |
| Directional guidance cue (haptic or spoken) | Scene 3 "help me grab it" needs to communicate direction; spoken alone is table stakes | HIGH | HDLR-03: LOCOMOTION → MANIPULATION state machine. Most complex requirement. |
| Offline / degraded-mode fallback | Demo network is unreliable; a dead screen kills the demo | MEDIUM | VISN-02: Ollama LLaVA fallback already partially wired. Startup health check missing (CONCERNS.md). |
| Demo reliability across 3 back-to-back runs | Judges see multiple teams; a crash is disqualifying | LOW (polish) | DEMO-01 + DEMO-02. Panic fallback (pre-recorded video + WoZ keyboard) already in scope. |

**Dependency note:** All three handlers (HDLR-01, HDLR-02, HDLR-03) depend on the intent classifier (VOIC-02) dispatching correctly. Intent classifier is untested (CONCERNS.md). This is a hidden table-stakes dependency — if classification misfires during the demo, none of the three scenes work.

---

## Differentiators (Visible Competitive Advantage for Audience)

Features that elevate Helios above a phone-app or slide-deck demo. Research from ObjectFinder (arXiv:2412.03118), AIris (arXiv:2405.07606), and the haptic navigation literature identifies these as the features that produce "wow" reactions at demos and that state-of-the-art systems treat as novel contributions.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| 4-motor wrist haptic compass | Physical, tactile directional cue is viscerally demonstrable — audience sees the wrist buzz and understands immediately | HIGH | HAPT-01. ESP32 firmware + HapticPublisher. Not yet flashed/tested (CONCERNS.md). This is the single highest-impact differentiator. |
| Multi-zone sonar (belt L/C/R + forward) | Judges can see multiple sensors; multi-zone is quantifiably more sophisticated than single-sensor distance | HIGH | SONR-01: 4+ HC-SR04 units on ESP32. Hardware on hand; firmware in design-only state. |
| Hardware PTT button (wearable-mounted) | Demo realism: operator doesn't reach for a keyboard. Makes the rig look like a real product, not a laptop script. | MEDIUM | PTT-01. ESP32 MQTT publish. Keyboard fallback preserved. |
| Cloud→local fallback with spoken transition cue | Audience sees graceful degradation: system says "switching to offline mode" and keeps going. Shows robustness thinking. | MEDIUM | VISN-02 already partially implemented. Ollama startup health check needed. |
| State machine: LOCOMOTION→MANIPULATION | Conceptually impressive: the system changes behavior mode based on task phase, not just responding to a fixed prompt. Judges from robotics/AI backgrounds will recognize this. | HIGH | MODE-01: IDLE→LOCOMOTION→MANIPULATION→IDLE. Was Phase 4 incomplete in prior scaffold. |
| Named-object targeting with voice ("find the mug") | Free-text object query is now state-of-the-art per ObjectFinder (arXiv:2412.03118). More impressive than fixed-category detection. | MEDIUM | Already in scope via YOLO-World open-vocab + VOIC-02 intent parser. Mostly already built. |

**Differentiation order for a one-week sprint:** The haptic wrist compass is the single feature that no phone app can replicate and that audiences understand instantly. It must work for the demo to be memorable. State machine mode switching is a close second for technically sophisticated audiences.

---

## Anti-Features (Deliberately NOT Build)

Features that look attractive but would consume sprint capacity, increase demo fragility, or conflict with PROJECT.md Out of Scope. Each maps to a specific reason.

| Feature | Why It Looks Attractive | Why NOT for This Demo | Mirrors PROJECT.md OOS? |
|---------|------------------------|----------------------|-------------------------|
| Text / document reading (OCR) | Every AI glasses product (Envision, Ally Solos) does this; audiences expect it | Separate CV pipeline (OCR), separate prompting, separate demo scene — adds a 4th scene with no carry-over code. Zero contribution to the 3-scene loop. | No (not listed OOS, but implied by "reliability beats features") |
| Face recognition | Impressive, privacy-relevant, demo-flashy | Requires a second model (face embed), adds PII/consent risk in a public expo, doesn't connect to any of the 3 scenes | No (implicit) |
| Always-on / VAD listening | Sounds more natural than PTT | Requires real-time audio expertise (VAD tuning, noise rejection), high false-positive rate in expo environments, adds latency to pipeline | Yes — explicitly OOS: "Always-on listening / VAD — push-to-talk sidesteps real-time audio expertise gap" |
| Outdoor navigation (GPS, path planning, curb detection) | Audiences associate AI glasses with navigation | HC-SR04 is indoor only; outdoor use with this rig is unsafe without proper review; no GPS hardware on hand | Yes — explicitly OOS: "Outdoor navigation, traffic safety, curb detection" |
| Bone-conduction / BT audio (Shokz, etc.) | More "product-like" form factor | BT pairing failure risk during live demo; judges can't hear system output if they're not wearing the device | Yes — explicitly OOS: "no pairing risk, judges hear what user hears" |
| Multi-session or cross-user memory | Shows "learning" behavior | Requires persistent store, session management, introduces state bugs. One operator, one session. | Yes — explicitly OOS: "Multi-user / multi-session memory" |
| Production auto-recovery / self-healing | Robustness is impressive | DEMO-02 panic fallback (pre-recorded video + WoZ) is sufficient and cheaper. Self-healing logic introduces new failure modes. | Yes — explicitly OOS: "demo includes manual panic fallback, not seamless self-healing" |
| Mobile / phone form factor | Smaller and more portable | Backpack laptop locked; porting to phone is a different codebase and a different sprint | Yes — explicitly OOS: "Mobile/phone form factor — backpack laptop locked" |
| Money / barcode / product label reading | AIris includes this; sounds useful | Another separate demo scene; doesn't connect to the 3-scene loop; adds CV pipeline complexity | No (implicit) |
| Emotion / social cue recognition | 2025 AI glasses hype feature | No training data for this task on this rig; no connection to the 3 scenes; high hallucination risk | No (implicit) |
| Continuous obstacle avoidance (belt + cane replacement) | Makes it sound like a cane replacement | Explicitly excluded: "Mobility-aid replacement — cane/dog still required." Sonar belt for continuous obstacle avoidance would require always-on inference loop and compete with PTT loop. | Yes — explicitly OOS: "Mobility-aid replacement" |

---

## Feature Dependencies

```
[PTT-01: Hardware push-to-talk button]
    └──triggers──> [VOIC-01: Whisper STT]
                       └──feeds──> [VOIC-02: Intent classifier]
                                       ├──dispatches──> [HDLR-01: Scene description]
                                       │                    └──requires──> [VISN-01: GPT-4o vision]
                                       │                    └──fallback──> [VISN-02: Ollama LLaVA]
                                       ├──dispatches──> [HDLR-02: Distance query]
                                       │                    └──requires──> [PERC-02: YOLO-World detection]
                                       │                    └──requires──> [SONR-01: HC-SR04 sonar MQTT]
                                       │                    └──drives──> [HAPT-01: Wrist compass centering]
                                       └──dispatches──> [HDLR-03: Grab guidance]
                                                            └──requires──> [PERC-02: YOLO-World]
                                                            └──requires──> [PERC-03: MediaPipe Hands]
                                                            └──requires──> [HAPT-01: Wrist compass]
                                                            └──requires──> [MODE-01: State machine]

[VISN-02: Ollama LLaVA fallback]
    └──requires──> Ollama running locally (startup health check missing — CONCERNS.md)

[HAPT-01: Wrist haptic]
    └──requires──> ESP32 haptic firmware flashed (design-only state — CONCERNS.md)

[SONR-01: Sonar MQTT]
    └──requires──> ESP32 sonar firmware flashed (design-only state — CONCERNS.md)

[SONR-01] ──enhances──> [HDLR-02] (distance accuracy vs placeholder 100 cm — LAPT-07)
[SONR-01] ──enhances──> [HDLR-03] (proximity during MANIPULATION phase)
```

### Dependency Notes

- **VOIC-02 intent classifier is a single point of failure** for all three demo scenes. It is currently untested (CONCERNS.md). Must be smoke-tested before any demo rehearsal.
- **ESP32 firmware is a blocking dependency** for HAPT-01 (haptic) and SONR-01 (sonar). Both are in design-only state. They must be flashed and integration-tested before HDLR-02 and HDLR-03 can be end-to-end tested.
- **Ollama startup health check is a blocking dependency** for reliable fallback. Without it, a missing `ollama serve` causes a silent hang (CONCERNS.md: Missing Ollama Auto-Start).
- **HDLR-03 grab guidance** depends on all four other subsystems (YOLO, hands, haptic, state machine) being simultaneously functional — it is the last scene to build and the highest integration risk.
- **DEMO-02 panic fallback** has no dependencies on hardware — it can be built any time. Build it early.

---

## MVP Definition

### Launch With (v1 — this week's demo)

These are the minimum requirements for the 2-minute 3-scene loop to be demonstrable. Matches PROJECT.md active requirements.

- [ ] PTT-01 — Hardware button triggers voice pipeline (keyboard fallback as safety net)
- [ ] VOIC-01 + VOIC-02 — Whisper STT + intent classifier dispatching all three intents correctly
- [ ] HDLR-01 — Scene description via GPT-4o in <3 s (scene 1 works end-to-end)
- [ ] HDLR-02 — Distance query: YOLO target + sonar reading + spoken result (scene 2 works end-to-end)
- [ ] HAPT-01 — Wrist haptic fires correct quadrant within 150 ms (required for both scene 2 centering and scene 3 guidance)
- [ ] HDLR-03 — Grab guidance: full LOCOMOTION→MANIPULATION state machine (scene 3 works end-to-end)
- [ ] DEMO-01 — 3 back-to-back runs pass with no manual intervention
- [ ] DEMO-02 — Panic fallback (pre-recorded video + WoZ keyboard) ready and tested

### Add After Validation (v1.x — post-demo if time)

Features that increase demo polish but are not blocking the 3-scene core.

- [ ] Ollama startup health check — prevents silent hang on fallback path; low effort, high reliability payoff
- [ ] Camera auto-detection — eliminates `--device 1` manual flag; reduces setup friction at expo (CONCERNS.md)
- [ ] Cloud timeout tuned to 5 s — reduces perceived hang from 10 s to 5 s before fallback triggers (CONCERNS.md)
- [ ] TTS dedup for grab-guidance cues — prevents repeated "left and up" spam on slow approach; protects OpenAI budget

### Future Consideration (v2+ — out of demo sprint scope)

Defer these until the 3-scene loop is proven and there is a second week available.

- [ ] Text / OCR reading scene — valid 4th scene but separate pipeline
- [ ] Always-on VAD — requires audio expertise gap to close first
- [ ] Multi-session memory — requires persistent store design
- [ ] Outdoor navigation — requires safety review and GPS hardware
- [ ] Mobile form factor port — separate engineering track

---

## Feature Prioritization Matrix

| Feature | Demo Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| HDLR-01: Scene description (scene 1) | HIGH | LOW (carried over) | P1 |
| VOIC-02: Intent classifier | HIGH | LOW (carried over, needs smoke test) | P1 |
| PTT-01: Hardware PTT button | HIGH | MEDIUM (ESP32 + MQTT) | P1 |
| SONR-01: Sonar MQTT (multi-zone) | HIGH | HIGH (firmware not flashed) | P1 |
| HAPT-01: Wrist haptic compass | HIGH | HIGH (firmware not flashed) | P1 |
| HDLR-02: Distance query handler | HIGH | MEDIUM (sonar wired; handler is gap) | P1 |
| HDLR-03: Grab guidance handler | HIGH | HIGH (highest integration risk) | P1 |
| MODE-01: State machine | HIGH | MEDIUM (was incomplete in prior scaffold) | P1 |
| DEMO-02: Panic fallback | HIGH | LOW (pre-record + keyboard WoZ) | P1 (build early) |
| VISN-02: Ollama fallback + health check | MEDIUM | LOW | P2 |
| Camera auto-detection | MEDIUM | LOW | P2 |
| TTS cue dedup (grab guidance) | MEDIUM | LOW | P2 |
| Cloud timeout tuning (5 s) | MEDIUM | LOW | P2 |
| Text / OCR scene | LOW (for this demo) | HIGH | P3 |
| Face recognition | LOW | HIGH + PII risk | P3 |
| Always-on VAD | LOW | HIGH + fragility | P3 |

---

## Competitor Feature Analysis

Research comparators: AIris (arXiv:2405.07606, NTUA), ObjectFinder (arXiv:2412.03118), Envision Glasses, Ally Solos, Google Project Astra prototype.

| Feature | AIris / ObjectFinder (research prototypes) | Envision / Ally (commercial products) | Helios v1 approach |
|---------|-------------------------------------------|--------------------------------------|-------------------|
| Scene description | Yes — GPT-4o equivalent via API + local fallback | Yes — cloud VLM | Yes — GPT-4o + LLaVA fallback |
| Distance to named object | ObjectFinder: egocentric localization (distance + direction) | No (description only) | Yes — sonar + YOLO bbox centering |
| Haptic directional cue | Research headbands (8 motors); wrist form: compass belt projects | No (audio only) | Yes — 4-motor wrist compass (TOP/BOT/L/R) |
| Grab/reach guidance | Not in AIris/ObjectFinder (navigation to, not manipulation of) | No | Yes — full LOCOMOTION→MANIPULATION mode (novel for demo prototypes) |
| Push-to-talk | Some prototypes use PTT | BeMyEyes uses on-demand button | Yes — ESP32 hardware button |
| Always-on / VAD | Some research systems | Envision has always-on option | Explicitly excluded (OOS) |
| Text / OCR reading | AIris: yes | Envision: yes, Ally: yes | Excluded for v1 |
| Face recognition | AIris: yes | Envision: yes | Excluded for v1 |
| Offline fallback | Some (Raspberry Pi local models) | Limited | Yes — Ollama LLaVA |
| Mobile form factor | Raspberry Pi wearable in AIris | Glasses/phone | Excluded (backpack laptop locked) |

**Key finding:** No research prototype surveyed combines open-vocab object detection + sonar distance + wrist haptic compass + grab-phase state machine in a single demo. ObjectFinder gets closest (open-vocab + distance + direction) but uses audio-only output. The haptic compass + mode-switching state machine is the genuinely novel contribution of this demo.

---

## Sources

- AIris: An AI-powered Wearable Assistive Device for the Visually Impaired — [arXiv:2405.07606](https://arxiv.org/abs/2405.07606)
- ObjectFinder: Open-Vocabulary Assistive System for Interactive Object Search by Blind People — [arXiv:2412.03118](https://arxiv.org/abs/2412.03118)
- AI-Powered Assistive Technologies for Visual Impairment (survey) — [arXiv:2503.15494](https://arxiv.org/html/2503.15494v1)
- Wearable assistive system using object detection, distance measurement and tactile presentation — [OAE Publishing 2023](https://www.oaepublish.com/articles/ir.2023.24)
- Navigation Assistance Via Haptic Technology for Blind or Low-Vision Users: A Scoping Review — [SAGE Journals 2025](https://journals.sagepub.com/doi/10.1177/10711813251360706)
- Haptic's touch-based navigation for blind/sighted — [TechCrunch 2024](https://techcrunch.com/2024/10/29/haptics-touch-based-navigation-helps-blind-and-sighted-alike-get-around-without-looking/)
- Envision Glasses features — [letsenvision.com](https://www.letsenvision.com/glasses/home)
- How to present a successful hackathon demo — [Devpost](https://info.devpost.com/blog/how-to-present-a-successful-hackathon-demo)
- Hackathon judging criteria — [TAIKAI](https://taikai.network/en/blog/hackathon-judging)
- PROJECT.md Out of Scope section — `.planning/PROJECT.md`
- CONCERNS.md — `.planning/codebase/CONCERNS.md`

---

*Feature research for: Helios wearable AI accessibility demo prototype*
*Researched: 2026-05-09*
