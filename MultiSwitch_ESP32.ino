/*
    ESP32-MultiSwitch  v1.42
   Basiert auf: ESP32-SBus-Switch 0.6   (Ziege-One / Der RC-Modellbauer)
   CRSF-Integration: ESP32-RC-Sound 0.43 (Ziege-One / Der RC-Modellbauer)

 /////Pin Belegung////
 GPIO 13: WiFi Pin (LOW = AP aktiv)
 GPIO 16: SBUS RX  /  CRSF RX
 GPIO 17: CRSF TX  (nur bei RC_System == 4)
 GPIO 18: Ausgang 1
 GPIO 19: Ausgang 2
 GPIO 21: Ausgang 3
 GPIO 22: Ausgang 4
 GPIO 23: Ausgang 5
 GPIO 25: Ausgang 6
 GPIO 26: Ausgang 7
 GPIO 27: Ausgang 8
 GPIO  2: Status LED (AUS = kein Signal, Blinken = verbunden)

 RC-System Werte:
   0 = FrSky          (SBUS)
   1 = FlySky         (SBUS)
   2 = ELRS normiert  (SBUS)
   3 = HoTT           (SBUS)
   4 = CRSF           (ExpressLRS / TBS CrossFire, bidirektional)

 ÄNDERUNGEN v0.14 (Verbesserungen):
   - MWprop-Unterstützung aus ESP32-RC-Sound übernommen:
     Lautstärke/PWM-Steuerung einzelner Ausgänge über CRSF-Kanal möglich.
   - wm_prop_value[8] Speicher für MWprop-Kanalwerte (duty 0–255).
   - nvsSave() jetzt nur noch bei tatsächlicher Änderung (nvsDirty-Flag
     war schon vorhanden, wird nun auch in webui korrekt gesetzt).
   - CRSF-Einkanal-Timeout erhöht auf 2000 ms (war 1000 ms) –
     robuster bei kurzen Signalunterbrechungen.
   - Output(): pwm_wert >= 300 Mapping auf benannte Konstante ausgelagert.
   - Konsistente Verwendung von static constexpr überall.
   - README.md aktualisiert.
*/

// ======== Bibliotheken ==========================================
/*
  Bolder Flight Systems SBUS  8.1.4   (für RC_System 0-3)
  CRSF_ESP32  Ziege-One               (für RC_System 4)
    https://github.com/Ziege-One/CRSF_ESP32
*/

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "sbus.h"
#include "crsf_esp32.h"
#include "blink_presets.h"

constexpr uint16_t Version = 142; // 1.42

// ======== SBUS-Schwellen (benannte Konstanten) ==================
static constexpr uint16_t SBUS_LOW_THRESHOLD  =  800;
static constexpr uint16_t SBUS_HIGH_THRESHOLD = 1200;
static constexpr uint16_t SBUS_PWM_MIN        =  200;
static constexpr uint16_t SBUS_PWM_MAX        = 1850;

// ======== PWM-Wert-Quellengrenze ================================
// pwm_wert[x] >= PWM_CHANNEL_OFFSET → Kanalgesteuert (Kanal = pwm_wert - Offset)
static constexpr int PWM_CHANNEL_OFFSET = 300;

// ======== MultiSwitch-Protokoll =================================
static constexpr uint8_t MWset       = 0x01;
static constexpr uint8_t MWprop      = 0x02;  // NEU v0.14: Proportional-Kanal
static constexpr uint8_t MWset4      = 0x07;
static constexpr uint8_t MWset4m     = 0x09;
static constexpr uint8_t Multiswitch = 0xA1;

// ======== PWM-Konfiguration =====================================
static constexpr int     PWM_FREQ       = 5000;
static constexpr int     PWM_RESOLUTION = 8;
static constexpr uint8_t PWM_MAX        = 255;

// ======== GPIO ==================================================
static constexpr uint8_t OutPin[8] = {18, 19, 21, 22, 23, 25, 26, 27};
static constexpr uint8_t WifiPin   = 13;
static constexpr uint8_t LedPin    = 2;

