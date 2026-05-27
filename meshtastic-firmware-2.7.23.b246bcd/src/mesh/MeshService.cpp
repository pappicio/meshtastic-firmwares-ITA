#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif

#include "../concurrency/Periodic.h"
#include "BluetoothCommon.h" // needed for updateBatteryLevel, FIXME, eventually when we pull mesh out into a lib we shouldn't be whacking bluetooth from here
#include "MeshService.h"
#include "MessageStore.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "RTC.h"
#include "TypeConversions.h"
#include "graphics/draw/MessageRenderer.h"
#include "main.h"
#include "mesh-pb-constants.h"
#include "meshUtils.h"
#include "modules/NodeInfoModule.h"
#include "modules/PositionModule.h"
#include "modules/RoutingModule.h"
#include "power.h"
#include <assert.h>
#include <string>

///////////////////////////////////////////////
#include "../detect/ScanI2C.h"
#include <Wire.h>
#include "MeshService.h"

#include "modules/Telemetry/EnvironmentTelemetry.h"

#include <math.h> // Serve per la funzione powf (potenza)
///////////////////////////////////////////////

// ... altri include ...

///////////////////////////////////////////////
// 1. DICHIARAZIONI GLOBALI (Devono stare PRIMA di initHardwarePins)
// 1. PRIMA GLI INCLUDE DELLE LIBRERIE
#ifdef ONEWIRE_TEMP_PIN
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

#ifdef DHT_TEMP_PIN
  #include <DHT.h>
#endif
///////////////////////////////////////////////


///////////////////////////////////////////////
// 2. POI LE DICHIARAZIONI DELLE VARIABILI (Riga 32-33)
#ifdef ONEWIRE_TEMP_PIN
  OneWire _oneWire(ONEWIRE_TEMP_PIN);
  DallasTemperature _owSensors(&_oneWire);
#endif

#ifdef DHT_TEMP_PIN
  DHT _dht(DHT_TEMP_PIN, DHTTYPE);
#endif
///////////////////////////////////////////////
// --- DICHIARAZIONI GLOBALI UNICHE (PULITE) ---

// Diciamo che 'service' e 'fanTemp' esistono già in main.cpp

///////////////////////////////////////////////
extern float fanTemp;
extern float fanHum; 
 
extern EnvironmentTelemetryModule *environmentTelemetryModule;
///////////////////////////////////////////////


///////////////////////////////////////////////
// scanI2C esiste già nel firmware
extern ScanI2C *scanI2C; 
///////////////////////////////////////////////

///////////////////////////////////////////////
// Questo lo teniamo noi qui perché è specifico del nostro nuovo task
TaskHandle_t fanTaskHandle = NULL; 
///////////////////////////////////////////////

///////////////////////////////////////////////
// Prototipi
void checkInternalFan();
void checkAutoReboot();
///////////////////////////////////////////////

// --- BLOCCHI ESISTENTI (NON TOCCARLI) ---
#if ARCH_PORTDUINO

///////////////////////////////////////////////
#include "modules/StoreForwardModule.h"
///////////////////////////////////////////////

#include "PortduinoGlue.h"
#endif

/*
receivedPacketQueue - this is a queue of messages we've received from the mesh, which we are keeping to deliver to the phone.
It is implemented with a FreeRTos queue (wrapped with a little RTQueue class) of pointers to MeshPacket protobufs (which were
alloced with new). After a packet ptr is removed from the queue and processed it should be deleted.  (eventually we should move
sent packets into a 'sentToPhone' queue of packets we can delete just as soon as we are sure the phone has acked those packets -
when the phone writes to FromNum)

mesh - an instance of Mesh class.  Which manages the interface to the mesh radio library, reception of packets from other nodes,
arbitrating to select a node number and keeping the current nodedb.

*/

/* Broadcast when a newly powered mesh node wants to find a node num it can use

The algorithm is as follows:
* when a node starts up, it broadcasts their user and the normal flow is for all other nodes to reply with their User as well (so
the new node can build its node db)
*/

MeshService *service;

#define MAX_MQTT_PROXY_MESSAGES 16
static MemoryPool<meshtastic_MqttClientProxyMessage, MAX_MQTT_PROXY_MESSAGES> staticMqttClientProxyMessagePool;

#define MAX_QUEUE_STATUS 4
static MemoryPool<meshtastic_QueueStatus, MAX_QUEUE_STATUS> staticQueueStatusPool;

#define MAX_CLIENT_NOTIFICATIONS 4
static MemoryPool<meshtastic_ClientNotification, MAX_CLIENT_NOTIFICATIONS> staticClientNotificationPool;

Allocator<meshtastic_MqttClientProxyMessage> &mqttClientProxyMessagePool = staticMqttClientProxyMessagePool;

Allocator<meshtastic_ClientNotification> &clientNotificationPool = staticClientNotificationPool;

