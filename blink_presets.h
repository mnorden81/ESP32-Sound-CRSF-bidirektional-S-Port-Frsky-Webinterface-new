/*
 * blink_presets.h
 * Vordefinierte Blink-Modi für MultiSwitch ESP32
 *
 * Das mode[]-Array in MultiSwitch_ESP32.ino kodiert AN- und AUS-Zeit
 * in einem einzigen 16-Bit-Wert:
 *
 *   mode = (modeH << 8) | modeL
 *   AN-Zeit  [ms] = modeH * 10
 *   AUS-Zeit [ms] = modeL * 10
 *
 * modeH == 0  →  Dauerlicht (kein Blinken)
 *
 * Einbinden in MultiSwitch_ESP32.ino:
 *   #include "blink_presets.h"
 *
 * Verwendung z. B. in nvsReset() oder beim Initialisieren:
 *   mode[0] = BLINK_100MS;    // Ausgang 1: 100 ms AN / 100 ms AUS
 *   mode[1] = BLINK_STEADY;   // Ausgang 2: Dauerlicht
 */

#pragma once

// ============================================================
//  Hilfsmakro: baut den mode-Wert aus AN- und AUS-Zeit in ms
//  Werte werden auf ein Vielfaches von 10 ms gerundet.
// ============================================================
#define BLINK_MODE(on_ms, off_ms) \
    ( (uint16_t)(((on_ms) / 10u) << 8) | (uint16_t)((off_ms) / 10u) )

// ============================================================
//  Dauerlicht  (kein Blinken)
// ============================================================
static constexpr uint16_t BLINK_STEADY     = BLINK_MODE(   0,    0);  // 0x0000

// ============================================================
//  Schnelle Blink-Modi
// ============================================================
static constexpr uint16_t BLINK_50MS       = BLINK_MODE(  50,   50);  // 0x0101  50 ms / 50 ms
static constexpr uint16_t BLINK_100MS      = BLINK_MODE( 100,  100);  // 0x0202 100 ms / 100 ms  ← NEU
static constexpr uint16_t BLINK_100MS_200  = BLINK_MODE( 100,  200);  // 0x0204 100 ms AN / 200 ms AUS

// ============================================================
//  Standard-Blink-Modi
// ============================================================
static constexpr uint16_t BLINK_250MS      = BLINK_MODE( 250,  250);  // 0x0505
static constexpr uint16_t BLINK_500MS      = BLINK_MODE( 500,  500);  // 0x0A0A
static constexpr uint16_t BLINK_750MS      = BLINK_MODE( 750,  750);  // 0x0F0F
static constexpr uint16_t BLINK_1S         = BLINK_MODE(1000, 1000);  // 0x1414
static constexpr uint16_t BLINK_2S         = BLINK_MODE(2000, 2000);  // 0x2828

// ============================================================
//  Asymmetrische Modi  (kurzer Blitz, lange Pause)
// ============================================================
static constexpr uint16_t BLINK_FLASH_SLOW = BLINK_MODE( 100, 1900);  // 0x023C kurzer Blitz, 2 s Periode
static constexpr uint16_t BLINK_FLASH_MED  = BLINK_MODE( 100,  900);  // 0x0212 kurzer Blitz, 1 s Periode
static constexpr uint16_t BLINK_SOS        = BLINK_MODE(  50,  150);  // 0x0103 sehr schnell, SOS-ähnlich

// ============================================================
//  Preset-Tabelle  (für Schleifen oder UI-Mapping)
// ============================================================
struct BlinkPreset {
    uint16_t    value;
    const char* label;
};

static const BlinkPreset BLINK_PRESETS[] = {
    { BLINK_STEADY,     "Dauerlicht"              },
    { BLINK_50MS,       "Blink  50ms /  50ms"     },
    { BLINK_100MS,      "Blink 100ms / 100ms"     },   // ← NEU: 0,1 s AN / 0,1 s AUS
    { BLINK_100MS_200,  "Blink 100ms / 200ms"     },
    { BLINK_250MS,      "Blink 250ms / 250ms"     },
    { BLINK_500MS,      "Blink 500ms / 500ms"     },
    { BLINK_750MS,      "Blink 750ms / 750ms"     },
    { BLINK_1S,         "Blink 1,0 s / 1,0 s"    },
    { BLINK_2S,         "Blink 2,0 s / 2,0 s"    },
    { BLINK_FLASH_SLOW, "Blitz 100ms, 1,9 s Pause"},
    { BLINK_FLASH_MED,  "Blitz 100ms, 900ms Pause"},
    { BLINK_SOS,        "SOS-Modus (50/150ms)"    },
};
static constexpr uint8_t BLINK_PRESET_COUNT =
    (uint8_t)(sizeof(BLINK_PRESETS) / sizeof(BLINK_PRESETS[0]));

// ============================================================
//  Hilfsfunktion: Preset anhand des Wertes suchen
//  Gibt nullptr zurück wenn kein passender Preset gefunden.
// ============================================================
inline const BlinkPreset* findBlinkPreset(uint16_t value) {
    for (uint8_t i = 0; i < BLINK_PRESET_COUNT; i++) {
        if (BLINK_PRESETS[i].value == value) return &BLINK_PRESETS[i];
    }
    return nullptr;
}