// ======== Ausgangszustand =======================================
int     pwm_wert[8]      = {255, 255, 255, 255, 255, 255, 255, 255};
int     mode[8]          = {0, 0, 0, 0, 0, 0, 0, 0};
int     Ausgang_Kanal[8] = {0, 1, 2, 3, 4, 5, 6, 7};
char    Ausgang_Name[8][17] = {
    "Ausgang 1","Ausgang 2","Ausgang 3","Ausgang 4",
    "Ausgang 5","Ausgang 6","Ausgang 7","Ausgang 8"
};
bool    Ausgang[8]       = {};

unsigned long previousTimeLED[8] = {};
bool          blinkOn[8]          = {};

// ======== RC-Bus ================================================
int  RC_System_boot = 0;

bfs::SbusRx   sbus_rx(&Serial1, 16, 17, true); // Serial1 fuer SBUS (GPIO16/17)
bfs::SbusData sbus_data;

CRSF crsf;

uint16_t channel_output[16] = {};
bool     BUS_OK              = false;

// ======== CRSF-Timeout ==========================================
// v0.14: auf 2000 ms erhöht – robuster bei kurzen Unterbrechungen
static constexpr unsigned long CRSF_TIMEOUT_MS = 2000;
unsigned long lastCrsfPacket = 0;

// ======== MWprop-Speicher (NEU v0.14) ===========================
// Speichert duty-Werte (0–255) für bis zu 8 proportionale CRSF-Kanäle.
// Verwendung: pwm_wert[x] = 200..207 → wm_prop_value[pwm_wert[x]-200]
uint8_t wm_prop_value[8] = {255, 255, 255, 255, 255, 255, 255, 255};

// ======== Konfiguration (aus NVS) ===============================
int RC_System      = 0;
int einkanal_mode  = 0;
int CRSF_Channel   = 4;
int modul_adress   = 0;

uint16_t einkanal_Data       = 0;
uint16_t einkanal_SpeicherWM = 0;
uint16_t Data                = 0;

// ======== WiFi ==================================================
char        g_wifi_ssid[33]     = "MultiSwitch";
char        g_wifi_pass[64]     = "123456789";
char        g_wifi_ip[16]       = "192.168.1.1";   // konfigurierbare AP-IP
const char* AP_IP_STR           = g_wifi_ip;        // Alias fuer Abwaertskompatibilitaet
char        g_device_name[24]   = "MultiSwitch";    // NEU V1.4: Geraetename im TBS Agent / LUA

// ======== NVS ===================================================
static bool nvsDirty = false;
static bool nvsSaveScheduled = false;
static unsigned long nvsSaveDueMs = 0;
static constexpr unsigned long NVS_SAVE_DEBOUNCE_MS = 500;

static void nvsWriteNow() {
    Preferences p;
    p.begin("msw", false);
    p.putInt("rc_sys",  RC_System);
    p.putInt("crsf_ch", CRSF_Channel);
    p.putInt("ek_mode", einkanal_mode);
    p.putInt("mod_adr", modul_adress);
    for (int i = 0; i < 8; i++) {
        char key[4];
        snprintf(key, sizeof(key), "ak%d", i); p.putInt(key, Ausgang_Kanal[i]);
        snprintf(key, sizeof(key), "pw%d", i); p.putInt(key, pwm_wert[i]);
        snprintf(key, sizeof(key), "mo%d", i); p.putInt(key, mode[i]);
        snprintf(key, sizeof(key), "nm%d", i); p.putString(key, Ausgang_Name[i]);
    }
    p.putString("ssid", g_wifi_ssid);
    p.putString("pass", g_wifi_pass);
    p.putString("ip",   g_wifi_ip);
    p.putString("dnam", g_device_name);   // NEU V1.4
    p.end();
    nvsDirty = false;
    nvsSaveScheduled = false;
    Serial.println("NVS gespeichert.");
}

static void nvsSave() {
    nvsDirty = true;
    nvsSaveScheduled = true;
    nvsSaveDueMs = millis() + NVS_SAVE_DEBOUNCE_MS;
}

static void nvsFlushPending() {
    if (nvsDirty || nvsSaveScheduled) {
        nvsWriteNow();
    }
}

static void nvsProcessPending() {
    if (!nvsSaveScheduled) return;
    if ((long)(millis() - nvsSaveDueMs) >= 0) {
        nvsWriteNow();
    }
}

