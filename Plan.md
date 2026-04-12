# SML-Display – WT32-ETH01 V1.4 Projektplan

> **Hardware:** WT32-ETH01 V1.4 (ESP32-WROOM + LAN8720 Ethernet) + Nokia 5110 Display + IR-Lesekopf
> **Zähler:** efr SGM-D4-A920N
> **Version:** 2.0.0

---

## 1. Hardware-Übersicht

| Komponente | Beschreibung | Bemerkung |
|---|---|---|
| **WT32-ETH01 V1.4** | ESP32-WROOM + LAN8720A Ethernet, RJ45, CP2104 USB-UART | Hauptprozessor, LAN + WLAN |
| **Nokia 5110 Display** | 84x48 Pixel, monochrom, PCD8544 Controller | Lokale Anzeige |
| **IR-Lesekopf** | TTL-Ausgang (3.3V) für SML-Protokoll | Liest SML-Daten vom Zähler |
| **Taster (optional)** | GPIO34 für manuellen Display-Wechsel | Input-only Pin |

---

## 2. Schaltplan und Verkabelung

### 2.1 WT32-ETH01 ↔ Nokia 5110 Display

```
┌─────────────────────┐          ┌─────────────────────────┐
│   WT32-ETH01 V1.4   │          │   Nokia 5110 Display    │
│                     │          │                         │
│  3.3V ────────────────────────────── VCC                 │
│  GND  ────────────────────────────── GND                 │
│  GPIO14 ──────────────────────────── SCLK                │
│  GPIO15 ──────────────────────────── DIN                 │
│  GPIO32 ──────────────────────────── D/C                 │
│  GPIO33 ──────────────────────────── CE                  │
│  GPIO12 ──────────────────────────── RST                 │
│  GPIO5  (PWM) ────────────────────── BL (über 330Ω)     │
└─────────────────────┘          └─────────────────────────┘
```

### 2.2 WT32-ETH01 ↔ IR-Lesekopf (UART2)

```
┌─────────────────────┐          ┌─────────────────────────┐
│   WT32-ETH01 V1.4   │          │   IR-Lesekopf (TTL)     │
│                     │          │                         │
│  GPIO13 (UART2 RX) ──────────── TX (Lesekopf)            │
│  GPIO4  (UART2 TX) ──────────── RX (Lesekopf)            │
│  3.3V oder 5V ────────────────── VCC                     │
│  GND  ────────────────────────── GND                     │
└─────────────────────┘          └─────────────────────────┘
```

### 2.3 LAN (RJ45 – integriert)

Der WT32-ETH01 hat eine integrierte RJ45-Buchse mit magnetics. Einfach Ethernet-Kabel einstecken.

### 2.4 Gesamtschaltbild

```
                    ┌──────────────────────────────────┐
                    │      WT32-ETH01 V1.4             │
                    │                                  │
   ┌────────────┐   │  3.3V ───┐      ┌── GPIO13 RX   │── IR-Lesekopf TX
   │ Nokia 5110 │   │  GND  ───┤      │               │
   │ Display    │   │          │      │── GPIO4  TX   │── IR-Lesekopf RX
   │            │   │ GPIO14 ──┤SCLK  │               │
   │ SCLK ←─────┤   │ GPIO15 ──┤DIN   │  UART2        │
   │ DIN  ←─────┤   │ GPIO32 ──┤D/C   │  9600 8N1     │
   │ D/C  ←─────┤   │ GPIO33 ──┤CE    │               │
   │ RST  ←─────┤   │ GPIO12 ──┤RST   │  LAN8720      │
   │ BL   ←─────┤   │ GPIO5  ──┤BL    │  RJ45 ←───────┤── Ethernet
   │ VCC  ←─────┤   │          │      │               │
   │ GND  ←─────┤   │          │      │── GPIO34      │── Taster (optional)
   └────────────┘   │  USB     │      │               │
                    │  CP2104  │      │               │
                    │  (Debug) │      │               │
                    └──────────┴──────┴───────────────┘
```

---

## 3. Netzwerk-Logik

### 3.1 Verbindungsreihenfolge

```
┌─────────────────────────┐
│  Start: Ethernet (LAN)  │
└────────────┬────────────┘
             │
      ┌──────▼───────┐
      │ LAN verbunden│──── Ja ──→ LAN-IP verwenden
      │ (Timeout 15s)│             │
      └──────┬───────┘             ▼
             │ No            ┌─────────────┐
             ▼              │ MQTT starten│
      ┌─────────────┐       │ (wenn aktiv)│
      │ WiFi aktiv? │       └─────────────┘
      │ (Config)    │
      └──────┬──────┘
             │
      ┌──────▼──────────┐
      │ Nur LAN?        │──── Ja ──→ WiFi OFF
      │ (NET_MODE)      │
      └──────┬──────────┘
             │ Nein
             ▼
      ┌──────────────┐
      │ WiFi STA     │──── Verbunden ──→ WiFi-IP verwenden
      │ verbinden    │
      └──────┬───────┘
             │ Fehlgeschlagen
             ▼
      ┌───────────────────┐
      │ Warte 20 Minuten  │
      │ (AP_FALLBACK)     │
      └───────┬───────────┘
              │
              ▼
      ┌──────────────┐
      │ AP-Mode       │────→ 192.168.4.1
      │ SML-Display   │      Einrichtungsportal
      └──────────────┘
```

### 3.2 Netzwerk-Modi (konfigurierbar über Webinterface)