Allocator<meshtastic_QueueStatus> &queueStatusPool = staticQueueStatusPool;

#include "Router.h"

MeshService::MeshService()
#ifdef ARCH_PORTDUINO
    : toPhoneQueue(MAX_RX_TOPHONE), toPhoneQueueStatusQueue(MAX_RX_QUEUESTATUS_TOPHONE),
      toPhoneMqttProxyQueue(MAX_RX_MQTTPROXY_TOPHONE), toPhoneClientNotificationQueue(MAX_RX_NOTIFICATION_TOPHONE)
#endif
{
    lastQueueStatus = {0, 0, 16, 0};
}

void MeshService::init()
{
#if HAS_GPS
    if (gps)
        gpsObserver.observe(&gps->newStatus);
#endif

///////////////////////////////////////////////
initHardwarePins(); // La tua nuova sub-routine di boot

    // --- AGGIUNTA PER VENTOLA ---
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)

    if (fanTaskHandle == NULL) {
#ifdef ESP32
        // Logica per ESP32 (V3, ecc.)
        xTaskCreatePinnedToCore(
            this->fanControlTask,   // Funzione
            "FanControl",           // Nome
            4096,                   // Stack
            NULL,                   // Parametri
            1,                      // Priorità
            &fanTaskHandle,         // Handle
            1                       // Core 1
        );
#else
        // Logica per nRF52 (T114) e altri chip single core
        xTaskCreate(
            this->fanControlTask,   // Funzione
            "FanControl",           // Nome
            4096,                   // Stack
            NULL,                   // Parametri
            1,                      // Priorità
            &fanTaskHandle          // Handle
        );
#endif
        LOG_INFO("Task Ventola inizializzato correttamente");
    }
#endif
}
///////////////////////////////////////////////


int MeshService::handleFromRadio(const meshtastic_MeshPacket *mp)
{
    powerFSM.trigger(EVENT_PACKET_FOR_PHONE); // Possibly keep the node from sleeping

    nodeDB->updateFrom(*mp); // update our DB state based off sniffing every RX packet from the radio
    bool isPreferredRebroadcaster =
        IS_ONE_OF(config.device.role, meshtastic_Config_DeviceConfig_Role_ROUTER, meshtastic_Config_DeviceConfig_Role_ROUTER_LATE,
                  meshtastic_Config_DeviceConfig_Role_CLIENT_BASE);
    if (mp->which_payload_variant == meshtastic_MeshPacket_decoded_tag &&
        mp->decoded.portnum == meshtastic_PortNum_TELEMETRY_APP && mp->decoded.request_id > 0) {
        LOG_DEBUG("Received telemetry response. Skip sending our NodeInfo");
        //  ignore our request for its NodeInfo
    } else if (mp->which_payload_variant == meshtastic_MeshPacket_decoded_tag && !nodeDB->getMeshNode(mp->from)->has_user &&
               nodeInfoModule && !isPreferredRebroadcaster && !nodeDB->isFull()) {
        if (airTime->isTxAllowedChannelUtil(true)) {
            const int8_t hopsUsed = getHopsAway(*mp, config.lora.hop_limit);
            if (hopsUsed > (int32_t)(config.lora.hop_limit + 2)) {
                LOG_DEBUG("Skip send NodeInfo: %d hops away is too far away", hopsUsed);
            } else {
                LOG_INFO("Heard new node on ch. %d, send NodeInfo and ask for response", mp->channel);
                nodeInfoModule->sendOurNodeInfo(mp->from, true, mp->channel);
            }
        } else {
            LOG_DEBUG("Skip sending NodeInfo > 25%% ch. util");
        }
    }

    printPacket("Forwarding to phone", mp);
    sendToPhone(packetPool.allocCopy(*mp));

    return 0;
}

///////////////////////////////////////////////
// =========================================================================
// GESTIONE HARDWARE PERSONALIZZATA - START
// =========================================================================

/**
 * Inizializzazione dei Pin (Chiamata una sola volta nel setup)
 */
void MeshService::initHardwarePins() {  
    LOG_INFO("HARDWARE: Inizializzazione Pin personalizzati...");

    // Setup Relay e Ventola (Output)
    #ifdef FAN_RELAY_PIN
        pinMode(FAN_RELAY_PIN, OUTPUT);
        digitalWrite(FAN_RELAY_PIN, LOW);
    #endif

    #ifdef RELAY_1_PIN
        pinMode(RELAY_1_PIN, OUTPUT);
        digitalWrite(RELAY_1_PIN, LOW);
    #endif

    #ifdef RELAY_2_PIN
        pinMode(RELAY_2_PIN, OUTPUT);
        digitalWrite(RELAY_2_PIN, LOW);
    #endif

    // Setup Sensori (Inizializzazione una tantum)
    #ifdef ONEWIRE_TEMP_PIN
         _owSensors.begin(); 
         _owSensors.setResolution(9); // Molto più veloce!
         _owSensors.setWaitForConversion(false); // Non bloccare il thread
    #endif

    #ifdef DHT_TEMP_PIN
        _dht.begin();
    #endif

    #ifdef ANALOG_TEMP_PIN
        pinMode(ANALOG_TEMP_PIN, INPUT); 
    #endif
}