static void nvsLoad() {
    Preferences p;
    p.begin("msw", true);
    if (!p.isKey("rc_sys")) { p.end(); return; }
    RC_System     = p.getInt("rc_sys",  0);
    CRSF_Channel  = p.getInt("crsf_ch", 4);
    einkanal_mode = p.getInt("ek_mode", 0);
    modul_adress  = p.getInt("mod_adr", 0);
    for (int i = 0; i < 8; i++) {
        char key[4];
        snprintf(key, sizeof(key), "ak%d", i); Ausgang_Kanal[i] = p.getInt(key, i);
        snprintf(key, sizeof(key), "pw%d", i); pwm_wert[i]      = p.getInt(key, 255);
        snprintf(key, sizeof(key), "mo%d", i); mode[i]          = p.getInt(key, 0);
        snprintf(key, sizeof(key), "nm%d", i);
        String nm = p.getString(key, Ausgang_Name[i]);
        strncpy(Ausgang_Name[i], nm.c_str(), 16); Ausgang_Name[i][16] = '\0';
    }
    String s    = p.getString("ssid", g_wifi_ssid);
    String pw   = p.getString("pass", g_wifi_pass);
    String ip   = p.getString("ip",   g_wifi_ip);
    String dnam = p.getString("dnam", g_device_name);  // NEU V1.4
    strncpy(g_wifi_ssid,    s.c_str(),    sizeof(g_wifi_ssid)-1);
    strncpy(g_wifi_pass,    pw.c_str(),   sizeof(g_wifi_pass)-1);
    strncpy(g_wifi_ip,      ip.c_str(),   sizeof(g_wifi_ip)-1);
    strncpy(g_device_name,  dnam.c_str(), sizeof(g_device_name)-1);
    p.end();
    nvsDirty = false;
    nvsSaveScheduled = false;
    Serial.println("NVS geladen.");
}

static void nvsReset() {
    RC_System = 0; CRSF_Channel = 4; einkanal_mode = 0; modul_adress = 0;
    for (int i = 0; i < 8; i++) {
        Ausgang_Kanal[i] = i; pwm_wert[i] = 255; mode[i] = 0;
        snprintf(Ausgang_Name[i], 17, "Ausgang %d", i + 1);
    }
    strncpy(g_wifi_ssid,   "MultiSwitch", sizeof(g_wifi_ssid)-1);
    strncpy(g_wifi_pass,   "123456789",   sizeof(g_wifi_pass)-1);
    strncpy(g_wifi_ip,     "192.168.1.1", sizeof(g_wifi_ip)-1);
    strncpy(g_device_name, "MultiSwitch", sizeof(g_device_name)-1);  // NEU V1.4
    nvsSave();
    nvsFlushPending();
}

void storageSave() { nvsFlushPending(); }

// ======== Status-LED ============================================
static unsigned long ledPrevMs = 0;
static bool          ledState  = false;

static void updateLed() {
    if (!BUS_OK) {
        digitalWrite(LedPin, LOW);
        return;
    }
    unsigned long now = millis();
    if (now - ledPrevMs >= 500) {
        ledPrevMs = now;
        ledState  = !ledState;
        digitalWrite(LedPin, ledState ? HIGH : LOW);
    }
}

// ======== CRSF-Failsafe =========================================
// Nur BUS_OK setzen – kein ledcWrite() hier!
// Output() schaltet Ausgänge kontrolliert ab.
static void checkCrsfTimeout() {
    if (RC_System_boot != 4) return;
    if (millis() - lastCrsfPacket > CRSF_TIMEOUT_MS) {
        BUS_OK = false;
    }
}

// ======== MultiSwitch-Decoder ===================================
static uint8_t compressSwitches(uint16_t state) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) result |= ((state >> (i * 2)) & 0x1) << i;
    return result;
}

