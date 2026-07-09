/*
 * hal_esp32.h
 * Hardware Abstraction Layer für ESP32 (Arduino Framework)
 * Ersetzt die WMuCpp STM32-Bibliothek (Mcu::Stm::*, External::Tick, ...)
 *
 * Portierung von msw30 (STM32G0B1) → ESP32 / ESP32-S3
 */

#pragma once

#include <Arduino.h>
#include <cstdint>
#include <array>
#include <functional>

// ============================================================
//  System Tick  (ersetzt Mcu::Stm::SystemTimer, External::Tick)
// ============================================================

struct SystemTimer {
    // Auflösung: 1 ms  (millis())
    // Der Original-Code läuft mit 2000 Hz → alle 0,5 ms.
    // Für ESP32 verwenden wir 1 ms – ausreichend für alle Timeouts.
    static constexpr uint32_t ticksPerSecond = 1000;

    static uint32_t now() {
        return millis();
    }
    // Periodic wird in loop() aufgerufen
    static void periodic(std::function<void()> fn) {
        static uint32_t last = 0;
        uint32_t now_ms = millis();
        if (now_ms - last >= 1) {   // alle 1 ms
            last = now_ms;
            fn();
        }
    }
};

// Tick-Helfer: zählt Aufrufe, feuert nach N Ticks
template<uint32_t N>
struct Tick {
    uint32_t count = 0;
    bool elapsed() {
        if (++count >= N) { count = 0; return true; }
        return false;
    }
    void reset() { count = 0; }
};

// Konvertierung ms → Tick-Zähler (bei 1 ms / Tick)
constexpr uint32_t msToTicks(uint32_t ms) { return ms; }

// ============================================================
//  GPIO Pin  (ersetzt Mcu::Stm::Pin)
// ============================================================

template<uint8_t GpioNum>
struct Pin {
    static constexpr uint8_t pin = GpioNum;

    static void dir_output() {
        pinMode(GpioNum, OUTPUT);
    }
    static void dir_input_pullup() {
        pinMode(GpioNum, INPUT_PULLUP);
    }
    static void dir_input() {
        pinMode(GpioNum, INPUT);
    }
    static void set() {
        digitalWrite(GpioNum, HIGH);
    }
    static void reset() {
        digitalWrite(GpioNum, LOW);
    }
    static bool read() {
        return digitalRead(GpioNum) == HIGH;
    }
};

// ============================================================
//  PWM-Kanal  (ersetzt Mcu::Stm::V3::Pwm::Simple + PwmAdapter)
//  ESP32: LEDC-Peripherie, 8 Kanäle, 1 kHz, 8-bit Auflösung
// ============================================================

static constexpr uint32_t PWM_FREQ_HZ  = 1000;
static constexpr uint8_t  PWM_BITS     = 8;     // 0..255
static constexpr uint32_t PWM_MAX      = 255;

struct PwmChannel {
    uint8_t  gpio    = 255;   // 255 = nicht zugewiesen
    uint8_t  channel = 255;

    void init(uint8_t gpio_num, uint8_t ledc_ch) {
        gpio    = gpio_num;
        channel = ledc_ch;
        // ESP32 Arduino Core ≥ 3.x: ledcAttach ersetzt ledcSetup+ledcAttachPin
        ledcAttach(gpio_num, PWM_FREQ_HZ, PWM_BITS);
        ledcWrite(gpio_num, 0);
    }
    void setDuty(uint8_t duty) {
        if (gpio != 255) ledcWrite(gpio, duty);
    }
    void setOn() {
        if (gpio != 255) ledcWrite(gpio, PWM_MAX);
    }
    void setOff() {
        if (gpio != 255) ledcWrite(gpio, 0);
    }
};

// Globaler Pool: 8 Ausgänge, Kanäle 0..7
extern PwmChannel g_pwmChannels[8];

// ============================================================
//  Output-Switch  (fasst GPIO + PWM zusammen, ersetzt bsw0..bsw7)
// ============================================================

struct OutputChannel {
    uint8_t  gpio      = 255;
    uint8_t  pwmCh     = 255;
    bool     pwmMode   = false;   // false = digital, true = PWM
    uint8_t  duty      = 0;       // aktueller PWM-Wert (0..255)
    bool     state     = false;   // digitaler Zustand

    void init(uint8_t gpio_num, uint8_t ledc_ch) {
        gpio  = gpio_num;
        pwmCh = ledc_ch;
        ledcAttach(gpio_num, PWM_FREQ_HZ, PWM_BITS);
        ledcWrite(gpio_num, 0);
    }
    void setDigital(bool on) {
        state = on;
        if (pwmMode) {
            g_pwmChannels[pwmCh].setDuty(on ? duty : 0);
        } else {
            digitalWrite(gpio, on ? HIGH : LOW);
        }
    }
    void setPwmDuty(uint8_t d) {
        duty = d;
        if (pwmMode) g_pwmChannels[pwmCh].setDuty(d);
    }
    void setPwmMode(bool enable) {
        pwmMode = enable;
        if (!enable) {
            ledcDetach(gpio);
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, state ? HIGH : LOW);
        } else {
            ledcAttach(gpio, PWM_FREQ_HZ, PWM_BITS);
        }
    }
    void toggle() { setDigital(!state); }
};