// --- LETTURA ONEWIRE (DS18B20) ---
#if defined(ONEWIRE_TEMP_PIN)
float readOneWireTemp() {

    // Nota: gli oggetti _oneWire e _owSensors devono essere definiti 
    // SOLO in alto nel file, non qui dentro!
    _owSensors.requestTemperatures();
    float t = _owSensors.getTempCByIndex(0);
    return (t == DEVICE_DISCONNECTED_C) ? -999.0f : t;
}
#endif 

// --- LETTURA DHT (11/22) ---
#if defined(DHT_TEMP_PIN)
float readDHTTemp() {
    float t = _dht.readTemperature();
    // Aggiorna la globale fanHum solo se richiesto esplicitamente
    #if defined(HAS_HUMIDITY) && HAS_HUMIDITY == 1
        float h = _dht.readHumidity();
        fanHum = (isnan(h)) ? 0.0f : h;
    #endif

    return (isnan(t)) ? -999.0f : t;
}
#endif

// --- LETTURA ANALOGICA (NTC 10k) ---
#if defined(ANALOG_TEMP_PIN)
float readAnalogTemp() {
    int raw = analogRead(ANALOG_TEMP_PIN);
    if (raw <= 0 || raw >= 4095) return -999.0f;

    // Calcola la resistenza attuale del sensore
    // Nota: Assumiamo che la resistenza di pull-up sia uguale a NTC_RES_NOMINAL
    float resistance = NTC_RES_NOMINAL / ((4095.0f / (float)raw) - 1.0f);

    // Steinhart-Hart super-compressa
    float t = log(resistance / NTC_RES_NOMINAL); // ln(R/Ro)
    t = (t / NTC_BETA) + (1.0f / 298.15f);       // + 1/To (25°C)

    return (1.0f / t) - 273.15f;                 // Risultato in Celsius
}
#endif
///////////////////////////////////////////////

// =========================================================================
// GESTIONE HARDWARE PERSONALIZZATA - END
// =========================================================================

//////////////////////////////////////////////
float readI2CTemp(uint8_t addr) {
 
    // 1. Invece di usare il puntatore esterno che ti dava errore (undefined reference)
    // usiamo il ServiceContext che è il "cuore" del firmware ed è sempre presente.
   // Il compilatore ci suggerisce che l'oggetto si chiama 'service'
    // Usiamo l'istanza statica della classe stessa
    if (EnvironmentTelemetryModule::instance != nullptr) {
        // La chiamiamo dritta in faccia
        EnvironmentTelemetryModule::instance->aggiornaTemperaturaBox();
        return fanTemp;
    }
    return -999.0f;
  
}
///////////////////////////////////////////////