static void einkanalFunctionCRSF() {
    uint8_t WMcode  = crsf.get_crfs_buffer(5);
    uint8_t command = crsf.get_crfs_buffer(6);
    if (WMcode != Multiswitch) return;
    switch (command) {
        case MWset4: {
            uint8_t address = crsf.get_crfs_buffer(7);
            if (address == (uint8_t)modul_adress) {
                uint16_t state = ((uint16_t)crsf.get_crfs_buffer(8) << 8)
                               |             crsf.get_crfs_buffer(9);
                einkanal_Data = compressSwitches(state);
            }
            break;
        }
        case MWset4m: {
            uint8_t count = min((uint8_t)crsf.get_crfs_buffer(7), (uint8_t)7);
            for (uint8_t i = 0; i < count; i++) {
                uint8_t address = crsf.get_crfs_buffer(8 + (3 * i));
                if (address == (uint8_t)modul_adress) {
                    uint16_t state = ((uint16_t)crsf.get_crfs_buffer(9  + (3*i)) << 8)
                                   |             crsf.get_crfs_buffer(10 + (3*i));
                    einkanal_Data = compressSwitches(state);
                }
            }
            break;
        }
        case MWset: {
            uint8_t address = crsf.get_crfs_buffer(7);
            if (address == (uint8_t)modul_adress)
                einkanal_Data = crsf.get_crfs_buffer(8);
            break;
        }
        // NEU v0.14: MWprop – setzt duty-Wert für proportionale PWM-Ausgänge
        case MWprop: {
            uint8_t address = crsf.get_crfs_buffer(7);
            if (address == (uint8_t)modul_adress) {
                uint8_t channel = crsf.get_crfs_buffer(8);
                uint8_t duty    = crsf.get_crfs_buffer(9); // 0-100%
                if (channel < 8) {
                    // MWprop sendet Prozent (0-100), PWM braucht 0-255
                    wm_prop_value[channel] = (uint8_t)((uint16_t)duty * 255 / 100);
                }
            }
            break;
        }
    }
}

// ======== SBUS-Einkanal-Decoder =================================
static void einkanalFunctionSBUS(uint16_t channel) {
    einkanal_Data = channel;
    if (einkanal_mode == 0) {
        switch (RC_System) {
            case 0: einkanal_Data /= 8; break;
            case 1:
                einkanal_Data = constrain(einkanal_Data, 206, 1837);
                einkanal_Data = ((einkanal_Data - 206) * 10 + 20) / 64;
                break;
            case 2: {
                float v = ((float)einkanal_Data - 172.0f + 1.5f) * 0.155677655677655f;
                einkanal_Data = (uint16_t)v;
                break;
            }
        }
    } else if (einkanal_mode >= 10) {
        uint16_t n = 0;
        switch (RC_System) {
            case 0: n = (channel >= 172) ? (channel - 172 + 1) : 0; break;
            case 1: n = (channel >= 220) ? (channel - 220)     : 0; n += (n >> 6); break;
            case 2: n = (channel >= 172) ? (channel - 172)     : 0; break;
            case 3: n = (channel >= 205) ? (channel - 205)     : 0; break;
        }
        uint8_t v       = (uint8_t)(n >> 4);
        uint8_t address = (v >> 4) & 0b11;
        uint8_t sw      = (v >> 1) & 0b111;
        uint8_t state   = v & 0b1;
        if (address == (uint8_t)(einkanal_mode - 10))
            bitWrite(einkanal_SpeicherWM, sw, state);
        einkanal_Data = einkanal_SpeicherWM;
    }
}

// ======== CRSF LUA Parameter-System =====================================
// Parameterstruktur (nach rcmultiswitchG030 Referenz):
//   0:  Root Folder, 1: Version Info, 2: Folder "Global"
//   3: Switch Addr, 4: CRSF Kanal
//   5+x*5+0..4: Folder + 4 Parameter pro Ausgang (x=0..7)
// ======== CRSF LUA / TBS Agent Parameter-System ========================
//
// Pro Ausgang (9 Eintraege inkl. Folder):
//   fi+0: Folder
//   fi+1: Sel "Ausgang-Quelle"  Einzelkanal;Kanal_L;Kanal_H
//   fi+2: U8  "Kanal Nr"        1-8 (Einzelkanal) oder 1-16 (Kanal_L/H)
//   fi+3: Sel "Blink"           Dauerlicht;Blinken
//   fi+4: U8  "Blink AN *10ms"  1-250
//   fi+5: U8  "Blink AUS *10ms" 1-250
//   fi+6: Sel "PWM Modus"       Festwert;MWprop (nur CRSF)
//   fi+7: U8  "PWM Festwert"    0-255
//   fi+8: Sel "Test"            Aus;Ein
// Global: 0=Root,1=Version,2=Folder,3=Switch Addr,4=CRSF Kanal
// Total: 5+8*9=77 Parameter (0..76)

