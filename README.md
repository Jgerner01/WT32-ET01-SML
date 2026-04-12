# SML-Display WT32-ETH01

Ein ESP32-basierter SML-Zählerausleser mit Ethernet (LAN) und WiFi-Unterstützung, Nokia 5110 Display und MQTT-Integration.

## Hardware

- **Board:** WT32-ETH01 V1.4 (ESP32 + LAN8720 Ethernet)
- **Display:** Nokia 5110 (SPI)
- **Schnittstelle:** IR-Lesekopf über UART2 (SML-Protokoll)
- **Netzwerk:** Ethernet (LAN) + WiFi (AP/STA)

---

## Upload-Anleitung (Firmware flashen)

### Anschlüsse

| WT32-ETH01 | USB-Programmer / PC |
|------------|---------------------|
| **GND**    | GND                 |
| **5V**     | 5V                  |
| **RX0**    | TX (Programmer)     |
| **TX0**    | RX (Programmer)     |

> **WICHTIG:**
> - **NICHT** RX und TX auf der 3,3V-Seite anschließen – nur die 5V-Pins verwenden!
> - Für die Programmierung müssen **GND** und **IO0** (die 2 Pins nebeneinander) **kurzgeschlossen** werden, um den ESP32 in den Flash-Modus zu versetzen.
> - Nach dem Flashen den Kurzschluss entfernen und das Board neu starten.

### Ablauf

1. **Verkabelung herstellen** (GND, 5V, RX0↔TX, TX0↔RX)
2. **GND und IO0 kurzschließen** (Jumper oder Drahtbrücke)
3. **Stromversorgung einschließen** (ESP32 startet im Flash-Modus)
4. **Firmware flashen** (z.B. über PlatformIO: `pio run --target upload`)
5. **Kurzschluss entfernen** (GND ↔ IO0 Trennung aufheben)
6. **Board neu starten** (Strom kurz trennen und wieder verbinden)

---

## Pinbelegung

### Nokia 5110 Display (SPI)

| Display-Pin | GPIO | Beschreibung | Hinweis |
|-------------|------|--------------|---------|
| SCLK        | 14   | SPI Clock    | |
| DIN         | 15   | SPI MOSI     | Strapping-Pin, muss beim Boot HIGH sein |
| D/C         | 32   | Data/Command | Auf Board beschriftet als "CFG" |
| CE          | 33   | Chip Enable  | Auf Board beschriftet als "485_EN" |
| RST         | 12   | Reset        | Strapping-Pin, muss beim Boot LOW bleiben |
| BL          | 5    | Backlight (PWM) | 330 Ω Vorwiderstand empfohlen |
| VCC         | 3.3V | Versorgung   | **Kein 5V!** |
| GND         | GND  | Masse        | |

### IR-Lesekopf (UART2, SML-Protokoll)

| IR-Kopf-Pin | GPIO | Beschreibung |
|-------------|------|--------------|
| TX          | 13   | ESP32 RX (UART2) |
| RX          | 4    | ESP32 TX (UART2, optional) |
| VCC         | 3.3V oder 5V | je nach Lesekopf-Typ |
| GND         | GND  | |

> Baudrate: **9600 8N1**

### Taster (optional)

| Pin  | GPIO | Beschreibung |
|------|------|--------------|
| Taster | 34 | Input-only, gegen GND schalten (kein Pull-Up nötig) |

### Debug / Flash (UART0)

| Funktion      | GPIO | Beschreibung |
|---------------|------|--------------|
| TX (→ PC)     | 1    | Serieller Monitor / Logs |
| RX (← PC)     | 3    | Flash-Programmierung |
| Flash-Modus   | 0    | Bei Reset auf GND ziehen |
| Reset         | EN   | Kurz auf GND → Neustart |

### LAN8720 Ethernet (fest verdrahtet, nicht änderbar)

