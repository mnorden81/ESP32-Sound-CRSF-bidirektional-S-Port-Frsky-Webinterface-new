/*
 * output_ctrl.h
 * Ausgangssteuerung für ESP32
 *
 * Ersetzt:
 *   External::Morse::BlinkerWithPwm  (Morse-Ausgabe)
 *   External::BlinkerWithPwm         (Blink + PWM)
 *   External::Pattern::Generator     (Sequenz-Muster)
 *
 * Jeder der 8 Ausgänge kann:
 *   - Digital schalten (EIN/AUS)
 *   - PWM-Dimmen (0..255)
 *   - Blinken (konfigurierbare An/Aus-Zeit + Blitzanzahl)
 *   - Morse-Code ausgeben
 *   - Muster abspielen (Sequenz-Pattern)
 */

#pragma once

#include <Arduino.h>
#include <cstdint>
#include <array>
#include <cstring>
#include "eeprom_esp32.h"

// ============================================================
//  Globale PWM-Kanäle
// ============================================================

PwmChannel g_pwmChannels[8];
OutputChannel g_outputs[8];

#ifdef USE_MORSE
char g_morse_text[MORSE_TEXT_SIZE];
#endif

// ============================================================
//  Morse-Encoder
// ============================================================

#ifdef USE_MORSE

// Morse-Code Tabelle A-Z, 0-9
static const char* MORSE_TABLE[] = {
    ".-",   // A
    "-...", // B
    "-.-.", // C
    "-..",  // D
    ".",    // E
    "..-.", // F
    "--.",  // G
    "....", // H
    "..",   // I
    ".---", // J
    "-.-",  // K
    ".-..", // L
    "--",   // M
    "-.",   // N
    "---",  // O
    ".--.", // P
    "--.-", // Q
    ".-.",  // R
    "...",  // S
    "-",    // T
    "..-",  // U
    "...-", // V
    ".--",  // W
    "-..-", // X
    "-.--", // Y
    "--.."  // Z
};

static const char* MORSE_DIGITS[] = {
    "-----", // 0
    ".----", // 1
    "..---", // 2
    "...--", // 3
    "....-", // 4
    ".....", // 5
    "-....", // 6
    "--...", // 7
    "---..", // 8
    "----."  // 9
};

class MorseOutput {
public:
    // Timing-Parameter (aus EEProm, Einheit: ratePeriodic-Ticks à 1 ms)
    uint32_t dit_ms  = 150;
    uint32_t dah_ms  = 450;
    uint32_t gap_ms  = 150;   // Zeichen-intern
    uint32_t igap_ms = 300;   // Zwischen Zeichen
    uint32_t wgap_ms = 700;   // Zwischen Wörtern

    void setText(const char* txt) {
        strncpy(_text, txt, sizeof(_text) - 1);
        _text[sizeof(_text) - 1] = '\0';
        _reset();
    }

    void setTimings(const EEProm& ee) {
        dit_ms  = ee.morse_dit  * 50u;
        dah_ms  = ee.morse_dah  * 50u;
        gap_ms  = ee.morse_gap  * 50u;
        igap_ms = ee.morse_igap * 50u;
        wgap_ms = ee.morse_wgap * 50u;
    }

    // Gibt zurück: true = High (LED an), false = Low
    bool ratePeriodic() {
        if (_text[0] == '\0') return false;

        if (_timer > 0) {
            --_timer;
            return _outputHigh;
        }

        // Nächstes Symbol bestimmen
        _advance();
        return _outputHigh;
    }

private:
    char     _text[64]     = {};
    uint8_t  _charIdx      = 0;
    uint8_t  _symbolIdx    = 0;
    bool     _outputHigh   = false;
    uint32_t _timer        = 0;
    bool     _inGap        = false;   // gerade in Pause-Phase

    enum class Phase : uint8_t { Symbol, IntraGap, InterGap, WordGap, Restart };
    Phase _phase = Phase::Restart;

    void _reset() {
        _charIdx   = 0;
        _symbolIdx = 0;
        _phase     = Phase::Restart;
        _timer     = 0;
        _outputHigh= false;
    }

