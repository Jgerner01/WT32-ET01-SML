/**
 * config_manager.h - Konfigurationsspeicher (LittleFS)
 * Speichert WLAN-, MQTT-, Display- und Netzwerk-Einstellungen
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"

// Netzwerk-Modus
enum NetworkMode {
    NET_MODE_AUTO,       // LAN zuerst, falls kein LAN dann WLAN
    NET_MODE_LAN_ONLY,   // Nur LAN
    NET_MODE_WIFI_ONLY,  // Nur WLAN
    NET_MODE_WIFI_OFF    // WLAN deaktiviert
};

struct WifiConfig {
    char ssid[64];
    char password[64];
    bool dhcp;
    char ip[16];
    char gateway[16];
    char subnet[16];
};

struct MqttConfig {
    char broker[64];
    uint16_t port;
    char username[32];
    char password[32];
    char clientId[32];
    char topicPrefix[32];
    uint16_t publishInterval;
    bool autoDiscovery;
    bool enabled;
};

struct DisplayConfig {
    uint8_t contrast;
    uint8_t brightness;
    uint16_t pageInterval;
};

struct NetworkConfig {
    NetworkMode mode;
    bool wifiEnabled;       // WLAN global ein/aus
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    bool begin();

    bool loadWifiConfig(WifiConfig& config);
    bool saveWifiConfig(const WifiConfig& config);
    
    bool loadMqttConfig(MqttConfig& config);
    bool saveMqttConfig(const MqttConfig& config);
    
    bool loadDisplayConfig(DisplayConfig& config);
    bool saveDisplayConfig(const DisplayConfig& config);

    bool loadNetworkConfig(NetworkConfig& config);
    bool saveNetworkConfig(const NetworkConfig& config);
    
    bool deleteConfig(const char* filename);
    bool configExists(const char* filename);
    size_t getFreeSpace();

private:
    bool initialized;
};

#endif // CONFIG_MANAGER_H