///////////////////////////////////////////////
void checkInternalFan() {
    
    float currentTemp = -999.0f;
    fanHum = 0.0f;
    fanTemp = -999.0f;
    
    // Stringa per identificare il tipo di sensore nel log
    const char* sensorType = "NONE";

    // Selettore dinamico basato su cosa hai compilato
    #if defined(I2C_FAN_SENSOR_ADDR)
        currentTemp = readI2CTemp(I2C_FAN_SENSOR_ADDR);
        sensorType = "I2C";
    #elif defined(ONEWIRE_TEMP_PIN)
        currentTemp = readOneWireTemp();
        sensorType = "OneWire";
    #elif defined(DHT_TEMP_PIN)
        currentTemp = readDHTTemp();
        sensorType = "DHT";
    #elif defined(ANALOG_TEMP_PIN)
        currentTemp = readAnalogTemp();
        sensorType = "Analog";
    #endif

    if (currentTemp > -50.0f && currentTemp < 150.0f) {
        fanTemp = currentTemp;
    }

    // Log arricchito con il tipo di sensore
// Log arricchito con il tipo di sensore e umidità

#if defined(HAS_HUMIDITY) && HAS_HUMIDITY == 1 && !defined(FAN_TEMP_START)
    LOG_INFO("Fan Monitoraggio: [%s] Current Temp = %.2f C, Hum = %.2f %%\n", sensorType, currentTemp, fanHum);
#elif !defined(HAS_HUMIDITY) && !defined(FAN_TEMP_START)
    LOG_INFO("Fan Monitoraggio: [%s] Current Temp = %.2f C\n", sensorType, currentTemp);
#endif


// Verifichiamo se esiste almeno una logica di attivazione (Temp O Umidità)
#if (defined(FAN_TEMP_START) && defined(FAN_TEMP_STOP)) || (defined(FAN_HUM_START) && defined(FAN_HUM_STOP))

    // --- LOG DI MONITORAGGIO DINAMICO ---
    LOG_INFO("Fan Monitoraggio: [%s] T:%.1f H:%.1f", sensorType, currentTemp, fanHum);

    // Sicurezza: eseguiamo solo se il sensore non è scollegato (-999)
    if (currentTemp > -50.0f && currentTemp < 150.0f) {
        
        #ifdef FAN_RELAY_PIN
            pinMode(FAN_RELAY_PIN, OUTPUT);
            bool currentState = digitalRead(FAN_RELAY_PIN);
            bool deveStareAccesa = false;

            // --- 1. LOGICA TEMPERATURA (Se configurata) ---
            #if defined(FAN_TEMP_START) && defined(FAN_TEMP_STOP)
                if (currentTemp >= (float)FAN_TEMP_START) {
                    deveStareAccesa = true;
                } else if (currentTemp > (float)FAN_TEMP_STOP && currentState) {
                    deveStareAccesa = true; 
                }
            #endif

            // --- 2. LOGICA UMIDITÀ (Se configurata e presente) ---
            #if defined(FAN_HUM_START) && defined(FAN_HUM_STOP)
                if (fanHum >= (float)FAN_HUM_START) {
                    deveStareAccesa = true;
                } else if (fanHum > (float)FAN_HUM_STOP && currentState) {
                    deveStareAccesa = true;
                }
            #endif

            // --- 3. ATTUAZIONE FINALE ---
            if (deveStareAccesa && !currentState) {
                digitalWrite(FAN_RELAY_PIN, HIGH);
                LOG_INFO("VENTOLA: ATTIVATA");
            } 
            else if (!deveStareAccesa && currentState) {
                digitalWrite(FAN_RELAY_PIN, LOW);
                LOG_INFO("VENTOLA: DISATTIVATA");
            }
        #endif
    }

#else
    // Questo log scatta se FAN_TEMP_START o FAN_TEMP_STOP non sono stati definiti
    // Utile per monitorare la temperatura anche senza l'automatismo della ventola
    LOG_INFO("Fan Monitoraggio: [%s] - Temp: %.1f C, Hum: %.1f %% (Soglie non configurate)", 
              sensorType,
              currentTemp, 
              fanHum);

#endif

}
///////////////////////////////////////////////
 


///////////////////////////////////////////////
void checkAutoReboot() {
    // Esegui tutto solo se la macro è definita e maggiore di 0
#if defined(AUTO_REBOOT_DAYS) && (AUTO_REBOOT_DAYS > 0)

    // Calcolo soglia a 64bit (86.400.000 ms in un giorno)
    static const uint64_t threshold = (uint64_t)AUTO_REBOOT_DAYS * 86400000ULL;

    if (millis() > threshold) {
        LOG_INFO("GHOST: Uptime limit reached (%d days). Cleaning DB and Rebooting...", AUTO_REBOOT_DAYS);
        
        // --- 1. PULIZIA DATABASE (Essenziale per il reset totale) ---
        if (nodeDB) {
            LOG_INFO("GHOST: Resetting NodeDB (including favorites)...");
            // resetNodes(bool keepFavorites)
            // Passiamo 'false' per eliminare anche i preferiti
            nodeDB->resetNodes(false); 
            
            delay(1000); // Tempo per il salvataggio su disco
        }

        delay(1000); 

        // --- 2. REBOOT SPECIFICO PER ARCHITETTURA ---
        
        #if defined(ARCH_ESP32) || defined(ESP32)
            ESP.restart();

        #elif defined(ARCH_NRF52) || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52840)
            NVIC_SystemReset();

        #elif defined(ARCH_RP2040)
            rp2040.reboot();

        #elif defined(ARCH_PORTDUINO)
            // Su Linux/PC il comando reboot(0) spesso fallisce senza root.
            // exit(0) chiude il processo e il service manager (systemd) lo riavvia subito.
            LOG_DEBUG("GHOST: Portduino exit triggered.");
            exit(0); 

        #elif defined(ARCH_STM32WL)
            // Comando specifico per chip STM32 con LoRa integrato (es. Wio-E5)
            HAL_NVIC_SystemReset();

        #elif defined(ARCH_STM32)
            // Per altri STM32 generici
            NVIC_SystemReset();

        #else
            // Fallback universale per processori ARM (incluso il T114)
            NVIC_SystemReset();
        #endif

        // --- 3. ULTIMA SPIAGGIA (Se il reboot software si incanta) ---
        // Se dopo 5 secondi siamo ancora qui, forziamo un loop infinito 
        // per far scattare il Watchdog hardware
        delay(5000);
        while(1) { (void)0; }
    }
#endif
}


////////////////////////////////////////////////

#ifdef WIND_VELOCITY_PIN
extern volatile uint32_t wind_pulse_count; 
 

// Questa variabile ora conterrà i m/s pronti per essere passati a Meshtastic
float vento_salvato_globale = 0.0f; 