    const char* _getMorseForChar(char c) {
        c = toupper(c);
        if (c >= 'A' && c <= 'Z') return MORSE_TABLE[c - 'A'];
        if (c >= '0' && c <= '9') return MORSE_DIGITS[c - '0'];
        return nullptr;
    }

    void _advance() {
        // Aktuelles Zeichen
        if (_charIdx >= strlen(_text)) {
            // Text fertig: Neustart mit Wort-Pause
            _outputHigh = false;
            _timer = wgap_ms;
            _charIdx   = 0;
            _symbolIdx = 0;
            return;
        }

        char c = _text[_charIdx];

        if (c == ' ') {
            // Wort-Pause
            _outputHigh = false;
            _timer = wgap_ms;
            _charIdx++;
            _symbolIdx = 0;
            return;
        }

        const char* morse = _getMorseForChar(c);
        if (!morse) {
            _charIdx++;
            _symbolIdx = 0;
            return;
        }

        if (_symbolIdx < strlen(morse)) {
            if (_inGap) {
                // Intra-Symbol-Pause
                _outputHigh = false;
                _timer = gap_ms;
                _inGap = false;
            } else {
                // Punkt oder Strich ausgeben
                char sym = morse[_symbolIdx];
                _outputHigh = true;
                _timer = (sym == '-') ? dah_ms : dit_ms;
                _symbolIdx++;
                _inGap = true;
            }
        } else {
            // Zeichen fertig: Inter-Character-Pause
            _outputHigh = false;
            _timer = igap_ms;
            _charIdx++;
            _symbolIdx = 0;
            _inGap = false;
        }
    }
};

#endif // USE_MORSE

// ============================================================
//  Blink-Steuerung pro Ausgang
// ============================================================

struct BlinkState {
    enum class Mode : uint8_t { Off, On, Blink, Pwm
#ifdef USE_MORSE
        , Morse
#endif
    };

    Mode    mode        = Mode::Off;
    uint8_t pwmDuty     = 0;      // 0..255
    uint32_t onTime_ms  = 500;
    uint32_t offTime_ms = 500;
    uint8_t flashCount  = 1;

    uint32_t ticker     = 0;
    uint8_t  flashIdx   = 0;

#ifdef USE_MORSE
    MorseOutput morse;
#endif