static constexpr uint8_t CRSF_PARAM_COUNT = 76;

// ── CRSF-Geraeteadresse + Ping-Slot aus der WM-Adresse (uebernommen aus Soundmodul v1.24) ──
// Adresse = 0xC0 + modul_adress; Slot = (Adresse-0xC0)*2; Antwort erst im eigenen
// Zeit-Slot, damit sich die Geraeteantworten mehrerer Module nicht ueberlappen.
#define CRSF_SLOT_MS 5
static inline uint8_t  crsfAddrFromWM()  { return 0xC0 + (uint8_t)constrain(modul_adress, 0, 15); }
static inline uint16_t crsfSlotDelayMs() { return (uint16_t)((uint8_t)constrain(modul_adress, 0, 15) * 2) * CRSF_SLOT_MS; }

// Ausgang_Kanal[x] -> Quelle (0=Einzelkanal,1=Kanal_L,2=Kanal_H) + Kanalnummer (1-basiert)
static uint8_t getKanalQuelle(int x) {
    int k = Ausgang_Kanal[x];
    if (k < 20) return 0;
    if (k < 40) return 1;
    return 2;
}
static uint8_t getKanalNr(int x) {
    int k = Ausgang_Kanal[x];
    if (k < 20) return k + 1;       // Einzelkanal: Bit 1-8
    if (k < 40) return k - 20 + 1; // Kanal_L: Kanal 1-16
    return k - 40 + 1;              // Kanal_H: Kanal 1-16
}
static void setKanalQuelleNr(int x, uint8_t quelle, uint8_t nr) {
    uint8_t idx = (nr > 0) ? nr - 1 : 0; // 0-basiert
    if (quelle == 0) Ausgang_Kanal[x] = min(idx, (uint8_t)7);  // Einzelkanal: 0-7
    else if (quelle == 1) Ausgang_Kanal[x] = 20 + min(idx, (uint8_t)15); // Kanal_L
    else Ausgang_Kanal[x] = 40 + min(idx, (uint8_t)15);        // Kanal_H
}

// PWM-Modus: 0=Festwert, 1=MWprop
static uint8_t getPwmModus(int x) {
    return (pwm_wert[x] >= 200 && pwm_wert[x] <= 207) ? 1 : 0;
}

