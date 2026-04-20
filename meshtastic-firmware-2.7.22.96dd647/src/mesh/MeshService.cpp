#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif

#include "../concurrency/Periodic.h"
#include "BluetoothCommon.h" 
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
#include "../detect/ScanI2C.h"
#include <Wire.h>
#include "MeshService.h"
// ... altri include ...

// 1. DICHIARAZIONI GLOBALI (Devono stare PRIMA di initHardwarePins)
// 1. PRIMA GLI INCLUDE DELLE LIBRERIE
#ifdef ONEWIRE_TEMP_PIN
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

#ifdef DHT_TEMP_PIN
  #include <DHT.h>
#endif


// 2. POI LE DICHIARAZIONI DELLE VARIABILI (Riga 32-33)
#ifdef ONEWIRE_TEMP_PIN
  OneWire _oneWire(ONEWIRE_TEMP_PIN);
  DallasTemperature _owSensors(&_oneWire);
#endif

#ifdef DHT_TEMP_PIN
  DHT _dht(DHT_TEMP_PIN, DHT11);
#endif
// --- DICHIARAZIONI GLOBALI UNICHE (PULITE) ---

// Diciamo che 'service' e 'fanTemp' esistono già in main.cpp
extern float fanTemp; 
 

// scanI2C esiste già nel firmware
extern ScanI2C *scanI2C; 

// Questo lo teniamo noi qui perché è specifico del nostro nuovo task
TaskHandle_t fanTaskHandle = NULL; 

// Prototipi
void checkInternalFan();
void checkAutoReboot();

// --- BLOCCHI ESISTENTI (NON TOCCARLI) ---
#if ARCH_PORTDUINO
#include "modules/StoreForwardModule.h"
#include "PortduinoGlue.h"
#endif
 
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

// --- IMPLEMENTAZIONE TASK E FUNZIONI ---
// --- IMPLEMENTAZIONE TASK E FUNZIONI ---

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

initHardwarePins(); // La tua nuova sub-routine di boot

    // --- AGGIUNTA PER VENTOLA ---
#ifdef I2C_FAN_SENSOR_ADDR
    if (fanTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            this->fanControlTask,   // Funzione
            "FanControl",           // Nome
            4096,                   // Stack
            NULL,                   // Parametri
            1,                      // Priorità
            &fanTaskHandle,         // Handle
            1                       // Core 1
        );
        LOG_INFO("Task Ventola inizializzato correttamente");
    }
#endif
}


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
    return (isnan(t)) ? -999.0f : t;
}
#endif

// --- LETTURA ANALOGICA (NTC 10k) ---
#if defined(ANALOG_TEMP_PIN)
float readAnalogTemp() {
    long reading = 0;
    for(int i=0; i<10; i++) reading += analogRead(ANALOG_TEMP_PIN);
    float raw = reading / 10.0f;
    
    if (raw <= 0 || raw >= 4095) return -999.0f;

    float res = 10000.0f * ((4095.0f / raw) - 1.0f);
    float steinhart;
    steinhart = res / 10000.0f; 
    steinhart = log(steinhart);
    steinhart /= 3950.0f; 
    steinhart += 1.0f / (25.0f + 273.15f);
    steinhart = 1.0f / steinhart;
    return steinhart - 273.15f; 
}
#endif

// =========================================================================
// GESTIONE HARDWARE PERSONALIZZATA - END
// =========================================================================

