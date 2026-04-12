/**
 * mqtt_client.h - MQTT Client Interface mit Home Assistant Auto-Discovery
 */

#ifndef MQTT_CLIENT_WRAPPER_H
#define MQTT_CLIENT_WRAPPER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#include "config_manager.h"
#include "sml_reader.h"

// Callback-Typen
typedef void (*MqttStatusCallback)(bool connected);

class MqttClientManager {
public:
    MqttClientManager();
    ~MqttClientManager();

    /**
     * Initialisiert den MQTT-Client
     * @param config MQTT-Konfiguration
     * @return true wenn erfolgreich initialisiert
     */
    bool begin(const MqttConfig& config);

    /**
     * Muss regelmäßig aufgerufen werden (hält die Verbindung aufrecht)
     */
    void loop();

    /**
     * Veröffentlicht alle SML-Daten als MQTT-Nachrichten
     * @param smlData Aktuelle SML-Daten
     */
    void publishSmlData(const SmlData& smlData);

    /**
     * Sendet Home Assistant Auto-Discovery Konfiguration
     */
    void sendDiscoveryConfig();

    /**
     * Testet die MQTT-Verbindung
     * @return true wenn Verbindung erfolgreich
     */
    bool testConnection();

    /**
     * Gibt den Verbindungsstatus zurück
     */
    bool isConnected() const;

    /**
     * Setzt Callback für Statusänderungen
     */
    void setStatusCallback(MqttStatusCallback callback);

    /**
     * Trennt die MQTT-Verbindung
     */
    void disconnect();

private:
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    MqttConfig mqttConfig;
    MqttStatusCallback statusCb;
    bool connected;
    unsigned long lastPublish;
    bool discoverySent;

    /**
     * Stellt Verbindung zum Broker her
     */
    bool connect();

    /**
     * Sendet Discovery für einen einzelnen Sensor
     */
    void sendSensorDiscovery(const char* sensorId, const char* name, 
                              const char* deviceClass, const char* stateClass,
                              const char* unit);
};

#endif // MQTT_CLIENT_WRAPPER_H