void aggiornaMeteoLocale(uint8_t ciclo_attuale) {

    // Scatta solo al ciclo 0 e al ciclo 3 (Intervallo preciso di 15 secondi)
    if (ciclo_attuale != 0 && ciclo_attuale != 3) {
        return; // Nei cicli intermedi esce subito tenendo l'ultimo vento calcolato
    }
 LOG_INFO("[MONITOR] Vento attuale (ultimi 5s): %.2f km/h", vento_salvato_globale);
    // --- ZONA CRITICA SICURA (Scatta ogni 15 secondi reali) ---
    noInterrupts();
    uint32_t pulses = wind_pulse_count;
    wind_pulse_count = 0; // Azzera il contatore hardware per i prossimi 15 secondi
    interrupts();
    // ---------------------------

    // Calcolo della frequenza media basata sui 15 secondi fissi del task
    float frequenza_hz = (float)pulses / 15.0f;
    
    // --- FORMULA LOGARITMICA APERTA (Da 0 a 150+ km/h) ---
    if (frequenza_hz > 0.0f) {
        // Calcola la velocità pura con curva di potenza + offset di attrito
        vento_salvato_globale = (ANEMOMETRO_GUADAGNO * powf(frequenza_hz, 0.92f)) + ANEMOMETRO_ATTRITO;
    } else {
        vento_salvato_globale = 0.0f; 
    }

    // Nel log moltiplichiamo al volo per 3.6f solo per la visualizzazione a schermo.
    // Così sul monitor seriale leggi i km/h e verifichi la taratura con la Ecowitt!
    LOG_INFO("[METEO-SUB] Finestra 15s conclusa (Ciclo %d). Impulsi: %d | Frequenza: %.2f Hz | Velocità: %.2f m/s (%.2f km/h)", 
             ciclo_attuale, pulses, frequenza_hz, vento_salvato_globale, (vento_salvato_globale * 3.6f));
}
#endif

#ifdef RAIN_SENSOR_PIN

// --- Variabili Globali ---
extern volatile uint32_t rain_pulse_count;

float pioggia_ultima_ora = 0.0f;     // Logica timeout 1h
float pioggia_totale_24h = 0.0f;     // Logica 24h
unsigned long ultima_goccia_ms = 0;
unsigned long inizio_finestra_24h = 0;
bool finestra_attiva_24h = false;

// --- Funzione Principale (Richiamata ogni 15 secondi) ---
void aggiornaPioggiaLocale(uint8_t ciclo_attuale) {

    
    // Scatta solo al ciclo 0 e al ciclo 3 (Intervallo preciso di 15 secondi)
    if (ciclo_attuale != 0 && ciclo_attuale != 3) {
        return; // Nei cicli intermedi esce subito tenendo l'ultimo vento calcolato
    }

    
        LOG_INFO("[MONITOR] Pioggia 1h: %.3f mm | Totale 24h: %.3f mm", pioggia_ultima_ora, pioggia_totale_24h);
    
    // 1. Lettura sicura interrupt
    noInterrupts();
    uint32_t pulses = rain_pulse_count;
    rain_pulse_count = 0; 
    interrupts();

    float mm_rilevati = (float)pulses * RAIN_GAUGE_FACTOR;

    // 2. Se è caduta pioggia, aggiorniamo gli stati
    if (mm_rilevati > 0) {
        // Aggiorna 1 ora
        pioggia_ultima_ora += mm_rilevati;
        ultima_goccia_ms = millis(); 

        // Aggiorna 24 ore
        if (!finestra_attiva_24h) {
            inizio_finestra_24h = millis();
            finestra_attiva_24h = true;
        }
        pioggia_totale_24h += mm_rilevati;
    }

    // 3. Gestione Timeout 1 ora (Reset automatico se non piove da 1 ora)
    if (pioggia_ultima_ora > 0 && (millis() - ultima_goccia_ms >= 3600000UL)) {
        pioggia_ultima_ora = 0.0f;
    }

    // 4. Gestione Finestra 24 ore (Reset dopo 24 ore di attività)
    if (finestra_attiva_24h && (millis() - inizio_finestra_24h >= 86400000UL)) {
        pioggia_totale_24h = 0.0f;
        finestra_attiva_24h = false;
    }

    // 5. Log
    if (mm_rilevati > 0) {
        LOG_INFO("[METEO] Pioggia 15s: %.3f | 1h: %.3f | 24h: %.3f", mm_rilevati, pioggia_ultima_ora, pioggia_totale_24h);
    }
}

#endif
///////////////////////////////////////////////

