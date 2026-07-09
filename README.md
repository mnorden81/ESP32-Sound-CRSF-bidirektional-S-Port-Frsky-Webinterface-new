# ESP32-MultiSwitch v1.42

RC-gesteuerter 8-Kanal-Schalter für ESP32 mit Web-Interface.

## Änderungen v1.42 (übertragen aus Soundmodul v1.24)

Diese Version macht das Multiswitch für den **Multi-Device-Betrieb** am selben Empfänger tauglich:

- **Eindeutige CRSF-Geräteadresse** aus der Modul-Adresse (`0xC0 + modul_adress`) statt fest 0xC8 – dadurch kollidiert das Multiswitch nicht mehr mit anderen CRSF-Konfigurationsgeräten (z. B. dem Soundmodul), die sich sonst ebenfalls als 0xC8 melden
- **Ping-Answer-Slot** (`Slot = (Adresse−0xC0)×2`, Verzögerung `Slot × CRSF_SLOT_MS`) – die device_info-Antwort wird im eigenen Zeit-Slot gesendet, sodass sich die Antworten mehrerer Module auf dem Downlink nicht überlagern
- **Versionskonsistenz** auf `v1.42` (Firmware-Konstante, Web-UI-Header, README)

*Nicht übertragen (in dieser Version nicht nötig):* RxBt-Korrektur (das Multiswitch sendet keinen Batterie-Frame), Parameter-Lücken-Fix (IDs 0–76 bereits lückenlos), S.Port-Absturz-Schutz (kein S.Port vorhanden).

**Voraussetzung Multi-Device:** unterschiedliche Modul-Adressen je Gerät und ein Empfänger mit ExpressLRS ab Version 4.x.

---

## Änderungen v1.41

- **CRSF Parser-Fix**: Doppelschreiben des CRC-Bytes im RX-Parser entfernt
- **CRSF Init-Fix**: `init_crsf()` verwendet nun den übergebenen Serial-Port
- **CRSF API-Fix**: `send_command()` korrekt als Klassenmethode implementiert
- **Web-API robuster**: JSON-Strings werden sicher escaped, Eingaben werden valider geparst/validiert
- **NVS-Schreibschutz**: Konfigurationsänderungen werden gebündelt gespeichert (Debounce), Flush vor Neustart
- **Versionskonsistenz**: Firmware, Web-UI und README auf `v1.41` vereinheitlicht

## Pin-Belegung

| GPIO | Funktion |
|------|----------|
| 13 | WiFi-Pin (LOW = AP aktiv) |
| 16 | SBUS RX / CRSF RX |
| 17 | CRSF TX |
| 18–27 | Ausgänge 1–8 |
| 2 | Status-LED |

## Bibliotheken

- Bolder Flight Systems SBUS 8.1.4
- CRSF_ESP32 (https://github.com/Ziege-One/CRSF_ESP32)

## RC-System Werte

| Wert | System |
|------|--------|
| 0 | FrSky (SBUS) |
| 1 | FlySky (SBUS) |
| 2 | ELRS normiert (SBUS) |
| 3 | HoTT (SBUS) |
| 4 | CRSF (ELRS/TBS) |

## Web-Interface

IP: `192.168.1.1` (Standard)  
SSID: `MultiSwitch` / Passwort: `123456789`

### API-Endpunkte

| Methode | Pfad | Beschreibung |
|---------|------|--------------|
| GET | `/api/status` | Aktueller Status (Ausgänge, Kanäle, MWprop) |
| GET/POST | `/api/config` | Konfiguration lesen/schreiben |
| POST | `/api/switch` | Ausgang manuell schalten / freigeben |
| POST | `/api/reset` | Werkseinstellungen |