| Signal  | GPIO | Signal  | GPIO |
|---------|------|---------|------|
| REF_CLK | 0    | TX_EN   | 21   |
| MDIO    | 18   | TXD0    | 19   |
| MDC     | 23   | TXD1    | 22   |
| RXD0    | 25   | RXD1    | 26   |
| CRS_DV  | 27   |         |      |

> GPIO 0, 18, 19, 21, 22, 23, 25, 26, 27 sind durch Ethernet dauerhaft belegt.

---

## OTA-Update (Firmware über Netzwerk)

Das Gerät unterstützt zwei Methoden für Over-The-Air-Updates – ein Neuanschluss des USB-Adapters ist nicht nötig, solange das Gerät im Netzwerk erreichbar ist.

### Methode 1: Browser-Upload

1. Web-Oberfläche öffnen: `http://<IP-Adresse>`
2. Menü **Firmware** aufrufen
3. `.bin`-Datei auswählen (aus PlatformIO: `.pio/build/wt32-eth01/firmware.bin`)
4. **Firmware hochladen** klicken
5. Fortschrittsbalken beobachten – das Gerät startet nach erfolgreichem Upload automatisch neu

> Die `.bin`-Datei wird erzeugt durch: `pio run` (ohne Upload)

### Methode 2: PlatformIO OTA-Push

Voraussetzung: Gerät ist im Netzwerk erreichbar (LAN oder WiFi).

In `platformio.ini` für den OTA-Upload ergänzen:

```ini
upload_protocol = espota
upload_port     = 192.168.178.XX   ; IP-Adresse des Geräts
upload_flags    = --auth=sml_ota_2026
```

Dann wie gewohnt flashen:

```bash
pio run --target upload
```

PlatformIO überträgt die Firmware automatisch per UDP (Port 3232). Das Passwort lautet `sml_ota_2026`.

> Nach dem OTA-Update wechselt `platformio.ini` wieder zurück auf den USB-Upload, indem du `upload_protocol = esptool` und `upload_port = COMx` setzt.

---

## Netzwerk-Konfiguration

### Verbindungslogik

Das Gerät unterstützt folgende Netzwerkmodi:

| Modus | Beschreibung |
|-------|-------------|
| **Auto (LAN zuerst, dann WiFi)** | Ethernet wird priorisiert. Falls LAN nicht verfügbar, wird WiFi genutzt. |
| **Nur LAN** | Nur Ethernet wird verwendet, WiFi ist vollständig deaktiviert. |
| **Nur WiFi** | Nur WiFi wird verwendet, Ethernet ist deaktiviert. |

### Ersteinrichtung (WiFi)

1. **Ethernet anschließen** (empfohlen) oder warten, bis der **AP-Mode** startet:
   - Falls **kein LAN** verfügbar ist, startet das Gerät nach **20 Minuten** automatisch im **AP-Mode**.
   - **AP-Name:** `SML-Display`
   - **Passwort:** `12345678`
   - **IP-Adresse:** `192.168.4.1`

2. **Mit dem AP verbinden:** Im WLAN-Menü des PCs/Smartphones mit `SML-Display` verbinden.

3. **Web-Oberfläche öffnen:** Browser öffnen und `http://192.168.4.1` aufrufen.

4. **WiFi einrichten:**
   - Navigiere zu **WiFi** im Menü.
   - Verfügbare Netzwerke werden angezeigt (SSID anklicken).
   - SSID und Passwort eingeben und auf **Verbinden** klicken.
   - Das Gerät verbindet sich und speichert die Daten dauerhaft.

5. **Status prüfen:** Unter **Netzwerk** den Verbindungsstatus und die IP-Adresse einsehen.

### WiFi automatisch abschalten

Das WiFi kann dauerhaft deaktiviert werden, um Strom zu sparen oder aus Sicherheitsgründen:

**Methode 1 – Über die Web-Oberfläche:**
1. Navigiere zu **Netzwerk**.
2. Wähle **Nur LAN** (dann ist WiFi vollständig deaktiviert).
3. Oder klicke auf **WiFi deaktivieren** im gleichen Menü.
4. Einstellungen speichern.

