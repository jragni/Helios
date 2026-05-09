# Helios

> A spatial awareness wearable belt for the blind and visually impaired.

Helios is a consumer wearable that gives users a 360° sense of their surroundings through haptic feedback. Ultrasonic sensors detect nearby objects and translate distance into vibration intensity — closer objects vibrate stronger, farther objects vibrate softer. No screens, no audio, no training required.

---

## How It Works

Helios wraps around the waist with three sensor-motor pairs spaced 120° apart — front, left, and right. Each zone operates independently:

1. An ultrasonic sensor pings for nearby objects
2. Distance is mapped to vibration strength (15 cm = full buzz, 120 cm = silence)
3. A coin vibration motor on the corresponding zone alerts the user

The result is an intuitive spatial map felt directly on the body.

---

## Hardware

| Component | Qty | Role |
|---|---|---|
| ESP32-WROOM-32 | 1 | Microcontroller + Bluetooth |
| HC-SR04 ultrasonic sensor | 3 | Object detection per zone |
| Coin vibration motor | 3 | Haptic feedback per zone |
| NPN transistor (2N2222) | 3 | Motor current switching |
| 1N4007 diode | 3 | Motor flyback protection |
| 1kΩ resistors | 9 | Current limiting + ECHO voltage dividers |

---

## Zones & GPIO Pinout

| Zone | Position | TRIG | ECHO | MOTOR |
|---|---|---|---|---|
| Front | Navel | GPIO 4 | GPIO 18 | GPIO 25 |
| Left | Left hip | GPIO 16 | GPIO 19 | GPIO 26 |
| Right | Right hip | GPIO 17 | GPIO 21 | GPIO 27 |

---

## Firmware

Built with Arduino IDE 2.x targeting the **ESP32 Dev Module** (esp32 by Espressif Systems, core v3.x).

### Key behaviors
- Sensors fire **sequentially** to prevent ultrasonic crosstalk between zones
- Vibration strength scales linearly from 120 cm → 15 cm
- Full intensity (PWM 255) triggers at ≤ 15 cm
- Silent (PWM 0) beyond 120 cm
- 80 ms loop cycle per full sensor sweep

### Setup

```bash
# Board manager URL (already configured in arduino-cli.yaml)
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

1. Install **esp32 by Espressif Systems** via Boards Manager
2. Select `Tools → Board → ESP32 Arduino → ESP32 Dev Module`
3. Select `Tools → Port → /dev/cu.SLAB_USBtoUART`
4. Upload sketch and open Serial Monitor at **115200 baud**

---

## Ecosystem

Helios is one half of a two-device system:

| Device | Role |
|---|---|
| **Helios Belt** | Spatial awareness — detects objects, delivers haptic feedback |
| **AI Glasses** | Visual AI processing — scene understanding, object recognition |

The ESP32's onboard Bluetooth is reserved for real-time sync between the belt and glasses. Protocol is under development.

---

## Design Philosophy

- **Haptic-first** — no audio output, no visual display, works in any environment
- **Zero training curve** — stronger buzz means closer object, that's it
- **Non-intrusive** — worn under clothing, complements a white cane without replacing it
- **Covers the cane's blind spots** — sides, rear, and objects at body height

---

## Repository Structure

```
Helios/
└── README.md        # This file
```

Firmware source coming soon.

---

## License

MIT