float readI2CTemp(uint8_t addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) return -999.0f;

    switch (addr) {
        // --- SHT3x / SHT4x / SHTC3 / STS3x ---
        case 0x44: case 0x45: case 0x70: {
            Wire.beginTransmission(addr);
            Wire.write(0x24); Wire.write(0x00);
            Wire.endTransmission();
            vTaskDelay(pdMS_TO_TICKS(20));
            if (Wire.requestFrom(addr, (uint8_t)6) >= 2) {
                uint16_t raw = (Wire.read() << 8) | Wire.read();
                return -45.0f + 175.0f * (raw / 65535.0f);
            }
            break;
        }

        // --- SI7021 / HTU21D / GY-21 / SHT2x ---
        case 0x40: {
            Wire.beginTransmission(addr);
            Wire.write(0xF3);
            Wire.endTransmission();
            vTaskDelay(pdMS_TO_TICKS(100));
            if (Wire.requestFrom(addr, (uint8_t)2) == 2) {
                uint16_t raw = (Wire.read() << 8) | Wire.read();
                return -46.85f + 175.72f * (raw / 65536.0f);
            }
            break;
        }

        // --- AHT10 / AHT20 / AHT21 / AHT25 ---
        case 0x38: {
            Wire.beginTransmission(addr);
            Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);
            Wire.endTransmission();
            vTaskDelay(pdMS_TO_TICKS(80));
            if (Wire.requestFrom(addr, (uint8_t)6) >= 6) {
                Wire.read(); // skip status
                uint32_t raw = ((uint32_t)(Wire.read() & 0x0F) << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
                return (raw / 1048576.0f) * 200.0f - 50.0f;
            }
            break;
        }

        // --- MCP9808 (Alta precisione) ---
        case 0x18: {
            Wire.beginTransmission(addr);
            Wire.write(0x05); // Registro Ambient Temp
            if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)2) == 2) {
                uint16_t raw = (Wire.read() << 8) | Wire.read();
                float t = (raw & 0x0FFF) / 16.0f;
                if (raw & 0x1000) t -= 256.0f;
                return t;
            }
            break;
        }

        // --- LM75 / TMP102 / Generic I2C Temp ---
        case 0x48: case 0x49: case 0x4A: {
            if (Wire.requestFrom(addr, (uint8_t)2) == 2) {
                int16_t raw = (Wire.read() << 8) | Wire.read();
                return (raw >> 4) * 0.0625f; // 12-bit resolution
            }
            break;
        }

        // --- MLX90614 (Infrarossi / Contactless) ---
        case 0x5A: case 0x5B: {
            Wire.beginTransmission(addr);
            Wire.write(0x07); // Ambient Temp (0x06 per Object Temp)
            if (Wire.endTransmission(false) == 0 && Wire.requestFrom(addr, (uint8_t)3) == 3) {
                uint16_t raw = Wire.read() | (Wire.read() << 8);
                Wire.read(); // PEC (Packet Error Code)
                return (raw * 0.02f) - 273.15f;
            }
            break;
        }

        // --- DS1621 / DS1624 ---
        case 0x4F: {
            Wire.beginTransmission(addr);
            Wire.write(0xAA); // Read Temp command
            if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)2) == 2) {
                int8_t h = Wire.read();
                uint8_t l = Wire.read();
                return h + (l >> 7) * 0.5f;
            }
            break;
        }

        // --- Bosch BME280 / BMP280 / BME680 ---
        case 0x76: case 0x77: {
            // Trigger Forced Measurement
            Wire.beginTransmission(addr);
            Wire.write(0xF4); Wire.write(0x21); 
            Wire.endTransmission();
            vTaskDelay(pdMS_TO_TICKS(10));

            // Lettura calibrazione T1, T2, T3
            uint16_t t1; int16_t t2, t3;
            Wire.beginTransmission(addr);
            Wire.write(0x88);
            if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)6) == 6) {
                t1 = Wire.read() | (Wire.read() << 8);
                t2 = Wire.read() | (Wire.read() << 8);
                t3 = Wire.read() | (Wire.read() << 8);

                // Lettura ADC Temp
                Wire.beginTransmission(addr);
                Wire.write(0xFA);
                if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)3) == 3) {
                    int32_t adc = (uint32_t)Wire.read() << 12 | (uint32_t)Wire.read() << 4 | (uint32_t)Wire.read() >> 4;
                    float v1 = ((float)adc / 16384.0f - (float)t1 / 1024.0f) * (float)t2;
                    float v2 = (((float)adc / 131072.0f - (float)t1 / 8192.0f) * ((float)adc / 131072.0f - (float)t1 / 8192.0f)) * (float)t3;
                    return (v1 + v2) / 5120.0f;
                }
            }
            break;
        }
        
        // --- DPS310 (Pressione e Temperatura) ---
        case 0x75: {
            // Qui servirebbe una logica molto complessa simile a Bosch. 
            // Se lo usi, conviene usare la sua libreria, ma per ora lo lasciamo come placeholder.
            break;
        }
    }

    return -999.0f; // Se arriviamo qui, l'indirizzo è riconosciuto ma la lettura è fallita
}