| Modus | Verhalten |
|---|---|
| **Auto** (Standard) | LAN zuerst, falls kein LAN → WiFi, nach 20 Min → AP |
| **Nur LAN** | Nur Ethernet, WiFi komplett deaktiviert |
| **Nur WiFi** | Nur WiFi (LAN ignorieren), nach 20 Min → AP |

### 3.3 WiFi Deaktivierung

Über das Webinterface (`/network`) kann WiFi explizit deaktiviert werden:
- Wenn deaktiviert: WiFi wird ausgeschaltet, kein AP-Fallback
- Nur LAN bleibt aktiv
- Zum Reaktivieren: Reset + Webinterface über LAN aufrufen

---

## 4. Display-Anzeige

### Statuszeile (immer sichtbar)

```
┌──────────────────────────────────────┐
│ 📶 🔌 192.168.178.50 ▶▶▶  🟢MQTT 1/3│
│ WiFi  LAN   Laufband-IP           Mqtt│
└──────────────────────────────────────┘
```

- **WiFi-Icon**: 📶 verbunden / □ nicht verbunden
- **LAN-Icon**: 🔌 verbunden / □ nicht verbunden
- **IP-Adresse**: Scrollt als Laufband (1 Pixel/100ms)
- **MQTT-Icon**: 🟢 verbunden / □ nicht verbunden
- **Seitenzahl**: 1/3, 2/3, 3/3

### Seiten (wechseln alle 5 Sek.)

**Seite 1 – Zählerstände:**
```
┌────────────────────┐
│ Bezug:             │
│ 12345.678 kWh     │
│ Lieferung:         │
│  5678.901 kWh     │
└────────────────────┘
```

**Seite 2 – Aktuelle Leistung:**
```
┌────────────────────┐
│ Leistung:          │
│ 1500 W            │
│ L1: 500W L2:600W  │
│                    │
└────────────────────┘
```

**Seite 3 – Spannung & Strom:**
```
┌────────────────────┐
│ U L1: 230.1V      │
│ U L2: 231.2V      │
│ U L3: 229.8V      │
│ I: 2.17A          │
└────────────────────┘
```

---

## 5. Webinterface

| Seite | URL | Inhalt |
|---|---|---|
| **Dashboard** | `/` | Status, Messwerte, Neustart |
| **WiFi** | `/wifi` | Netzwerke scannen, verbinden |
| **Netzwerk** | `/network` | Modus wählen, WiFi ein/aus |
| **MQTT** | `/mqtt` | Broker, Benutzer, Discovery |
| **Status** | `/status` | Verbindungsstatus |
| **API** | `/api/data` | JSON mit allen Messwerten |

**Erreichbar über:**
- LAN-IP (z.B. `http://192.168.178.50`)
- WiFi-IP (z.B. `http://192.168.178.51`)
- AP-Mode: `http://192.168.4.1`

---

## 6. Projektstruktur

```
WT32-ET01-SML/
├── Plan.md
├── WT32_ETH01_Pinout.txt      ← WT32-ETH01 Pinout-Referenz
├── platformio.ini             ← Board: wt32-eth01
│
├── include/
│   └── config.h               ← Pin-Definitionen (WT32-ETH01)
│
├── src/
│   ├── main.cpp               ← LAN + WiFi + AP-Fallback Logik
│   ├── config.h
│   ├── sml_reader.h/cpp       ← UART2 (GPIO13/4)
│   ├── display.h/cpp           ← Nokia 5110 (GPIO5/12/14/15/32/33)
│   ├── config_manager.h/cpp   ← LittleFS JSON (inkl. NetworkConfig)
│   ├── webserver.h/cpp        ← WiFiServer, LAN+WiFi Status
│   └── mqtt_client.h/cpp      ← PubSubClient + HA Discovery
```

---

## 7. Benötigte Bibliotheken

```ini
lib_deps =
    ; Display
    https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library.git
    https://github.com/adafruit/Adafruit-GFX-Library.git

    ; MQTT
    knolleary/PubSubClient@^2.8

    ; JSON
    bblanchon/ArduinoJson@^7.2.0

    ; SML Parser
    https://github.com/volkszaehler/libsml.git

    ; ETH.h, WiFi.h, DNSServer.h, LittleFS.h – im ESP32 Framework enthalten
```

---

## 8. Pin-Übersicht

| Funktion | GPIO | Bemerkung |
|---|---|---|
| **LAN8720** | 0,18,19,21,22,23,25,26,27 | Fest verdrahtet |
| **Display SCLK** | 14 | Frei |
| **Display DIN** | 15 | Strapping (HIGH beim Boot) |
| **Display D/C** | 32 | Auf Board beschriftet als "CFG" |
| **Display CE** | 33 | Auf Board beschriftet als "485_EN" |
| **Display RST** | 12 | Strapping (VDD_SDIO) |
| **Display BL** | 5 | PWM |
| **SML RX** | 13 | UART2 RX |
| **SML TX** | 4 | UART2 TX |
| **Taster** | 34 | Input-only |
| **Debug TX** | 1 | UART0 TX (CP2104) |
| **Debug RX** | 3 | UART0 RX (CP2104) |

---

## 9. Konfigurationsdateien (LittleFS)

| Datei | Inhalt |
|---|---|
| `/wifi_config.json` | SSID, Passwort, DHCP/statisch |
| `/mqtt_config.json` | Broker, Port, Benutzer, Discovery |
| `/display_config.json` | Kontrast, Helligkeit, Intervall |
| `/network_config.json` | Modus (Auto/LAN/WiFi), WiFi ein/aus |

---

*Erstellt am 11. April 2026. WT32-ETH01 V1.4 mit LAN8720 Ethernet.*