// Task di sistema per il loop di controllo
void MeshService::fanControlTask(void *pvParameters) {
#ifdef ESP32
    LOG_INFO("TASK_MAINTENANCE: Avviato su Core 1");
#else
    LOG_INFO("TASK_MAINTENANCE: Avviato (Single Core Mode)");
#endif

    // Delay iniziale per stabilizzazione sistema (Ripristinato a 15 secondi)
    vTaskDelay(pdMS_TO_TICKS(15000));

    // Variabile per contare i cicli da 5 secondi
    uint8_t cicli = 0;

    for (;;) {

        // =========================================================================
        // OGNI 5 SECONDI: Lettura Anemometro (Il valore perfetto)
        // =========================================================================
#ifdef WIND_VELOCITY_PIN
       aggiornaMeteoLocale(cicli); // <--- Passiamo il contatore dei cicli qui!
#endif

#ifdef RAIN_SENSOR_PIN
        aggiornaPioggiaLocale(cicli);
 
#endif


        // =========================================================================
        // OGNI 30 SECONDI: Controllo Ventola e Reboot (Eseguiti solo al ciclo 0)
        // =========================================================================
        if (cicli == 0) {
            checkInternalFan();
            checkAutoReboot();
        }

        // Avanziamo il contatore: resetta a 0 ogni 6 giri (6 cicli * 5s = 30 secondi)
        cicli = (cicli + 1) % 6;

        // Il cuore del task adesso pulsa ogni 5 secondi
        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
}
///////////////////////////////////////////////


/// Do idle processing (mostly processing messages which have been queued from the radio)
void MeshService::loop()
{
    if (lastQueueStatus.free == 0) { // check if there is now free space in TX queue
        meshtastic_QueueStatus qs = router->getQueueStatus();
        if (qs.free != lastQueueStatus.free)
            (void)sendQueueStatusToPhone(qs, 0, 0);
    }
    if (oldFromNum != fromNum) { // We don't want to generate extra notifies for multiple new packets
        int result = fromNumChanged.notifyObservers(fromNum);
        if (result == 0) // If any observer returns non-zero, we will try again
            oldFromNum = fromNum;
    }
}

/// The radioConfig object just changed, call this to force the hw to change to the new settings
void MeshService::reloadConfig(int saveWhat)
{
    // If we can successfully set this radio to these settings, save them to disk

    // This will also update the region as needed
    nodeDB->resetRadioConfig(); // Don't let the phone send us fatally bad settings

    configChanged.notifyObservers(NULL); // This will cause radio hardware to change freqs etc
    nodeDB->saveToDisk(saveWhat);
}

/// The owner User record just got updated, update our node DB and broadcast the info into the mesh
void MeshService::reloadOwner(bool shouldSave)
{
    // LOG_DEBUG("reloadOwner()");
    // update our local data directly
    nodeDB->updateUser(nodeDB->getNodeNum(), owner);
    assert(nodeInfoModule);
    // update everyone else and save to disk
    if (nodeInfoModule && shouldSave) {
        nodeInfoModule->sendOurNodeInfo();
    }
}

// search the queue for a request id and return the matching nodenum
NodeNum MeshService::getNodenumFromRequestId(uint32_t request_id)
{
    NodeNum nodenum = 0;
    for (int i = 0; i < toPhoneQueue.numUsed(); i++) {
        meshtastic_MeshPacket *p = toPhoneQueue.dequeuePtr(0);
        if (p->id == request_id) {
            nodenum = p->to;
            // make sure to continue this to make one full loop
        }
        // put it right back on the queue
        toPhoneQueue.enqueue(p, 0);
    }
    return nodenum;
}

/**
 *  Given a ToRadio buffer parse it and properly handle it (setup radio, owner or send packet into the mesh)
 * Called by PhoneAPI.handleToRadio.  Note: p is a scratch buffer, this function is allowed to write to it but it can not keep a
 * reference
 */
void MeshService::handleToRadio(meshtastic_MeshPacket &p)
{
#if defined(ARCH_PORTDUINO)
    if (SimRadio::instance && p.decoded.portnum == meshtastic_PortNum_SIMULATOR_APP) {
        // Simulates device received a packet via the LoRa chip
        SimRadio::instance->unpackAndReceive(p);
        return;
    }
#endif
    p.from = 0;                          // We don't let clients assign nodenums to their sent messages
    p.next_hop = NO_NEXT_HOP_PREFERENCE; // We don't let clients assign next_hop to their sent messages
    p.relay_node = NO_RELAY_NODE;        // We don't let clients assign relay_node to their sent messages

    if (p.id == 0)
        p.id = generatePacketId(); // If the phone didn't supply one, then pick one

    p.rx_time = getValidTime(RTCQualityFromNet); // Record the time the packet arrived from the phone

    IF_SCREEN(if (p.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP && p.decoded.payload.size > 0 &&
                  p.to != NODENUM_BROADCAST && p.to != 0) // DM only
              {
                  perhapsDecode(&p);
                  const StoredMessage &sm = messageStore.addFromPacket(p);
                  graphics::MessageRenderer::handleNewMessage(nullptr, sm, p); // notify UI
              })
    // Send the packet into the mesh
    DEBUG_HEAP_BEFORE;
    auto a = packetPool.allocCopy(p);
    DEBUG_HEAP_AFTER("MeshService::handleToRadio", a);
    sendToMesh(a, RX_SRC_USER);

    bool loopback = false; // if true send any packet the phone sends back itself (for testing)
    if (loopback) {
        // no need to copy anymore because handle from radio assumes it should _not_ delete
        // packetPool.allocCopy(r.variant.packet);
        handleFromRadio(&p);
        // handleFromRadio will tell the phone a new packet arrived
    }
}

/** Attempt to cancel a previously sent packet from this _local_ node.  Returns true if a packet was found we could cancel */
bool MeshService::cancelSending(PacketId id)
{
    return router->cancelSending(nodeDB->getNodeNum(), id);
}

ErrorCode MeshService::sendQueueStatusToPhone(const meshtastic_QueueStatus &qs, ErrorCode res, uint32_t mesh_packet_id)
{
    meshtastic_QueueStatus *copied = queueStatusPool.allocCopy(qs);

    copied->res = res;
    copied->mesh_packet_id = mesh_packet_id;

    if (toPhoneQueueStatusQueue.numFree() == 0) {
        LOG_INFO("tophone queue status queue is full, discard oldest");
        meshtastic_QueueStatus *d = toPhoneQueueStatusQueue.dequeuePtr(0);
        if (d)
            releaseQueueStatusToPool(d);
    }

    lastQueueStatus = *copied;

    res = toPhoneQueueStatusQueue.enqueue(copied, 0);
    fromNum++;

    return res ? ERRNO_OK : ERRNO_UNKNOWN;
}

void MeshService::sendToMesh(meshtastic_MeshPacket *p, RxSource src, bool ccToPhone)
{
    uint32_t mesh_packet_id = p->id;
    nodeDB->updateFrom(*p); // update our local DB for this packet (because phone might have sent position packets etc...)

    // Note: We might return !OK if our fifo was full, at that point the only option we have is to drop it
    ErrorCode res = router->sendLocal(p, src);

    /* NOTE(pboldin): Prepare and send QueueStatus message to the phone as a
     * high-priority message. */
    meshtastic_QueueStatus qs = router->getQueueStatus();
    ErrorCode r = sendQueueStatusToPhone(qs, res, mesh_packet_id);
    if (r != ERRNO_OK) {
        LOG_DEBUG("Can't send status to phone");
    }

    if ((res == ERRNO_OK || res == ERRNO_SHOULD_RELEASE) && ccToPhone) { // Check if p is not released in case it couldn't be sent
        DEBUG_HEAP_BEFORE;
        auto a = packetPool.allocCopy(*p);
        DEBUG_HEAP_AFTER("MeshService::sendToMesh", a);

        sendToPhone(a);
    }

    // Router may ask us to release the packet if it wasn't sent
    if (res == ERRNO_SHOULD_RELEASE) {
        releaseToPool(p);
    }
}

bool MeshService::trySendPosition(NodeNum dest, bool wantReplies)
{
    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeDB->getNodeNum());

    assert(node);

    if (nodeDB->hasValidPosition(node)) {
#if HAS_GPS && !MESHTASTIC_EXCLUDE_GPS
        if (positionModule) {
            if (!config.position.fixed_position && !nodeDB->hasLocalPositionSinceBoot()) {
                LOG_DEBUG("Skip position ping; no fresh position since boot");
                return false;
            }
            LOG_INFO("Send position ping to 0x%x, wantReplies=%d, channel=%d", dest, wantReplies, node->channel);
            positionModule->sendOurPosition(dest, wantReplies, node->channel);
            return true;
        }
    } else {
#endif
        if (nodeInfoModule) {
            LOG_INFO("Send nodeinfo ping to 0x%x, wantReplies=%d, channel=%d", dest, wantReplies, node->channel);
            nodeInfoModule->sendOurNodeInfo(dest, wantReplies, node->channel);
        }
    }
    return false;
}

