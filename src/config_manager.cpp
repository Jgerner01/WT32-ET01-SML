/**
 * config_manager.cpp – LittleFS JSON Speicherung
 */

#include "config_manager.h"
#include <ArduinoJson.h>

ConfigManager::ConfigManager() : initialized(false) {}
ConfigManager::~ConfigManager() {}

bool ConfigManager::begin() {
    if (!LittleFS.begin(false)) {
        DEBUG_PRINTLN("[Config] LittleFS fehlgeschlagen, formatiere...");
        if (!LittleFS.format()) {
            DEBUG_PRINTLN("[Config] LittleFS Format fehlgeschlagen!");
            return false;
        }
        if (!LittleFS.begin(false)) return false;
    }
    initialized = true;
    DEBUG_PRINTLN("[Config] LittleFS initialisiert");
    return true;
}

// ============================================================
// WIFI
// ============================================================

bool ConfigManager::loadWifiConfig(WifiConfig& config) {
    if (!configExists(CONFIG_FILE_WIFI)) {
        memset(&config, 0, sizeof(WifiConfig));
        config.dhcp = true;
        return false;
    }
    File file = LittleFS.open(CONFIG_FILE_WIFI, "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    memset(&config, 0, sizeof(WifiConfig));
    if (doc["ssid"].is<const char*>()) strncpy(config.ssid, doc["ssid"].as<const char*>(), sizeof(config.ssid)-1);
    if (doc["password"].is<const char*>()) strncpy(config.password, doc["password"].as<const char*>(), sizeof(config.password)-1);
    config.dhcp = doc["dhcp"] | true;
    if (doc["ip"].is<const char*>()) strncpy(config.ip, doc["ip"].as<const char*>(), sizeof(config.ip)-1);
    if (doc["gateway"].is<const char*>()) strncpy(config.gateway, doc["gateway"].as<const char*>(), sizeof(config.gateway)-1);
    if (doc["subnet"].is<const char*>()) strncpy(config.subnet, doc["subnet"].as<const char*>(), sizeof(config.subnet)-1);
    return true;
}

bool ConfigManager::saveWifiConfig(const WifiConfig& config) {
    JsonDocument doc;
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["dhcp"] = config.dhcp;
    doc["ip"] = config.ip;
    doc["gateway"] = config.gateway;
    doc["subnet"] = config.subnet;
    File file = LittleFS.open(CONFIG_FILE_WIFI, "w");
    if (!file) return false;
    serializeJsonPretty(doc, file);
    file.close();
    return true;
}

// ============================================================
// MQTT
// ============================================================

bool ConfigManager::loadMqttConfig(MqttConfig& config) {
    if (!configExists(CONFIG_FILE_MQTT)) {
        memset(&config, 0, sizeof(MqttConfig));
        config.port = MQTT_DEFAULT_PORT;
        config.publishInterval = MQTT_PUBLISH_INTERVAL / 1000;
        config.autoDiscovery = true;
        config.enabled = false;
        strncpy(config.clientId, DEVICE_ID, sizeof(config.clientId)-1);
        strncpy(config.topicPrefix, MQTT_TOPIC_PREFIX, sizeof(config.topicPrefix)-1);
        return false;
    }
    File file = LittleFS.open(CONFIG_FILE_MQTT, "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    memset(&config, 0, sizeof(MqttConfig));
    if (doc["broker"].is<const char*>()) strncpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker)-1);
    config.port = doc["port"] | MQTT_DEFAULT_PORT;
    if (doc["username"].is<const char*>()) strncpy(config.username, doc["username"].as<const char*>(), sizeof(config.username)-1);
    if (doc["password"].is<const char*>()) strncpy(config.password, doc["password"].as<const char*>(), sizeof(config.password)-1);
    if (doc["clientId"].is<const char*>()) strncpy(config.clientId, doc["clientId"].as<const char*>(), sizeof(config.clientId)-1);
    if (doc["topicPrefix"].is<const char*>()) strncpy(config.topicPrefix, doc["topicPrefix"].as<const char*>(), sizeof(config.topicPrefix)-1);
    config.publishInterval = doc["publishInterval"] | (MQTT_PUBLISH_INTERVAL/1000);
    config.autoDiscovery = doc["autoDiscovery"] | true;
    config.enabled = doc["enabled"] | false;
    return true;
}

bool ConfigManager::saveMqttConfig(const MqttConfig& config) {
    JsonDocument doc;
    doc["broker"] = config.broker;
    doc["port"] = config.port;
    doc["username"] = config.username;
    doc["password"] = config.password;
    doc["clientId"] = config.clientId;
    doc["topicPrefix"] = config.topicPrefix;
    doc["publishInterval"] = config.publishInterval;
    doc["autoDiscovery"] = config.autoDiscovery;
    doc["enabled"] = config.enabled;
    File file = LittleFS.open(CONFIG_FILE_MQTT, "w");
    if (!file) return false;
    serializeJsonPretty(doc, file);
    file.close();
    return true;
}

// ============================================================
// DISPLAY
// ============================================================

bool ConfigManager::loadDisplayConfig(DisplayConfig& config) {
    if (!configExists(CONFIG_FILE_DISPLAY)) {
        config.contrast = DISPLAY_CONTRAST;
        config.brightness = DISPLAY_BRIGHTNESS;
        config.pageInterval = DISPLAY_PAGE_INTERVAL;
        return false;
    }
    File file = LittleFS.open(CONFIG_FILE_DISPLAY, "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;
    config.contrast = doc["contrast"] | DISPLAY_CONTRAST;
    config.brightness = doc["brightness"] | DISPLAY_BRIGHTNESS;
    config.pageInterval = doc["pageInterval"] | DISPLAY_PAGE_INTERVAL;
    return true;
}

bool ConfigManager::saveDisplayConfig(const DisplayConfig& config) {
    JsonDocument doc;
    doc["contrast"] = config.contrast;
    doc["brightness"] = config.brightness;
    doc["pageInterval"] = config.pageInterval;
    File file = LittleFS.open(CONFIG_FILE_DISPLAY, "w");
    if (!file) return false;
    serializeJsonPretty(doc, file);
    file.close();
    return true;
}

// ============================================================
// NETWORK (neu)
// ============================================================

bool ConfigManager::loadNetworkConfig(NetworkConfig& config) {
    if (!configExists(CONFIG_FILE_NETWORK)) {
        config.mode = NET_MODE_AUTO;
        config.wifiEnabled = true;
        return false;
    }
    File file = LittleFS.open(CONFIG_FILE_NETWORK, "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;
    config.mode = (NetworkMode)(doc["mode"] | NET_MODE_AUTO);
    config.wifiEnabled = doc["wifiEnabled"] | true;
    return true;
}

bool ConfigManager::saveNetworkConfig(const NetworkConfig& config) {
    JsonDocument doc;
    doc["mode"] = (int)config.mode;
    doc["wifiEnabled"] = config.wifiEnabled;
    File file = LittleFS.open(CONFIG_FILE_NETWORK, "w");
    if (!file) return false;
    serializeJsonPretty(doc, file);
    file.close();
    return true;
}

// ============================================================
// HELFER
// ============================================================

bool ConfigManager::deleteConfig(const char* filename) {
    if (LittleFS.exists(filename)) return LittleFS.remove(filename);
    return false;
}

bool ConfigManager::configExists(const char* filename) {
    return LittleFS.exists(filename);
}

size_t ConfigManager::getFreeSpace() {
    return ESP.getFreeHeap();
}
