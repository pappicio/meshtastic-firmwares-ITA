#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR || !MESHTASTIC_EXCLUDE_AIR_QUALITY_SENSOR

#pragma once
#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "MeshModule.h"
#include "NodeDB.h"
#include "detect/ScanI2C.h"
#include <utility>

#if !ARCH_PORTDUINO
class TwoWire;
#endif

#define DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS 1000
extern std::pair<uint8_t, TwoWire *> nodeTelemetrySensorsMap[_meshtastic_TelemetrySensorType_MAX + 1];

class TelemetrySensor
{
  protected:
    TelemetrySensor(meshtastic_TelemetrySensorType sensorType, const char *sensorName)
    {
        this->sensorName = sensorName;
        this->sensorType = sensorType;
        this->status = 0;
        this->address = 0; // Inizializzazione indirizzo
    }

    meshtastic_TelemetrySensorType sensorType = meshtastic_TelemetrySensorType_SENSOR_UNSET;
    unsigned status;
    bool initialized = false;
    uint8_t address; // Variabile per memorizzare l'indirizzo I2C

    int32_t initI2CSensor()
    {
        if (!status) {
            LOG_WARN("Can't connect to detected %s sensor. Remove from nodeTelemetrySensorsMap", sensorName);
            nodeTelemetrySensorsMap[sensorType].first = 0;
        } else {
            LOG_INFO("Opened %s sensor on i2c bus", sensorName);
            setup();
        }
        initialized = true;
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }

    // TODO: check is setup used at all?
    virtual void setup() {}

  public:
    virtual ~TelemetrySensor() {}

    virtual AdminMessageHandleResult handleAdminMessage(const meshtastic_MeshPacket &mp, meshtastic_AdminMessage *request,
                                                        meshtastic_AdminMessage *response)
    {
        return AdminMessageHandleResult::NOT_HANDLED;
    }

    const char *sensorName;
    
   
    virtual uint8_t getAddr() const { 
        // 1. Log iniziale: vediamo cosa c'è registrato in memoria
        LOG_DEBUG("I2C-CHECK: Richiesta addr per '%s'. Valore attuale in memoria: 0x%02x", 
                  (sensorName ? sensorName : "Sconosciuto"), address);

        if (this->address != 0x00) {
            return this->address; 
        }

        // 2. Se l'indirizzo è 0x00, iniziamo la procedura di recupero basata sul nome
        if (this->sensorName != nullptr) {
            LOG_DEBUG("I2C-FIX: Indirizzo nullo per '%s', avvio scansione del nome...", sensorName);
            uint8_t fixedAddr = 0x00;

            if (strstr(sensorName, "SHT") || strstr(sensorName, "GY-21")) {
                fixedAddr = (strstr(sensorName, "SHT3")) ? 0x44 : 0x40;
            } 
            else if (strstr(sensorName, "BME280") || strstr(sensorName, "BMP280") || strstr(sensorName, "DPS310")) {
                fixedAddr = 0x76;
            } 
            else if (strstr(sensorName, "BME680") || strstr(sensorName, "BMP085") || strstr(sensorName, "BMP180") || strstr(sensorName, "BMP3")) {
                fixedAddr = 0x77;
            }
            else if (strstr(sensorName, "AHT")) {
                fixedAddr = 0x38;
            }
            else if (strstr(sensorName, "MCP9808")) {
                fixedAddr = 0x18;
            }
            else if (strstr(sensorName, "PCT2075")) {
                fixedAddr = 0x37;
            }
            else if (strstr(sensorName, "SCD30")) {
                fixedAddr = 0x61;
            }
            else if (strstr(sensorName, "SCD4")) {
                fixedAddr = 0x62;
            }
            else if (strstr(sensorName, "SEN5")) {
                fixedAddr = 0x69;
            }

            // 3. Log dell'esito del fix
            if (fixedAddr != 0x00) {
                LOG_INFO("I2C-FIX: Recupero riuscito! '%s' forzato a 0x%02x", sensorName, fixedAddr);
                return fixedAddr;
            } else {
                LOG_WARN("I2C-FIX: Nome '%s' non riconosciuto nel database dei fix", sensorName);
            }
        } else {
            LOG_ERROR("I2C-FIX: Impossibile recuperare addr, sensorName e' NULL!");
        }

        // 4. Se siamo arrivati qui, ritorna comunque 0
        LOG_DEBUG("I2C-CHECK: Fallimento totale. Ritorno 0x00 per sensore orfano.");
        return this->address; 
    }

    // TODO: delete after migration
    bool hasSensor() { return nodeTelemetrySensorsMap[sensorType].first > 0; }

    // Functions to sleep / wakeup sensors that support it
    // These functions can save power consumption in cases like AQ
    virtual void sleep(){};
    virtual uint32_t wakeUp() { return 0; }
    virtual bool isActive() { return true; }  // Return true by default, override per sensor
    virtual bool canSleep() { return false; } // Return false by default, override per sensor
    virtual int32_t wakeUpTimeMs() { return 0; }
    virtual int32_t pendingForReadyMs() { return 0; }

#if WIRE_INTERFACES_COUNT > 1
    // Set to true if Implementation only works first I2C port (Wire)
    virtual bool onlyWire1() { return false; }
#endif
    virtual int32_t runOnce() { return INT32_MAX; }
    virtual bool isInitialized() { return initialized; }
    // TODO: is this used?
    virtual bool isRunning() { return status > 0; }

    virtual bool getMetrics(meshtastic_Telemetry *measurement) = 0;
    
virtual bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) { 
        if (dev) {
            // Proviamo l'accesso diretto alla proprietà address dell'oggetto address
            this->address = dev->address.address; 
        }
        return false; 
    };
};

#endif