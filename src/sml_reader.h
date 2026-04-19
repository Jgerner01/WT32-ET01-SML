/**
 * sml_reader.h – SML-Lesekopf Interface
 */
#ifndef SML_READER_H
#define SML_READER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

#define MAX_OBIS_CODE_LEN 32

struct ObisValue {
    char obisCode[MAX_OBIS_CODE_LEN];
    float value;
    char unit[16];
    unsigned long lastUpdate;
};

struct SmlData {
    ObisValue values[SML_MAX_OBIS_VALUES];
    int valueCount;
    unsigned long lastMessageTime;
    bool isValid;
};

class SmlReader {
public:
    SmlReader();
    ~SmlReader();
    void begin();
    bool update();
    const SmlData& getData() const;
    const ObisValue* getObisValue(const char* obisCode) const;
    float getActivePower() const;
    float getImportTotal() const;
    float getExportTotal() const;
    void printDebug() const;

    const uint8_t* getRawBuffer() const { return lastRawMsg; }
    int getRawLen() const { return lastRawLen; }
    unsigned long getTotalBytesReceived() const { return totalBytesReceived; }

private:
    HardwareSerial* smlSerial;
    SmlData smlData;
    uint8_t rxBuffer[SML_RX_BUFFER_SIZE];
    int bufferPos;
    uint8_t lastRawMsg[SML_RX_BUFFER_SIZE];
    int lastRawLen;
    unsigned long totalBytesReceived;
    bool parseSmlMessage(const uint8_t* buffer, int length);
    void storeObisValue(const char* obisCode, float value, const char* unit);
    int findOrCreateObisEntry(const char* obisCode);
};

#endif