**Methode 2 – Über die API:**
Ein POST-Request an `/network/save` mit:
```json
{
  "mode": 1,
  "wifiEnabled": false
}
```

> **Hinweis:** Wenn WiFi deaktiviert ist und kein LAN verfügbar ist, steht **kein Einrichtungsportal** (AP-Mode) mehr zur Verfügung. Stelle sicher, dass eine Ethernet-Verbindung besteht, bevor du WiFi deaktivierst!

### WiFi wieder aktivieren

Falls WiFi deaktiviert wurde, kann es jederzeit wieder aktiviert werden:
1. Navigiere zu **Netzwerk** in der Web-Oberfläche.
2. Klicke auf **WiFi aktivieren**.
3. Einstellungen speichern – das Gerät startet WiFi neu.

---

## Web-Oberfläche

Die integrierte Web-Oberfläche bietet folgende Funktionen:

| Seite | URL | Funktion |
|-------|-----|----------|
| **Dashboard** | `/` | SML-Messwerte, Netzwerkstatus, Neustart |
| **WiFi** | `/wifi` | Verfügbare Netzwerke scannen, WiFi einrichten |
| **Netzwerk** | `/network` | Modus wählen (Auto/LAN/WiFi), WiFi aktivieren/deaktivieren |
| **MQTT** | `/mqtt` | Broker-Konfiguration, Testverbindung, Publish-Intervall |
| **Firmware** | `/ota` | OTA-Update per Browser-Upload |
| **API** | `/api/data` | JSON mit allen aktuellen Messwerten |

---

## MQTT-Konfiguration

Über die Web-Oberfläche (**MQTT**-Menü) können folgende Parameter eingestellt werden:

- **Broker:** Adresse des MQTT-Servers (z.B. `192.168.1.100`)
- **Port:** Standard `1883`
- **Benutzer/Passwort:** Optional, für authentifizierte Broker
- **Client-ID:** Eindeutige Kennung (Standard: `sml_meter_001`)
- **Topic-Präfix:** Standard `sml_meter`
- **Publish-Intervall:** Wie oft Daten gesendet werden (in Sekunden)
- **HA Auto-Discovery:** Automatische Einbindung in Home Assistant

---

## Taster-Bedienung

Am Gerät befindlicher Taster (GPIO34):

| Aktion | Funktion |
|--------|----------|
| **Kurzer Druck** (< 3 Sek.) | Display-Seite wechseln |
| **Langer Druck** (> 3 Sek.) | Display-Seite zurücksetzen |

---

## Technische Daten

| Parameter | Wert |
|-----------|------|
| SML-Baudrate | 9600 8N1 |
| Display | Nokia 5110 (84×48 Pixel, PCD8544) |
| Display-Pins | SCLK=14, DIN=15, D/C=32, CE=33, RST=12, BL=5 |
| Ethernet-Chip | LAN8720A (GPIO 0,18,19,21,22,23,25,26,27) |
| SML UART2 | RX=GPIO13, TX=GPIO4 |
| Taster | GPIO34 (Input-only) |
| Flash | 4 MB, LittleFS für Konfiguration |
| Firmware-Version | 2.0.0 |
| AP-SSID | `SML-Display` |
| AP-Passwort | `12345678` |
| AP-IP | `192.168.4.1` |
| Hostname | `sml-display` |
| OTA-Passwort | `sml_ota_2026` |
| OTA-Port | 3232 (UDP, ArduinoOTA) |

---

## Entwicklung

### Voraussetzungen

- [PlatformIO](https://platformio.org/) (VS Code Extension)
- ESP32 Platform (espressif32)

### Build & Upload

```bash
# Build
pio run

# Upload (Board im Flash-Modus, siehe oben)
pio run --target upload

# Serial Monitor
pio device monitor --baud 115200
```

---

## Lizenz

Dieses Projekt ist Open Source.