void checkInternalFan() {
 
    float currentTemp = -999.0f;

    // Selettore dinamico basato su cosa hai compilato
    #if defined(I2C_FAN_SENSOR_ADDR)
        currentTemp = readI2CTemp(I2C_FAN_SENSOR_ADDR);
    #elif defined(ONEWIRE_TEMP_PIN)
        currentTemp = readOneWireTemp();
    #elif defined(DHT_TEMP_PIN)
        currentTemp = readDHTTemp();
    #elif defined(ANALOG_TEMP_PIN)
        currentTemp = readAnalogTemp();
    #endif

   
 
    // --- ATTUAZIONE RELAY ---
    LOG_INFO("FAN: prima di if-then: Temp attuale: %.1f C (Start: %d, Stop: %d)", fanTemp, FAN_TEMP_START, FAN_TEMP_STOP);

    if (currentTemp > -55.0f && currentTemp < 155.0f) {
        fanTemp = currentTemp;
        
        // LOG SEMPRE ATTIVO: Così vedi se il sensore sta leggendo!
        LOG_INFO("FAN_CHECK: Temp attuale: %.1f C (Start: %d, Stop: %d)", fanTemp, FAN_TEMP_START, FAN_TEMP_STOP);

        #ifdef FAN_RELAY_PIN
        LOG_INFO("FAN_RELAY_PIN: Temp attuale: %.1f C (Start: %d, Stop: %d)", fanTemp, FAN_TEMP_START, FAN_TEMP_STOP);

            pinMode(FAN_RELAY_PIN, OUTPUT);
            LOG_INFO("FAN_RELAY_PIN: OUTPUT attuale: %.1f C (Start: %d, Stop: %d)", fanTemp, FAN_TEMP_START, FAN_TEMP_STOP);

            bool currentState = digitalRead(FAN_RELAY_PIN);
            LOG_INFO("FAN_RELAY_PIN: digitalRead attuale: %.1f C (Start: %d, Stop: %d)", fanTemp, FAN_TEMP_START, FAN_TEMP_STOP);

            if (fanTemp >= FAN_TEMP_START) {
                LOG_INFO("FAN_RELAY_PIN: digitalRead attuale: %.1f C (Start: %d, Stop: %d)", fanTemp, FAN_TEMP_START, FAN_TEMP_STOP);

                if (!currentState) { // Era spenta, accendiamo
                    digitalWrite(FAN_RELAY_PIN, HIGH);
                    LOG_INFO("VENTOLA: STATO CAMBIATO -> ACCESA");
                }
            } else if (fanTemp <= FAN_TEMP_STOP) {
                if (currentState) { // Era accesa, spegniamo
                    digitalWrite(FAN_RELAY_PIN, LOW);
                    LOG_INFO("VENTOLA: STATO CAMBIATO -> SPENTA");
                }
            }
        #endif
    } else {
        // Se arrivi qui, il sensore è tornato a -999 o errore
        LOG_ERROR("FAN_CHECK: Lettura sensore NON VALIDA (%.1f)", currentTemp);
    }
 
}

 



void checkAutoReboot() {
// Esegui tutto solo se la macro è definita e maggiore di 0
#if defined(AUTO_REBOOT_DAYS) && (AUTO_REBOOT_DAYS > 0)

    // Calcolo soglia a 64bit per evitare casini (86.400.000 ms in un giorno)
    static const uint64_t threshold = (uint64_t)AUTO_REBOOT_DAYS * 86400000ULL;

    if (millis() > threshold) {
        LOG_INFO("GHOST: Uptime limit reached (%d days). Rebooting...", AUTO_REBOOT_DAYS);
        delay(1000); 

        // --- IL CUORE DEL REBOOT (Copiato dalla funzione ufficiale) ---
        
        // Se possibile, avvisiamo il sistema (opzionale, se dà errore commenta la riga sotto)
        // notifyReboot.notifyObservers(NULL);

        #if defined(ARCH_ESP32) || defined(ESP32)
            ESP.restart();

        #elif defined(ARCH_NRF52) || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52840)
            NVIC_SystemReset();

        #elif defined(ARCH_RP2040)
            rp2040.reboot();

        #elif defined(ARCH_PORTDUINO)
            // Se lo stai facendo girare su Linux/PC
            LOG_DEBUG("final reboot!");
            ::reboot(0); 

        #elif defined(ARCH_STM32WL)
            HAL_NVIC_SystemReset();

        #else
            // Fallback universale per processori ARM (incluso il T114)
            NVIC_SystemReset();
        #endif
    }
#endif
}




// Task di sistema per il loop di controllo
void MeshService::fanControlTask(void *pvParameters) {
    LOG_INFO("TASK_MAINTENANCE: Avviato su Core 1");
    // Questo delay gira UNA SOLA VOLTA all'avvio del modulo
    vTaskDelay(pdMS_TO_TICKS(3000));
    for (;;) {
       
        checkInternalFan();
        checkAutoReboot();
        vTaskDelay(pdMS_TO_TICKS(15000)); // Controllo ogni 15 sec
    }
}



/// Do idle processing (mostly processing messages which have been queued from the radio)
void MeshService::loop()
{
    ////////////////checkAutoReboot(); // Aggiungi questo
    //////////checkInternalFan(); // Gestisce la ventola in base alla temp interna
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