static void crsfSendParam(uint8_t idx) {
    char buf[32];
    const bool isCrsf = (RC_System_boot == 4);

    if (idx == 0) {
        crsf.send_param_response_CRSF_FOLDER(0, 0, "",
            {1,2,5,14,23,32,41,50,59,68});
    } else if (idx == 1) {
        crsf.send_param_response_CRSF_INFO(1, 0, "Version", "v1.42 ESP32");
    } else if (idx == 2) {
        crsf.send_param_response_CRSF_FOLDER(2, 0, "Global", {3,4});
    } else if (idx == 3) {
        crsf.send_param_response_CRSF_UINT8(3, 2, "Switch Addr",
            (uint8_t)modul_adress, 0, 20, "");
    } else if (idx == 4) {
        crsf.send_param_response_CRSF_UINT8(4, 2, "CRSF Kanal",
            (uint8_t)CRSF_Channel, 0, 15, "");
    } else if (idx >= 5 && idx <= 76) {
        uint8_t x   = (idx - 5) / 9;
        uint8_t sub = (idx - 5) % 9;
        uint8_t fi  = 5 + x * 9;
        uint8_t mH  = (mode[x] >> 8) & 0xFF;
        uint8_t mL  =  mode[x]        & 0xFF;
        snprintf(buf, sizeof(buf), "Ausgang %d", x + 1);

        switch (sub) {
        case 0: // Folder
            crsf.send_param_response_CRSF_FOLDER(fi, 0, buf,
                {(uint8_t)(fi+1),(uint8_t)(fi+2),(uint8_t)(fi+3),
                 (uint8_t)(fi+4),(uint8_t)(fi+5),(uint8_t)(fi+6),
                 (uint8_t)(fi+7),(uint8_t)(fi+8)});
            break;
        case 1: // Ausgang-Quelle
            crsf.send_param_response_CRSF_TEXT_SELECTION(fi+1, fi,
                "Ausgang-Quelle", "Einzelkanal;Kanal_L;Kanal_H",
                getKanalQuelle(x), 0, 2);
            break;
        case 2: // Kanal Nr
        {
            uint8_t q = getKanalQuelle(x);
            uint8_t maxNr = (q == 0) ? 8 : 16;
            snprintf(buf, sizeof(buf), (q == 0) ? "Bit Nr" : "RC-Kanal Nr");
            crsf.send_param_response_CRSF_UINT8(fi+2, fi,
                buf, getKanalNr(x), 1, maxNr, "");
            break;
        }
        case 3: // Blink
            crsf.send_param_response_CRSF_TEXT_SELECTION(fi+3, fi,
                "Blink", "Dauerlicht;Blinken",
                (mH > 0) ? 1 : 0, 0, 1);
            break;
        case 4: // Blink AN
            crsf.send_param_response_CRSF_UINT8(fi+4, fi,
                "Blink AN *10ms", mH ? mH : 10, 1, 250, "");
            break;
        case 5: // Blink AUS
            crsf.send_param_response_CRSF_UINT8(fi+5, fi,
                "Blink AUS *10ms", mL ? mL : 10, 1, 250, "");
            break;
        case 6: // PWM Modus
            if (isCrsf) {
                crsf.send_param_response_CRSF_TEXT_SELECTION(fi+6, fi,
                    "PWM Modus", "Festwert;MWprop (Kanal=Ausgang)",
                    getPwmModus(x), 0, 1);
            } else {
                crsf.send_param_response_CRSF_INFO(fi+6, fi,
                    "PWM Modus", "Festwert (SBUS)");
            }
            break;
        case 7: // PWM Festwert
            if (getPwmModus(x) == 1) {
                snprintf(buf, sizeof(buf), "MWprop Kanal %d (auto)", x+1);
                crsf.send_param_response_CRSF_INFO(fi+7, fi, "PWM Wert", buf);
            } else {
                crsf.send_param_response_CRSF_UINT8(fi+7, fi,
                    "PWM Festwert", (uint8_t)pwm_wert[x], 0, 255, "");
            }
            break;
        case 8: // Test
            crsf.send_param_response_CRSF_TEXT_SELECTION(fi+8, fi,
                "Test", "Aus;Ein", 0, 0, 1);
            break;
        }
    }
}

static void crsfWriteParam(uint8_t idx, uint8_t val) {
    const bool isCrsf = (RC_System_boot == 4);
    if (idx == 3) {
        modul_adress = val; crsf.setDeviceAddress(crsfAddrFromWM()); nvsSave();
    } else if (idx == 4) {
        CRSF_Channel = val; nvsSave();
    } else if (idx >= 5 && idx <= 76) {
        uint8_t x   = (idx - 5) / 9;
        uint8_t sub = (idx - 5) % 9;
        switch (sub) {
        case 1: // Ausgang-Quelle: Quelle wechseln, Kanalnummer beibehalten
            setKanalQuelleNr(x, val, getKanalNr(x));
            nvsSave(); break;
        case 2: // Kanal Nr
            setKanalQuelleNr(x, getKanalQuelle(x), val);
            nvsSave(); break;
        case 3: // Blink
            if (val == 0) {
                mode[x] = 0;
            } else {
                uint8_t mH = (mode[x] >> 8) & 0xFF;
                uint8_t mL =  mode[x]        & 0xFF;
                if (!mH) mH = 10;
                if (!mL) mL = 10;
                mode[x] = ((uint16_t)mH << 8) | mL;
            }
            nvsSave(); break;
        case 4: // Blink AN
            mode[x] = ((uint16_t)val << 8) | (mode[x] & 0xFF);
            nvsSave(); break;
        case 5: // Blink AUS
            mode[x] = (mode[x] & 0xFF00) | val;
            nvsSave(); break;
        case 6: // PWM Modus
            if (isCrsf) {
                if (val == 1) {
                    pwm_wert[x] = 200 + x; // MWprop
                    einkanal_Data |= (1 << x); // Ausgang einschalten
                } else if (getPwmModus(x) == 1) {
                    pwm_wert[x] = 255; // zurueck auf Festwert
                }
                nvsSave();
            }
            break;
        case 7: // PWM Festwert
            if (getPwmModus(x) == 0) { pwm_wert[x] = val; nvsSave(); }
            break;
        case 8: // Test
            if (val == 1) einkanal_Data |=  (1 << x);
            else          einkanal_Data &= ~(1 << x);
            break;
        }
    }
}