void MeshService::sendToPhone(meshtastic_MeshPacket *p)
{
    perhapsDecode(p);

#ifdef ARCH_ESP32
#if !MESHTASTIC_EXCLUDE_STOREFORWARD
    if (moduleConfig.store_forward.enabled && storeForwardModule->isServer() &&
        p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        releaseToPool(p); // Copy is already stored in StoreForward history
        fromNum++;        // Notify observers for packet from radio
        return;
    }
#endif
#endif

    if (toPhoneQueue.numFree() == 0) {
        if (p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP ||
            p->decoded.portnum == meshtastic_PortNum_RANGE_TEST_APP) {
            LOG_WARN("ToPhone queue is full, discard oldest");
            meshtastic_MeshPacket *d = toPhoneQueue.dequeuePtr(0);
            if (d)
                releaseToPool(d);
        } else {
            LOG_WARN("ToPhone queue is full, drop packet");
            releaseToPool(p);
            fromNum++; // Make sure to notify observers in case they are reconnected so they can get the packets
            return;
        }
    }

    if (toPhoneQueue.enqueue(p, 0) == false) {
        LOG_CRIT("Failed to queue a packet into toPhoneQueue!");
        abort();
    }
    fromNum++;
}

void MeshService::sendMqttMessageToClientProxy(meshtastic_MqttClientProxyMessage *m)
{
    LOG_DEBUG("Send mqtt message on topic '%s' to client for proxy", m->topic);
    if (toPhoneMqttProxyQueue.numFree() == 0) {
        LOG_WARN("MqttClientProxyMessagePool queue is full, discard oldest");
        meshtastic_MqttClientProxyMessage *d = toPhoneMqttProxyQueue.dequeuePtr(0);
        if (d)
            releaseMqttClientProxyMessageToPool(d);
    }

    if (toPhoneMqttProxyQueue.enqueue(m, 0) == false) {
        LOG_CRIT("Failed to queue a packet into toPhoneMqttProxyQueue!");
        abort();
    }
    fromNum++;
}

