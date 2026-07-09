/*
 * eeprom_esp32.h
 * EEProm-Datenstruktur portiert für ESP32 (Arduino)
 *
 * Identisch zur originalen eeprom.h, aber ohne STM32-spezifische Includes.
 * Persistenz erfolgt über NVS (Preferences), siehe hal_esp32.h → NVS::save/load
 */

#pragma once

#include <cstdint>
#include <array>
#include <cstring>

#define EEPROM_MAGIC 42

// Feature-Flags (werden aus der Hauptdatei vererbt)
// USE_MORSE, USE_FAILSAFE, USE_TELEMETRY, USE_PATTERNS, SERIAL_DEBUG

// Hilfsfunktion: String in Array kopieren (ersetzt etl::copy)
template<size_t N>
static inline void str_copy(const char* src, std::array<char, N>& dst) {
    size_t i = 0;
    while (i < N - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

struct EEProm {
    constexpr EEProm() {
        str_copy("Output 0", outputs[0].name);
        str_copy("Output 1", outputs[1].name);
        str_copy("Output 2", outputs[2].name);
        str_copy("Output 3", outputs[3].name);
        str_copy("Output 4", outputs[4].name);
        str_copy("Output 5", outputs[5].name);
        str_copy("Output 6", outputs[6].name);
        str_copy("Output 7", outputs[7].name);
#ifdef USE_MORSE
        str_copy("SOS", morse_texts[0]);
#endif
    }

    struct Output {
        uint8_t pwm         = 0;
        uint8_t pwmDuty     = 1;
        uint8_t pwmScale    = 0;
        uint8_t blink       = 0;
        uint8_t blinkOnTime = 1;
        uint8_t blinkOffTime= 2;
        uint8_t flashCount  = 1;
        std::array<char, 16> name = {};
    };

    uint8_t magic = EEPROM_MAGIC;

    // CRSF Adresse und Slot
    uint8_t crsf_address   = 0xC8;
    uint8_t response_slot  = 0;

#ifdef USE_TELEMETRY
    uint8_t telemetry = 1;
#else
    uint8_t telemetry = 0;
#endif

    // Spannungssensor-Kalibrierung
    uint8_t cells_id = 1;
    uint8_t temp_id  = 1;

    // MultiSwitch-Adresse
    uint8_t address = 0;

    // PWM-Globalwerte (Basisfrequenz-Scaler, 1–20 → 50–1000 Hz)
    uint8_t pwm1 = 10;
    uint8_t pwm2 = 10;
    uint8_t pwm3 = 10;
    uint8_t pwm4 = 10;

    // 8 Ausgangskonfigurationen
    std::array<Output, 8> outputs{};

#ifdef USE_MORSE
    // Morse-Texte: 4 Slots à 16 Zeichen
    std::array<std::array<char, 16>, 4> morse_texts{};
    uint8_t morse_dit  = 3;   // Einheit: systemTimer-Ticks × 50 ms → 150 ms
    uint8_t morse_dah  = 6;
    uint8_t morse_gap  = 3;
    uint8_t morse_igap = 3;
    uint8_t morse_wgap = 9;
#endif

#ifdef USE_PATTERNS
    struct Pattern {
        uint8_t type     = 0;
        std::array<uint8_t, 8> member{1, 2, 3, 4, 5, 6, 7, 8};
        uint8_t onTime   = 33;   // × 10 ms
        uint8_t offTime  = 1;
        uint8_t next_address = 3;
        uint8_t group    = 1;
    };
    std::array<Pattern, 4> pattern{};
#endif

#ifdef USE_FAILSAFE
    uint8_t failsafe_mode = 0;   // 0=hold, 1=all_off, 2=set
    std::array<uint8_t, 8> failsafe_pattern = {};
#endif
};

static_assert(sizeof(EEProm) < 4096, "EEProm exceeds NVS limit");
