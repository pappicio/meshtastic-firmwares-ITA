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
		
///////////////////////////////////////////////
        this->address = 0; // Inizializzazione indirizzo
///////////////////////////////////////////////

    }

    meshtastic_TelemetrySensorType sensorType = meshtastic_TelemetrySensorType_SENSOR_UNSET;
    unsigned status;
    bool initialized = false;

///////////////////////////////////////////////
    uint8_t address; // Variabile per memorizzare l'indirizzo I2C
///////////////////////////////////////////////

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
    
///////////////////////////////////////////////
String cleanName(const char* name) const {
    if (!name) return "";
    String cleaned = name;
    
    // Rimuove tutto lo sporco tipico dei sensori temperatura
    cleaned.replace("-", ""); 
    cleaned.replace("_", ""); 
    cleaned.replace(" ", ""); 
    cleaned.replace("/", ""); 
    cleaned.toUpperCase();
    cleaned.trim();

    // Taglia le X (SHTXX -> SHT, BMP3XX -> BMP3)
    while (cleaned.length() > 2 && cleaned.endsWith("X")) {
        cleaned = cleaned.substring(0, cleaned.length() - 1);
    }

    return cleaned;
}

   
virtual uint8_t getAddr() const {
        // 1. Priorità assoluta all'indirizzo forzato manualmente (se presente)
    if (this->address != 0) return this->address;

    if (this->sensorName) {
        // Puliamo il nome del sensore che la telemetria sta cercando
        String searchName = cleanName(this->sensorName);
        
        // Logga il punto di partenza
        LOG_DEBUG("I2C-SEARCH: Inizio ricerca per '%s' (Originale: %s)", searchName.c_str(), this->sensorName);

        for (auto const& [name, addr] : discoveredDevicesMap) {
            // Puliamo il nome trovato dallo scanner in questo ciclo
            String mapName = cleanName(name.c_str());

            // LOG atomico: vedi esattamente cosa viene confrontato in ogni secondo
            LOG_DEBUG("I2C-COMPARE: [%s] vs [%s]", searchName.c_str(), mapName.c_str());

            // Confronto a due vie: se uno contiene l'altro, abbiamo un vincitore
            if (searchName.length() > 0 && mapName.length() > 0) { // Sicurezza contro stringhe vuote
                if (searchName.indexOf(mapName) != -1 || mapName.indexOf(searchName) != -1) {
                    LOG_INFO("I2C-MATCH: Trovato! %s (0x%02x) corrisponde a %s", 
                             name.c_str(), addr, this->sensorName);
                    return addr;
                }
            }
        }
        LOG_WARN("I2C-FAIL: Nessun dispositivo in mappa per '%s'", this->sensorName);
    }
    return 0;
}
///////////////////////////////////////////////




 

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
    virtual bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) { return false; };
};

#endif