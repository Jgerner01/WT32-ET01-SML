/**
 * mqtt_client.cpp - MQTT Client Implementierung
 */

#include "mqtt_client.h"

MqttClientManager::MqttClientManager() 
    : mqttClient(nullptr), connected(false), lastPublish(0), 
      discoverySent(false), statusCb(nullptr) {
    memset(&mqttConfig, 0, sizeof(MqttConfig));
}

MqttClientManager::~MqttClientManager() {
    disconnect();
}

bool MqttClientManager::begin(const MqttConfig& config) {
    mqttConfig = config;
    
    if (mqttClient) {
        delete mqttClient;
        mqttClient = nullptr;
    }
    
    mqttClient = new PubSubClient(wifiClient);
    mqttClient->setServer(config.broker, config.port);
    mqttClient->setBufferSize(1024);  // Größerer Buffer für Discovery-Nachrichten
    
    connected = false;
    discoverySent = false;
    
    DEBUG_PRINTF("[MQTT] Initialisiert: Broker=%s:%d\n", config.broker, config.port);
    return true;
}

void MqttClientManager::loop() {
    if (!mqttClient || !mqttConfig.enabled) return;
    
    // Verbindung aufrechten
    if (!mqttClient->connected()) {
        if (connected) {
            // War verbunden, jetzt getrennt
            connected = false;
            discoverySent = false;
            DEBUG_PRINTLN("[MQTT] Verbindung verloren");
            if (statusCb) statusCb(false);
        }
        
        // Wiederverbinden
        connect();
    } else {
        if (!connected) {
            // Neu verbunden
            connected = true;
            DEBUG_PRINTLN("[MQTT] Verbunden");
            if (statusCb) statusCb(true);
            
            // Discovery senden (falls aktiviert)
            if (mqttConfig.autoDiscovery && !discoverySent) {
                sendDiscoveryConfig();
            }
        }
        
        // MQTT Loop (für eingehende Nachrichten)
        mqttClient->loop();
    }
}

bool MqttClientManager::connect() {
    if (!mqttClient) return false;
    
    // Status-Topic für Last Will
    String willTopic = String(mqttConfig.topicPrefix) + "/status";
    
    bool result;
    if (strlen(mqttConfig.username) > 0) {
        result = mqttClient->connect(
            mqttConfig.clientId,
            mqttConfig.username,
            mqttConfig.password,
            willTopic.c_str(),
            MQTT_QOS,
            MQTT_RETAIN,
            "offline"
        );
    } else {
        result = mqttClient->connect(
            mqttConfig.clientId,
            willTopic.c_str(),
            MQTT_QOS,
            MQTT_RETAIN,
            "offline"
        );
    }
    
    if (result) {
        // Online-Status senden
        mqttClient->publish(willTopic.c_str(), "online", MQTT_RETAIN);
    } else {
        DEBUG_PRINTF("[MQTT] Verbindung fehlgeschlagen (rc=%d), retry in %d ms\n", 
                     mqttClient->state(), MQTT_RECONNECT_INTERVAL);
    }
    
    return result;
}

void MqttClientManager::publishSmlData(const SmlData& smlData) {
    if (!connected || !mqttClient) return;
    
    String baseTopic = String(mqttConfig.topicPrefix) + "/sensors";
    
    for (int i = 0; i < smlData.valueCount; i++) {
        // OBIS-Code zu Topic-ID konvertieren
        // Aus "1-0:1.8.0*255" wird "1_0_1_8_0_255"
        String sensorId = smlData.values[i].obisCode;
        sensorId.replace("-", "_");
        sensorId.replace(":", "_");
        sensorId.replace(".", "_");
        sensorId.replace("*", "_");
        
        String topic = baseTopic + "/" + sensorId;
        String payload;
        payload += "{\"value\":";
        payload += String(smlData.values[i].value, 3);
        payload += ",\"unit\":\"";
        payload += smlData.values[i].unit;
        payload += "\",\"ts\":";
        payload += millis() / 1000;
        payload += "}";
        
        mqttClient->publish(topic.c_str(), payload.c_str(), MQTT_RETAIN);
    }
}