// ======== Webinterface ==========================================





#include "webui.h"

// ======== PWM-Duty für Ausgang x ermitteln (NEU v0.14) ==========
// Unterstützt drei Quellen:
//   pwm_wert[x] < 200          → Festwert (direkt als uint8_t)
//   pwm_wert[x] 200..207       → wm_prop_value[pwm_wert[x]-200] (MWprop)
//   pwm_wert[x] >= PWM_CHANNEL_OFFSET → Kanalgesteuert (SBUS/CRSF-Kanal)
static uint8_t getPwmDuty(int x) {
    int pv = pwm_wert[x];
    if (pv >= PWM_CHANNEL_OFFSET) {
        // Kanal-gemappter PWM-Wert
        return (uint8_t)map(channel_output[pv - PWM_CHANNEL_OFFSET],
                            SBUS_PWM_MIN, SBUS_PWM_MAX, 0, PWM_MAX);
    } else if (pv >= 200 && pv <= 207) {
        // MWprop-Kanal (NEU v0.14)
        return wm_prop_value[pv - 200];
    } else {
        return (uint8_t)pv;
    }
}

// ======== Output ================================================
// Zentrale Ausgabefunktion – EINZIGER Ort, der ledcWrite() auf OutPin[] aufruft.
static void Output(uint16_t d) {
    unsigned long now = millis();
    for (int x = 0; x < 8; x++) {

        if (g_manual_override[x]) {
            bool newState = g_manual_state[x];
            if (newState && !Ausgang[x]) {
                previousTimeLED[x] = now - 9999;
                blinkOn[x] = false;
            }
            Ausgang[x] = newState;

        } else if (!BUS_OK) {
            Ausgang[x] = false;

        } else {
            int k = Ausgang_Kanal[x];
            // MWprop-Modus: Ausgang immer AN (PWM-Wert steuert Helligkeit)
            if (pwm_wert[x] >= 200 && pwm_wert[x] <= 207) {
                Ausgang[x] = true;
            } else if (k < 20) {
                Ausgang[x] = bitRead(d, k);
            } else if (k < 40) {
                Ausgang[x] = (channel_output[k-20] < SBUS_LOW_THRESHOLD);
            } else if (k < 60) {
                Ausgang[x] = (channel_output[k-40] > SBUS_HIGH_THRESHOLD);
            }
        }

        if (Ausgang[x]) {
            int modeH = (mode[x] >> 8) & 0xFF;
            int modeL =  mode[x]        & 0xFF;
            if (modeH > 0 && modeL == 0) modeL = modeH;

            uint8_t duty = getPwmDuty(x);  // NEU: ausgelagerter Helfer

            if (modeH > 0) {
                if (!blinkOn[x]) {
                    if (now - previousTimeLED[x] >= (unsigned long)(10 * modeL)) {
                        previousTimeLED[x] = now;
                        blinkOn[x] = true;
                        ledcWrite(OutPin[x], duty);
                    }
                } else {
                    if (now - previousTimeLED[x] >= (unsigned long)(10 * modeH)) {
                        previousTimeLED[x] = now;
                        blinkOn[x] = false;
                        ledcWrite(OutPin[x], 0);
                    }
                }
            } else {
                blinkOn[x] = true;
                ledcWrite(OutPin[x], duty);
            }
        } else {
            blinkOn[x] = false;
            ledcWrite(OutPin[x], 0);
        }
    }
}