    bool ratePeriodic() {
        // Gibt aktuellen Ausgangszustand zurück (true = HIGH)
        switch (mode) {
        case Mode::Off:
            return false;
        case Mode::On:
            return true;
        case Mode::Pwm:
            return false;   // PWM wird separat über g_outputs gesteuert
        case Mode::Blink:
            return _doBlink();
#ifdef USE_MORSE
        case Mode::Morse:
            return morse.ratePeriodic();
#endif
        }
        return false;
    }

private:
    bool _doBlink() {
        ++ticker;
        uint32_t period = (onTime_ms + offTime_ms) * flashCount + offTime_ms;
        uint32_t t = ticker % period;

        for (uint8_t i = 0; i < flashCount; i++) {
            uint32_t flash_start = i * (onTime_ms + offTime_ms);
            if (t >= flash_start && t < flash_start + onTime_ms) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================
//  Pattern Generator
// ============================================================

#ifdef USE_PATTERNS

class PatternGenerator {
public:
    uint8_t id = 0;

    void init(uint8_t genId) { id = genId; }

    void configure(const EEProm::Pattern& p) {
        _pattern    = p;
        _step       = 0;
        _ticker     = 0;
        _active     = (p.type != 0);
    }

    // Gibt 8-Bit-Maske der aktiven Ausgänge zurück
    uint8_t ratePeriodic() {
        if (!_active) return 0;

        ++_ticker;
        uint32_t period_ms = (_pattern.onTime * 10u) + (_pattern.offTime * 10u);
        if (period_ms == 0) period_ms = 1;

        if (_ticker >= period_ms) {
            _ticker = 0;
            _step   = (_step + 1) % 8;
        }

        // Im ON-Fenster: aktuellen Schritt aktivieren
        if (_ticker < (uint32_t)(_pattern.onTime * 10u)) {
            if (_step < 8) return (1 << _step);
        }
        return 0;
    }

    bool isActive() const { return _active; }

private:
    EEProm::Pattern _pattern{};
    uint8_t  _step   = 0;
    uint32_t _ticker = 0;
    bool     _active = false;
};

extern PatternGenerator g_patGen[4];
PatternGenerator g_patGen[4];

#endif // USE_PATTERNS

// ============================================================
//  Zentrale Output-Manager-Klasse
// ============================================================

class OutputManager {
public:
    BlinkState blink[8];

    void init(const uint8_t* gpioPins) {
        for (uint8_t i = 0; i < 8; i++) {
            g_outputs[i].init(gpioPins[i], i);
            pinMode(gpioPins[i], OUTPUT);
            digitalWrite(gpioPins[i], LOW);
        }
    }

    void applyEeprom(const EEProm& ee) {
        for (uint8_t i = 0; i < 8; i++) {
            const auto& out = ee.outputs[i];

            if (out.pwm > 0) {
                blink[i].mode    = BlinkState::Mode::Pwm;
                blink[i].pwmDuty = out.pwmDuty;
                g_outputs[i].setPwmMode(true);
                g_outputs[i].setPwmDuty(out.pwmDuty);
            } else if (out.blink > 0) {
                blink[i].mode        = BlinkState::Mode::Blink;
                blink[i].onTime_ms   = out.blinkOnTime  * 100u;
                blink[i].offTime_ms  = out.blinkOffTime * 100u;
                blink[i].flashCount  = out.flashCount;
                g_outputs[i].setPwmMode(false);
            } else {
                blink[i].mode = BlinkState::Mode::Off;
                g_outputs[i].setPwmMode(false);
            }

#ifdef USE_MORSE
            blink[i].morse.setTimings(ee);
#endif
        }
    }

    // Direktes Schalten aus CRSF-Daten (ch = Bitmaske 0..7)
    void setChannel(uint8_t ch, bool on) {
        if (ch >= 8) return;
        if (blink[ch].mode == BlinkState::Mode::Pwm) {
            g_outputs[ch].setPwmDuty(on ? blink[ch].pwmDuty : 0);
        } else if (blink[ch].mode == BlinkState::Mode::Blink || blink[ch].mode == BlinkState::Mode::Off) {
            if (on) {
                blink[ch].mode = BlinkState::Mode::Blink;
            } else {
                blink[ch].mode = BlinkState::Mode::Off;
                g_outputs[ch].setDigital(false);
            }
        }
    }

    // Alle Ausgänge auf definierten Failsafe-Zustand
    void applyFailsafe(const EEProm& ee) {
#ifdef USE_FAILSAFE
        switch (ee.failsafe_mode) {
        case 0: // hold: nichts tun
            break;
        case 1: // all_off
            for (uint8_t i = 0; i < 8; i++) {
                blink[i].mode = BlinkState::Mode::Off;
                g_outputs[i].setDigital(false);
            }
            break;
        case 2: // set pattern
            for (uint8_t i = 0; i < 8; i++) {
                bool on = (ee.failsafe_pattern[i] != 0);
                blink[i].mode = on ? BlinkState::Mode::On : BlinkState::Mode::Off;
                g_outputs[i].setDigital(on);
            }
            break;
        }
#endif
    }

#ifdef USE_MORSE
    void setMorseText(uint8_t ch, const char* text) {
        if (ch >= 8) return;
        blink[ch].mode = BlinkState::Mode::Morse;
        blink[ch].morse.setText(text);
        g_outputs[ch].setPwmMode(false);
    }
#endif

    // Muss alle 1 ms aufgerufen werden
    void ratePeriodic() {
        for (uint8_t i = 0; i < 8; i++) {
            bool state = blink[i].ratePeriodic();
            if (blink[i].mode != BlinkState::Mode::Pwm) {
                g_outputs[i].setDigital(state);
            }
        }
    }
};

extern OutputManager g_outputMgr;
OutputManager g_outputMgr;
