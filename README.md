# ESP32-RC-Sound v1.22

**Autor:** PiperPilot

ESP32-basiertes RC-Soundmodul mit I2S-Audioausgabe, SD-Karten-Wiedergabe, WLAN-Weboberfläche, CRSF-Parametersystem und optionaler LiPo-Telemetrie über S.Port (Hardware V4).

---

## Funktionsübersicht

- Motorklangsimulation (Start, Loop, Abschalten) mit drehzahlabhängiger Abspielgeschwindigkeit
- 8 frei konfigurierbare Zusatzsounds (WAV-Dateien von SD-Karte)
- Unterstützung für SBUS (FrSky, FlySky, ELRS SBUS, Hott) und CRSF/ELRS
- Einkanal-Multiplexing (MultiSwitch-Protokoll, WM0–WM3)
- Ebenenumschaltung (bis zu 7 Ebenen × 3 Gruppen)
- Alle vier Hardware-Versionen (V1, V2, V3, V4) in einem Firmware-Image
- CRSF-Parametersystem (75 Parameter, vollständige Konfiguration über TBS Agent)
- LiPo-Telemetrie über FrSky S.Port (Hardware V4)

---

## Neu in v1.22 (gegenüber v0.84)

- **Weboberfläche komplett neu** – vollständiges HTML/CSS/JS liegt als PROGMEM im Flash
  - Kein RAM-Allokation mehr pro Request (zuvor ~60 KB RAM pro Seitenaufruf)
  - `buildPage()` entfernt, `server.send_P()` streamt direkt aus dem Flash
  - Modernes Dark-Mode-UI (GitHub-Farbschema)
  - Responsive Design für Mobilgeräte
- Stabilitätsverbesserungen bei häufigen Webzugriffen

---

## Hardware-Versionen

| Version | GPIO-Pins (Eingänge) | Besonderheit |
|---|---|---|
| **V1** | 22, 0, 2, 4 | BUS + PWM-Eingang + GPIO-Pin + Einkanal |
| **V2** | 14, 27, 32, 33 | BUS + PWM-Eingang + GPIO-Pin + Einkanal |
| **V3** | – | Nur BUS-Kanal + Einkanal (SBUS/CRSF) |
| **V4** | – | Wie V3 + S.Port LiPo-Telemetrie |

---

## Pin-Belegung

### Alle Versionen

| GPIO | Funktion |
|---|---|
| 13 | WiFi-Aktivierung (LOW = AP-Modus) |
| 16 | CRSF RX / SBUS RX |
| 17 | CRSF TX |
| 05 | SD_CS |
| 18 | SD_CLK |
| 19 | SD_MISO |
| 23 | SD_MOSI |
| 21 | I2S_DOUT |
| 25 | I2S_LRC |
| 26 | I2S_BCLK |

### V1 – zusätzliche Eingänge

| GPIO | Funktion |
|---|---|
| 22, 0, 2, 4 | Input 3–6 (PWM/GPIO) |

### V2 – zusätzliche Eingänge

| GPIO | Funktion |
|---|---|
| 14, 27, 32, 33 | Input 3–6 (PWM/GPIO) |

### V4 – S.Port LiPo (zusätzlich)

| GPIO | Funktion |
|---|---|
| 32 | S.Port RX |
| 33 | S.Port TX |

> **Schaltung V4:** GPIO33 → Anode → [1N4148] → Kathode → S.Port Signal ← GPIO32

---

## Hardware-Voraussetzungen

| Komponente | Beschreibung |
|---|---|
| ESP32 | Board-Version 3.3.8 |
| SD-Karte | SPI-Anschluss |
| I2S-DAC/Verstärker | z. B. MAX98357A |
| RC-Empfänger | SBUS oder CRSF |
| LiPo-BMS mit S.Port | Nur für Hardware V4 |

---

## SD-Karten-Dateien

| Dateiname | Funktion |
|---|---|
| `loop.wav` | Motor-Laufgeräusch (Schleife) |
| `start.wav` | Motorstart-Geräusch |
| `shut.wav` | Motorabschalt-Geräusch |
| `sound1.wav` – `sound8.wav` | Zusatzsounds 1–8 |

---

## Bibliotheken

| Bibliothek | Version |
|---|---|
| Bolder Flight Systems SBUS | 8.1.4 |
| CRSF_ESP32 | https://github.com/Ziege-One/CRSF_ESP32 |

---

## Konfiguration

### Weboberfläche (alle Versionen)
1. GPIO 13 auf LOW ziehen
2. ESP32 startet als WLAN-Access-Point
3. Mit dem konfigurierten Netzwerk verbinden
4. Weboberfläche im Browser öffnen (modernes Dark-Mode-UI, mobilfreundlich)
5. Einstellungen anpassen und speichern

### TBS Agent / EdgeTX Lua (CRSF-Modus)
- 75 CRSF-Parameter in Ordnerstruktur: Motor, Sound 1–8, Einstellungen
- Hardware-Version umschaltbar (Neustart erforderlich)
- Sound-Test direkt aus dem Agent heraus möglich

---

## Quellentypen

| Code | Quelle | V1/V2 | V3/V4 |
|---|---|---|---|
| 0–15 | BUS-Kanal Low (1–16) | ✓ | ✓ |
| 20–35 | BUS-Kanal High (1–16) | ✓ | ✓ |
| 40–45 | PWM-Eingang Low (1–6) | ✓ | – |
| 50–55 | PWM-Eingang High (1–6) | ✓ | – |
| 60–65 | GPIO-Pin direkt (1–6) | ✓ | – |
| 70–77 | Einkanal-Bit (1–8) | ✓ | ✓ |
| 80–103 | Ebenen-Umschaltung | ✓ | ✓ |
| 200 | Dauerbetrieb | ✓ | ✓ |
| 999 | Deaktiviert | ✓ | ✓ |

---

## Versionshistorie

| Version | Highlights |
|---|---|
| v0.45 | NVS statt EEPROM, CRSF-Timeout-Failsafe |
| v0.70 | V1/V2/V3 vereint, CRSF-Parametersystem (75 Parameter) |
| v0.84 | Hardware V4, S.Port LiPo-Telemetrie, Einzelzellen-SoC |
| v1.22 | Weboberfläche als PROGMEM (Flash), ~60 KB RAM-Ersparnis pro Request, Dark-Mode-UI |