extern OutputChannel g_outputs[8];

// ============================================================
//  Blinker  (ersetzt External::Blinker für die Status-LED)
// ============================================================

struct LedBlinker {
    enum class Event : uint8_t { Off, Steady, Slow, Medium, Fast };

    static inline uint8_t  gpio     = 255;
    static inline Event    mode     = Event::Off;
    static inline uint8_t  flashCnt = 1;
    static inline uint8_t  flashIdx = 0;
    static inline uint32_t ticker   = 0;

    static void setPin(uint8_t g) {
        gpio = g;
        pinMode(g, OUTPUT);
        digitalWrite(g, LOW);
    }
    static void event(Event e) { mode = e; ticker = 0; flashIdx = 0; }
    static void count(uint8_t n) { flashCnt = n; }

    // Muss alle 1 ms in ratePeriodic() aufgerufen werden
    static void ratePeriodic() {
        if (gpio == 255) return;
        ++ticker;
        switch (mode) {
        case Event::Off:
            digitalWrite(gpio, LOW);
            break;
        case Event::Steady:
            digitalWrite(gpio, HIGH);
            break;
        case Event::Fast:
            if (ticker % 100 == 0) digitalWrite(gpio, !digitalRead(gpio));
            break;
        case Event::Medium:
            // flashCnt Blitze, dann Pause
            _doFlash(200, 600);
            break;
        case Event::Slow:
            _doFlash(500, 1500);
            break;
        }
    }
private:
    static void _doFlash(uint32_t on_ms, uint32_t period_ms) {
        uint32_t t = ticker % period_ms;
        uint32_t flash_slot = on_ms / flashCnt;
        uint8_t  cur_flash  = t / flash_slot;
        uint32_t in_flash   = t % flash_slot;
        if (cur_flash < flashCnt && in_flash < on_ms / (flashCnt * 2)) {
            digitalWrite(gpio, HIGH);
        } else {
            digitalWrite(gpio, LOW);
        }
    }
};

// ============================================================
//  Button  (ersetzt External::Button)
// ============================================================

struct Button {
    enum class Press : uint8_t { None, Short, Long };

    static inline uint8_t  gpio     = 255;
    static inline uint32_t pressedAt = 0;
    static inline bool     wasDown   = false;
    static inline Press    pending   = Press::None;

    static void init(uint8_t g) {
        gpio = g;
        pinMode(g, INPUT_PULLUP);
    }
    static void ratePeriodic() {
        if (gpio == 255) return;
        bool down = (digitalRead(gpio) == LOW);
        if (down && !wasDown) {
            pressedAt = millis();
        } else if (!down && wasDown) {
            uint32_t dur = millis() - pressedAt;
            pending = (dur > 1000) ? Press::Long : Press::Short;
        }
        wasDown = down;
    }
    static Press event() {
        Press p = pending;
        pending = Press::None;
        return p;
    }
};

// ============================================================
//  Watchdog  (ersetzt WatchDog<WdgConfig>)
// ============================================================

#include <esp_task_wdt.h>

struct WatchDog {
    static constexpr uint32_t TIMEOUT_S = 2;
    static void init() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        // Core 3.x: esp_task_wdt_config_t erforderlich
        const esp_task_wdt_config_t wdtCfg = {
            .timeout_ms     = TIMEOUT_S * 1000,
            .idle_core_mask = 0,
            .trigger_panic  = true
        };
        esp_task_wdt_reconfigure(&wdtCfg);
        esp_task_wdt_add(nullptr);
#else
        // Core 2.x: alte API
        esp_task_wdt_init(TIMEOUT_S, true);
        esp_task_wdt_add(nullptr);
#endif
    }
    static void ratePeriodic() {
        esp_task_wdt_reset();
    }
};

// ============================================================
//  NVS Storage  (ersetzt Mcu::Stm::savecfg / .eeprom section)
// ============================================================

#include <Preferences.h>

namespace NVS {
    static Preferences prefs;
    static constexpr const char* NS  = "msw";
    static constexpr const char* KEY = "eeprom";

    template<typename T>
    static bool save(const T& data) {
        prefs.begin(NS, false);
        size_t written = prefs.putBytes(KEY, &data, sizeof(T));
        prefs.end();
        return written == sizeof(T);
    }

    template<typename T>
    static bool load(T& data) {
        prefs.begin(NS, true);
        bool ok = prefs.isKey(KEY);
        if (ok) {
            size_t got = prefs.getBytes(KEY, &data, sizeof(T));
            ok = (got == sizeof(T));
        }
        prefs.end();
        return ok;
    }
}

// ============================================================
//  Morse-Text Buffer
// ============================================================

#ifdef USE_MORSE
static constexpr size_t MORSE_TEXT_SIZE = 16 * 4;  // 4 Texte à 16 Zeichen
extern char g_morse_text[MORSE_TEXT_SIZE];
#endif

// ============================================================
//  Minimal IO::outl<debug> Ersatz
// ============================================================

#ifdef SERIAL_DEBUG
// Variadic debug output über Serial
template<typename... Args>
void debug_out(Args... args) {
    (Serial.print(args), ...);
    Serial.println();
}
#define DBG(...) debug_out(__VA_ARGS__)
#else
#define DBG(...) do {} while(0)
#endif