void MqttClientManager::sendDiscoveryConfig() {
    if (!connected || !mqttClient) return;
    
    DEBUG_PRINTLN("[MQTT] Sende Auto-Discovery Konfiguration");
    
    // Energie Bezug Gesamt
    sendSensorDiscovery("energy_import_total", "Strom Bezug Gesamt",
                        "energy", "total_increasing", "kWh");
    
    // Energie Einspeisung Gesamt
    sendSensorDiscovery("energy_export_total", "Strom Einspeisung Gesamt",
                        "energy", "total_increasing", "kWh");
    
    // Aktuelle Leistung
    sendSensorDiscovery("power_active", "Strom Leistung Aktuell",
                        "power", "measurement", "W");
    
    // Leistung je Phase
    sendSensorDiscovery("power_l1", "Leistung L1",
                        "power", "measurement", "W");
    sendSensorDiscovery("power_l2", "Leistung L2",
                        "power", "measurement", "W");
    sendSensorDiscovery("power_l3", "Leistung L3",
                        "power", "measurement", "W");
    
    // Spannung je Phase
    sendSensorDiscovery("voltage_l1", "Spannung L1",
                        "voltage", "measurement", "V");
    sendSensorDiscovery("voltage_l2", "Spannung L2",
                        "voltage", "measurement", "V");
    sendSensorDiscovery("voltage_l3", "Spannung L3",
                        "voltage", "measurement", "V");
    
    // Strom je Phase
    sendSensorDiscovery("current_l1", "Strom L1",
                        "current", "measurement", "A");
    sendSensorDiscovery("current_l2", "Strom L2",
                        "current", "measurement", "A");
    sendSensorDiscovery("current_l3", "Strom L3",
                        "current", "measurement", "A");
    
    // Netzfrequenz
    sendSensorDiscovery("frequency", "Netzfrequenz",
                        "frequency", "measurement", "Hz");
    
    discoverySent = true;
}

void MqttClientManager::sendSensorDiscovery(const char* sensorId, const char* name,
                                              const char* deviceClass, const char* stateClass,
                                              const char* unit) {
    String discoveryTopic = String(MQTT_TOPIC_DISCOVERY) + "/" + sensorId + "/config";
    
    String stateTopic = String(mqttConfig.topicPrefix) + "/sensors/";
    // OBIS-Code zu Topic-ID
    if (strcmp(sensorId, "energy_import_total") == 0) stateTopic += "1_0_1_8_0_255";
    else if (strcmp(sensorId, "energy_export_total") == 0) stateTopic += "1_0_2_8_0_255";
    else if (strcmp(sensorId, "power_active") == 0) stateTopic += "1_0_16_7_0_255";
    else if (strcmp(sensorId, "power_l1") == 0) stateTopic += "1_0_36_7_0_255";
    else if (strcmp(sensorId, "power_l2") == 0) stateTopic += "1_0_56_7_0_255";
    else if (strcmp(sensorId, "power_l3") == 0) stateTopic += "1_0_76_7_0_255";
    else if (strcmp(sensorId, "voltage_l1") == 0) stateTopic += "1_0_32_7_0_255";
    else if (strcmp(sensorId, "voltage_l2") == 0) stateTopic += "1_0_52_7_0_255";
    else if (strcmp(sensorId, "voltage_l3") == 0) stateTopic += "1_0_72_7_0_255";
    else if (strcmp(sensorId, "current_l1") == 0) stateTopic += "1_0_31_7_0_255";
    else if (strcmp(sensorId, "current_l2") == 0) stateTopic += "1_0_51_7_0_255";
    else if (strcmp(sensorId, "current_l3") == 0) stateTopic += "1_0_71_7_0_255";
    else if (strcmp(sensorId, "frequency") == 0) stateTopic += "1_0_14_7_0_255";
    else stateTopic += sensorId;
    
    String payload;
    payload += "{";
    payload += "\"name\":\"" + String(name) + "\",";
    payload += "\"state_topic\":\"" + stateTopic + "\",";
    payload += "\"unit_of_measurement\":\"" + String(unit) + "\",";
    payload += "\"device_class\":\"" + String(deviceClass) + "\",";
    payload += "\"state_class\":\"" + String(stateClass) + "\",";
    payload += "\"value_template\":\"{{value_json.value}}\",";
    payload += "\"device\":{";
    payload += "\"identifiers\":[\"" DEVICE_ID "\"],";
    payload += "\"name\":\"" DEVICE_NAME "\",";
    payload += "\"model\":\"" DEVICE_MODEL "\",";
    payload += "\"manufacturer\":\"" DEVICE_MANUFACTURER "\",";
    payload += "\"sw_version\":\"" FIRMWARE_VERSION "\"";
    payload += "}}";
    
    mqttClient->publish(discoveryTopic.c_str(), payload.c_str(), true);
    delay(100);  // Kurze Pause zwischen Discovery-Nachrichten
}

bool MqttClientManager::testConnection() {
    if (!mqttClient) return false;
    return connect();
}

bool MqttClientManager::isConnected() const {
    return connected && mqttClient && mqttClient->connected();
}

void MqttClientManager::setStatusCallback(MqttStatusCallback callback) {
    statusCb = callback;
}

void MqttClientManager::disconnect() {
    if (mqttClient && mqttClient->connected()) {
        String willTopic = String(mqttConfig.topicPrefix) + "/status";
        mqttClient->publish(willTopic.c_str(), "offline", MQTT_RETAIN);
        mqttClient->disconnect();
    }
    connected = false;
}
