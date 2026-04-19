/**
 * config.h - Pin-Definitionen und globale Konstanten
 * SML Display Projekt - WT32-ETH01 V1.4 (ESP32 + LAN8720 Ethernet)
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// HARDWARE PIN-DEFINITIONEN (WT32-ETH01 V1.4)
// ============================================================

/*
 * LAN8720 Ethernet PHY (FEST VERDRAHTET, NICHT äNDERBAR):
 *   REF_CLK=GPIO0, MDIO=GPIO18, MDC=GPIO23, TX_EN=GPIO21,
 *   TXD0=GPIO19, TXD1=GPIO22, RXD0=GPIO25, RXD1=GPIO26, CRS_DV=GPIO27
 *
 * Blockierte GPIOs: 0, 18, 19, 21, 22, 23, 25, 26, 27
 */

// Ethernet PHY Konfiguration
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#define ETH_PHY_ADDR        1
#define ETH_PHY_MDC         23
#define ETH_PHY_MDIO        18
#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN
// ETH_PHY_POWER wird in pins_arduino.h definiert (GPIO16)

// Nokia 5110 Display (SPI)
#define PIN_DISPLAY_SCLK    14     // Frei nutzbar
#define PIN_DISPLAY_DIN     15     // Frei nutzbar (Strapping: HIGH beim Boot)
#define PIN_DISPLAY_DC      32     // Frei nutzbar (auf Board: CFG)
#define PIN_DISPLAY_CS      33     // Frei nutzbar (auf Board: 485_EN)
#define PIN_DISPLAY_RST     12     // Strapping-Pin (VDD_SDIO, muss LOW beim Boot)
#define PIN_DISPLAY_BL      5      // Frei nutzbar (PWM)

// IR-Lesekopf SML (UART2)
#define PIN_SML_RX          4      // GPIO4 (frei)
#define PIN_SML_TX          2      // GPIO2  (frei)
#define SML_BAUD_RATE       9600   // SML Standard-Baudrate

// Taster (optional, manueller Display-Wechsel)
#define PIN_BUTTON          34     // Input-only Pin, kein Pull-Up nötig
#define PIN_BUTTON_ENABLED  0      // 0 = deaktiviert (noch nicht bestückt)

// ============================================================
// SML PROTOKOLL KONSTANTEN
// ============================================================

#define SML_RX_BUFFER_SIZE  1024
#define SML_MAX_OBIS_VALUES 20

// Wichtige OBIS-Codes
#define OBIS_ENERGY_IMPORT_TOTAL    "1-0:1.8.0*255"
#define OBIS_ENERGY_EXPORT_TOTAL    "1-0:2.8.0*255"
#define OBIS_POWER_ACTIVE           "1-0:16.7.0*255"
#define OBIS_POWER_L1               "1-0:36.7.0*255"
#define OBIS_POWER_L2               "1-0:56.7.0*255"
#define OBIS_POWER_L3               "1-0:76.7.0*255"
#define OBIS_VOLTAGE_L1             "1-0:32.7.0*255"
#define OBIS_VOLTAGE_L2             "1-0:52.7.0*255"
#define OBIS_VOLTAGE_L3             "1-0:72.7.0*255"
#define OBIS_CURRENT_L1             "1-0:31.7.0*255"
#define OBIS_CURRENT_L2             "1-0:51.7.0*255"
#define OBIS_CURRENT_L3             "1-0:71.7.0*255"
#define OBIS_FREQUENCY              "1-0:14.7.0*255"

// ============================================================
// DISPLAY KONSTANTEN
// ============================================================

#define DISPLAY_CONTRAST        0x41   // Kontrastwert (65 dezimal)
#define DISPLAY_PAGE_INTERVAL   10000  // 10 Sekunden pro Seite
#define DISPLAY_BRIGHTNESS      128    // PWM Helligkeit (0-255)

#define DISPLAY_PAGE_COUNT      3
#define DISPLAY_PAGE_1          0
#define DISPLAY_PAGE_2          1
#define DISPLAY_PAGE_3          2

// ============================================================
// WIFI KONSTANTEN
// ============================================================

#define WIFI_HOSTNAME           "sml-display"
#define WIFI_RECONNECT_INTERVAL 10000  // 10 Sekunden
#define WIFI_CONNECT_TIMEOUT    20000  // 20 Sekunden Timeout
#define WIFI_AP_PASSWORD        "12345678"

// ============================================================
// ETHERNET KONSTANTEN
// ============================================================

#define ETH_CONNECT_TIMEOUT     15000  // 15s auf Ethernet-Verbindung warten

// ============================================================
// WEBSERVER KONSTANTEN
// ============================================================

#define WEB_SERVER_PORT         80
// AP-Fallback Timeout in webserver.h definiert

// ============================================================
// MQTT KONSTANTEN
// ============================================================

#define MQTT_DEFAULT_PORT       1883
#define MQTT_PUBLISH_INTERVAL   1000   // 1 Sekunde
#define MQTT_RECONNECT_INTERVAL 5000
#define MQTT_QOS                0
#define MQTT_RETAIN             true

#define MQTT_TOPIC_PREFIX       "sml_meter"
#define MQTT_TOPIC_BASE         MQTT_TOPIC_PREFIX "/sensors"
#define MQTT_TOPIC_DISCOVERY    "homeassistant/sensor/sml_meter"

// ============================================================
// KONFIGURATIONSSPEICHER (LittleFS)
// ============================================================

#define CONFIG_FILE_WIFI        "/wifi_config.json"
#define CONFIG_FILE_MQTT        "/mqtt_config.json"
#define CONFIG_FILE_DISPLAY     "/display_config.json"
#define CONFIG_FILE_NETWORK     "/network_config.json"

// ============================================================
// ALLGEMEINE KONSTANTEN
// ============================================================

#define DEVICE_ID               "sml_meter_001"
#define DEVICE_NAME             "SML Stromzähler"
#define DEVICE_MODEL            "EFR SGM-D4-A920N"
#define DEVICE_MANUFACTURER     "WT32-ETH01 SML Reader"
#define FIRMWARE_VERSION        "2.0.0"

// Debug
#define DEBUG_ENABLE            true
#define DEBUG_PRINTLN(msg)      if(DEBUG_ENABLE) Serial.println(msg)
#define DEBUG_PRINT(msg)        if(DEBUG_ENABLE) Serial.print(msg)
#define DEBUG_PRINTF(fmt, ...)  if(DEBUG_ENABLE) Serial.printf(fmt, ##__VA_ARGS__)

#endif // CONFIG_H