void MeshService::sendRoutingErrorResponse(meshtastic_Routing_Error error, const meshtastic_MeshPacket *mp)
{
    if (!mp) {
        LOG_WARN("Cannot send routing error response: null packet");
        return;
    }

    // Use the routing module to send the error response
    if (routingModule) {
        routingModule->sendAckNak(error, mp->from, mp->id, mp->channel);
    } else {
        LOG_ERROR("Cannot send routing error response: no routing module");
    }
}

void MeshService::sendClientNotification(meshtastic_ClientNotification *n)
{
    LOG_DEBUG("Send client notification to phone");
    if (toPhoneClientNotificationQueue.numFree() == 0) {
        LOG_WARN("ClientNotification queue is full, discard oldest");
        meshtastic_ClientNotification *d = toPhoneClientNotificationQueue.dequeuePtr(0);
        if (d)
            releaseClientNotificationToPool(d);
    }

    if (toPhoneClientNotificationQueue.enqueue(n, 0) == false) {
        LOG_CRIT("Failed to queue a notification into toPhoneClientNotificationQueue!");
        abort();
    }
    fromNum++;
}

meshtastic_NodeInfoLite *MeshService::refreshLocalMeshNode()
{
    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeDB->getNodeNum());
    assert(node);

    // We might not have a position yet for our local node, in that case, at least try to send the time
    if (!node->has_position) {
        memset(&node->position, 0, sizeof(node->position));
        node->has_position = true;
    }

    meshtastic_PositionLite &position = node->position;

    // Update our local node info with our time (even if we don't decide to update anyone else)
    node->last_heard =
        getValidTime(RTCQualityFromNet); // This nodedb timestamp might be stale, so update it if our clock is kinda valid

    position.time = getValidTime(RTCQualityFromNet);

    if (powerStatus->getHasBattery() == 1) {
        updateBatteryLevel(powerStatus->getBatteryChargePercent());
    }

    return node;
}

#if HAS_GPS
int MeshService::onGPSChanged(const meshtastic::GPSStatus *newStatus)
{
    // Update our local node info with our position (even if we don't decide to update anyone else)
    const meshtastic_NodeInfoLite *node = refreshLocalMeshNode();
    meshtastic_Position pos = meshtastic_Position_init_default;

    if (newStatus->getHasLock()) {
        // load data from GPS object, will add timestamp + battery further down
        pos = gps->p;
    } else {
        // The GPS has lost lock
#ifdef GPS_DEBUG
        LOG_DEBUG("onGPSchanged() - lost validLocation");
#endif
    }
    // Used fixed position if configured regardless of GPS lock
    if (config.position.fixed_position) {
        LOG_WARN("Use fixed position");
        pos = TypeConversions::ConvertToPosition(node->position);
    }

    // Add a fresh timestamp
    pos.time = getValidTime(RTCQualityFromNet);

    // In debug logs, identify position by @timestamp:stage (stage 4 = nodeDB)
    LOG_DEBUG("onGPSChanged() pos@%x time=%u lat=%d lon=%d alt=%d", pos.timestamp, pos.time, pos.latitude_i, pos.longitude_i,
              pos.altitude);

    // Update our current position in the local DB
    nodeDB->updatePosition(nodeDB->getNodeNum(), pos, RX_SRC_LOCAL);

    return 0;
}
#endif
bool MeshService::isToPhoneQueueEmpty()
{
    return toPhoneQueue.isEmpty();
}

uint32_t MeshService::GetTimeSinceMeshPacket(const meshtastic_MeshPacket *mp)
{
    uint32_t now = getTime();

    uint32_t last_seen = mp->rx_time;
    int delta = (int)(now - last_seen);
    if (delta < 0) // our clock must be slightly off still - not set from GPS yet
        delta = 0;

    return delta;
}
