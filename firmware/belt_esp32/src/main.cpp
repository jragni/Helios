// Helios haptic belt — 2× HC-SR04 at 180° spacing → 2× ERM vibration motors.
// Standalone: no WiFi, no MQTT. Closer = stronger vibration.
//
// Wiring per zone (HC-SR04 echo is 5V — level-shift ECHO to 3.3V):
//   HC-SR04 VCC  → 5V
//   HC-SR04 GND  → GND
//   HC-SR04 TRIG → ESP32 GPIO (3.3V drive is enough to trigger)
//   HC-SR04 ECHO → ESP32 GPIO via 1kΩ series + 2kΩ to GND (5V→3.3V divider)
//   Motor +      → 5V via NPN transistor collector (base ← GPIO via 1kΩ)
//   Motor -      → transistor emitter → GND
//   Flyback diode across motor terminals (cathode to +)
//
// GPIO assignments:
//   Zone A (  0°): TRIG=4,  ECHO=5,  MOTOR=16
//   Zone B (180°): TRIG=13, ECHO=15, MOTOR=17

#include <Arduino.h>

struct Zone {
    uint8_t trig;
    uint8_t echo;
    uint8_t motor;
    uint8_t pwmCh;
};

static Zone ZONES[2] = {
    {4,  5,  16, 0},  // Zone A (  0°)
    {13, 15, 17, 1},  // Zone B (180°)
};

static const uint32_t ECHO_TIMEOUT_US = 25000;  // ~4.3 m max; no point waiting longer
static const int      MIN_BUZZ_MM     = 20;      // at or below → full buzz (255)
static const int      MAX_BUZZ_MM     = 2000;    // at or above → no buzz (0)
static const uint8_t  MIN_DUTY        = 40;      // floor so motor always spins once in range
static const uint32_t MOTOR_PWM_FREQ  = 200;     // Hz — typical ERM operating freq
static const uint8_t  MOTOR_PWM_BITS  = 8;       // 0–255 duty
static const uint32_t INTER_PING_MS   = 20;      // gap between zone pings (prevents crosstalk)

// Exponential ramp: gentle hum at range, surges hard when close.
// Formula: MIN_DUTY + (255 - MIN_DUTY) * (e^(k*t) - 1) / (e^k - 1)
// t=1.0 at MIN_BUZZ_MM (full), t=0.0 at MAX_BUZZ_MM (floor).
// EXP_K controls steepness — higher = more of the buzz concentrated close-in.
static const float EXP_K = 4.0f;

static uint8_t distToDuty(int mm) {
    if (mm < 0 || mm > MAX_BUZZ_MM) return 0;
    if (mm <= MIN_BUZZ_MM) return 255;
    float t = (float)(MAX_BUZZ_MM - mm) / (MAX_BUZZ_MM - MIN_BUZZ_MM);
    float curve = (expf(EXP_K * t) - 1.0f) / (expf(EXP_K) - 1.0f);
    return (uint8_t)(MIN_DUTY + (255 - MIN_DUTY) * curve);
}

static int pingMm(uint8_t trig, uint8_t echo) {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    uint32_t us = pulseIn(echo, HIGH, ECHO_TIMEOUT_US);
    if (us == 0) return -1;
    return (int)(us * 343UL / 2000UL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("=== belt_esp32 boot ===");
    for (auto& z : ZONES) {
        pinMode(z.trig, OUTPUT);
        digitalWrite(z.trig, LOW);
        pinMode(z.echo, INPUT);
        ledcSetup(z.pwmCh, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
        ledcAttachPin(z.motor, z.pwmCh);
        ledcWrite(z.pwmCh, 0);
    }
}

void loop() {
    for (auto& z : ZONES) {
        int mm = pingMm(z.trig, z.echo);
        uint8_t duty = distToDuty(mm);
        ledcWrite(z.pwmCh, duty);
        Serial.printf("zone=%d mm=%4d duty=%3d\n", z.pwmCh, mm, duty);
        delay(INTER_PING_MS);
    }
}
