# Helios

> A spatial awareness wearable belt for the blind and visually impaired.

Helios is a consumer wearable that gives users a 360° sense of their surroundings through haptic feedback. Ultrasonic sensors detect nearby objects and translate distance into vibration intensity — closer objects vibrate stronger, farther objects vibrate softer. No screens, no audio, no training required.

---

## How It Works

Helios wraps around the waist with three sensor-motor pairs spaced 120° apart — front, left, and right. Each zone operates independently:

1. An ultrasonic sensor pings for nearby objects
2. Distance is mapped to vibration strength exponentially (2 cm = full buzz, 200 cm = silence)
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
| Front (0°)   | Navel     | GPIO 4  | GPIO 5  | GPIO 16 |
| Right (120°) | Right hip | GPIO 13 | GPIO 15 | GPIO 17 |
| Left (240°)  | Left hip  | GPIO 27 | GPIO 26 | GPIO 21 |

---

## Firmware

Built with **PlatformIO** targeting the ESP32 Dev Module.

### Key behaviors
- Sensors fire **sequentially** to prevent ultrasonic crosstalk between zones
- Vibration strength scales **exponentially** — buzz surges hard close-in, tapers at range
- Full intensity (PWM 255) at ≤ 2 cm, floor buzz (PWM 40) at 200 cm, silent beyond
- Motors always spin at minimum duty once an object is in range (overcomes stiction)
- ~60 ms loop cycle per full sensor sweep

### Setup

```sh
cd firmware/belt_esp32
pio run -t upload
pio device monitor
```

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
helios/
├── firmware/
│   └── belt_esp32/
│       ├── platformio.ini
│       └── src/main.cpp
└── README.md
```

---

## License

MIT