// ======== Setup =================================================
void setup() {
    Serial.begin(115200);
    nvsLoad();

    RC_System_boot = RC_System;

    if (RC_System_boot == 4) {
        crsf.init_crsf(&Serial2, 16, 17); // Serial2 fuer CRSF (GPIO16=RX, 17=TX)
        crsf.setDeviceAddress(crsfAddrFromWM());   // eindeutige CRSF-Adresse aus WM-Adresse
        Serial.printf("CRSF gestartet (RX=16, TX=17, 420000 Bd)  Geraeteadresse 0x%02X  Slot %d\n",
                      crsfAddrFromWM(), (uint8_t)constrain(modul_adress,0,15)*2);
    } else {
        sbus_rx.Begin();
        Serial.printf("SBUS gestartet (RX=16, RC-System=%d)\n", RC_System_boot);
    }

    for (int i = 0; i < 8; i++) {
        ledcAttach(OutPin[i], PWM_FREQ, PWM_RESOLUTION);
        ledcWrite(OutPin[i], 0);
    }

    pinMode(WifiPin, INPUT_PULLUP);
    pinMode(LedPin,  OUTPUT);
    digitalWrite(LedPin, LOW);

    webui_init();

    Serial.printf("MultiSwitch ESP32 v%d.%02d\n", Version / 100, Version % 100);
    Serial.printf("SSID: %s\n", g_wifi_ssid);
    Serial.printf("IP:   %s\n", AP_IP_STR);
}

// ======== Loop ==================================================
void loop() {

    nvsProcessPending();

    if (RC_System_boot == 4) {
        crsf.read_packets(0);
        for (int i = 0; i < 16; i++)
            channel_output[i] = crsf.get_crfs_channels(i);

        // BUS_OK: channel_output[0] > 0 zeigt aktives CRSF-Signal
        if (channel_output[0] > 0) {
            BUS_OK = true;
            lastCrsfPacket = millis();
        }
        checkCrsfTimeout();

        // Kein Telemetrie-Dummy: DEVICE_PING / LUA-Pakete werden durch
        // device_info und param-Antworten sichergestellt (keine Dummy-Daten noetig)

        // Bei CRSF: Einkanal-Steuerung NUR ueber WM-Protokoll (einkanalFunctionCRSF).
        // einkanalFunctionSBUS NICHT aufrufen - CRSF-Kanalwerte sind zu gross
        // fuer SBUS-WM-Dekodierung und wuerden einkanal_Data verfaelschen.
        // (Fehler in V1.1: ELSE-Zweig lief auch bei CRSF -> falsches Schalten)
        // ELRS LUA Script: Device Info Antwort auf DEVICE_PING
        if (crsf.getDeviceInfoReplyPending() && (millis() - crsf.getPingTime() >= crsfSlotDelayMs())) {
            crsf.setDeviceInfoReplyPending(false);
            char devName[24];
            snprintf(devName, sizeof(devName), "%s@%d", g_device_name, modul_adress);  // NEU V1.4
            crsf.send_device_info(devName, CRSF_PARAM_COUNT);
        }
        // ELRS LUA Script: Parameter lesen
        if (crsf.getDeviceReadReplyPending()) {
            crsf.setDeviceReadReplyPending(false);
            crsfSendParam(crsf.getParamReadIndex());
        }
        // ELRS LUA Script: Parameter schreiben + Read-Back (CRSF Standard)
        if (crsf.getDeviceWriteReplyPending()) {
            uint8_t widx = crsf.getParamWriteIndex();
            uint8_t wval = crsf.getParamWriteValue();
            crsf.setDeviceWriteReplyPending(false);
            crsfWriteParam(widx, wval);
            // Read-Back: geschriebenen Parameter direkt zuruecksenden
            crsfSendParam(widx);
        }
        // WM-Protokoll Befehle
        if (crsf.getDeviceCommandReplyPending()) {
            crsf.setDeviceCommandReplyPending(false);
            einkanalFunctionCRSF();
        }
        Data = einkanal_Data;

    } else {
        if (sbus_rx.Read()) {
            sbus_data = sbus_rx.data();

            if (sbus_data.failsafe) {
                BUS_OK = false;
                updateLed();
                webui_handle();
                return;
            }

            BUS_OK = true;
            for (int i = 0; i < 16; i++)
                channel_output[i] = sbus_data.ch[i];

            if (einkanal_mode != 999 && CRSF_Channel < 16)
                einkanalFunctionSBUS(sbus_data.ch[CRSF_Channel]);
            Data = einkanal_Data;
        }
    }

    Output(Data);
    updateLed();
    webui_handle();
}
