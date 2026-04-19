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




 
 

// --- GESTIONE MANUTENZIONE AVANZATA (VENTOLA + SENSORI I2C) ---
void checkInternalFan() {
#ifdef I2C_FAN_SENSOR_ADDR
    float currentTemp = -999.0;
    uint8_t addr = I2C_FAN_SENSOR_ADDR;

    // Controllo presenza device sul bus
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
        switch (addr) {
            // SHT3x / SHT4x / SHTC3 / STS3x
            case 0x44: case 0x45: case 0x70:
                Wire.beginTransmission(addr);
                Wire.write(0x24); Wire.write(0x00);
                if (Wire.endTransmission() == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                    if (Wire.requestFrom(addr, (uint8_t)6) >= 2) {
                        uint16_t raw = (Wire.read() << 8) | Wire.read();
                        currentTemp = -45.0f + 175.0f * (raw / 65535.0f);
                        LOG_DEBUG("FAN: SHT rilevato a 0x%02x: %.1f C", addr, currentTemp);
                    }
                }
                break;

            // SHT2x / SI7021 / HTU21D
            case 0x40:
                Wire.beginTransmission(addr);
                Wire.write(0xF3);
                if (Wire.endTransmission() == 0) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (Wire.requestFrom(addr, (uint8_t)2) == 2) {
                        uint16_t raw = (Wire.read() << 8) | Wire.read();
                        currentTemp = -46.85f + 175.72f * (raw / 65536.0f);
                        LOG_DEBUG("FAN: SI7021 rilevato: %.1f C", currentTemp);
                    }
                }
                break;

            // AHT10 / AHT20 / AHT21
            case 0x38:
                Wire.beginTransmission(addr);
                Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);
                Wire.endTransmission();
                vTaskDelay(pdMS_TO_TICKS(80));
                if (Wire.requestFrom(addr, (uint8_t)6) >= 6) {
                    Wire.read(); // Skip status
                    uint32_t rawTemp = ((uint32_t)(Wire.read() & 0x0F) << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
                    currentTemp = (rawTemp / 1048576.0f) * 200.0f - 50.0f;
                    LOG_DEBUG("FAN: AHT rilevato: %.1f C", currentTemp);
                }
                break;

case 0x76: case 0x77:
    {
        // 1. FORZA IL SENSORE A FARE UNA MISURA (Power Mode: Forced)
        // Scriviamo nel registro 0xF4 (ctrl_meas): 
        // 0x21 significa: Oversampling Temp x1, Mode: Forced
        Wire.beginTransmission(addr);
        Wire.write(0xF4); 
        Wire.write(0x21); 
        Wire.endTransmission();
        delay(10); // Aspetta che il sensore finisca la conversione

        uint16_t dig_T1; int16_t dig_T2, dig_T3;
        
        // 2. LEGGI I COEFFICIENTI DI CALIBRAZIONE (Registro 0x88)
        Wire.beginTransmission(addr);
        Wire.write(0x88);
        if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)6) == 6) {
            dig_T1 = Wire.read() | (Wire.read() << 8);
            dig_T2 = Wire.read() | (Wire.read() << 8);
            dig_T3 = Wire.read() | (Wire.read() << 8);

            // 3. LEGGI I DATI RAW DELLA TEMPERATURA (Registro 0xFA)
            Wire.beginTransmission(addr);
            Wire.write(0xFA);
            if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)3) == 3) {
                int32_t adc_T = (uint32_t)Wire.read() << 12 | (uint32_t)Wire.read() << 4 | (uint32_t)Wire.read() >> 4;
                
                // Formula Bosch compensata (Floating point)
                float v1 = (((float)adc_T) / 16384.0f - ((float)dig_T1) / 1024.0f) * ((float)dig_T2);
                float v2 = ((((float)adc_T) / 131072.0f - ((float)dig_T1) / 8192.0f) *
                           (((float)adc_T) / 131072.0f - ((float)dig_T1) / 8192.0f)) * ((float)dig_T3);
                
                currentTemp = (v1 + v2) / 5120.0f;
                LOG_INFO("FAN: Lettura manuale 0x%02x: %.1f C", addr, currentTemp);
            }
        }
    }
    break;

            // MLX90614
            case 0x5A: case 0x5B:
                Wire.beginTransmission(addr);
                Wire.write(0x07);
                if (Wire.endTransmission(false) == 0 && Wire.requestFrom(addr, (uint8_t)3) == 3) {
                    uint16_t raw = Wire.read() | (Wire.read() << 8);
                    (void)Wire.read(); // Legge PEC e pulisce buffer (no warning)
                    currentTemp = (raw * 0.02f) - 273.15f;
                    LOG_DEBUG("FAN: MLX rilevato: %.1f C", currentTemp);
                }
                break;

            // DS1621 / DS1624
            case 0x4F:
                Wire.beginTransmission(addr);
                Wire.write(0xAA);
                if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)2) == 2) {
                    int8_t h = Wire.read(); uint8_t l = Wire.read();
                    currentTemp = h + (l >> 7) * 0.5f;
                    LOG_DEBUG("FAN: DS1621 rilevato: %.1f C", currentTemp);
                }
                break;

            // MCP9808 / LM75
            case 0x18: case 0x48:
                Wire.beginTransmission(addr);
                Wire.write(addr == 0x18 ? 0x05 : 0x00);
                if (Wire.endTransmission() == 0 && Wire.requestFrom(addr, (uint8_t)2) == 2) {
                    uint16_t raw = (Wire.read() << 8) | Wire.read();
                    if (addr == 0x18) {
                        uint16_t t = raw & 0x0FFF;
                        currentTemp = t / 16.0f;
                        if (raw & 0x1000) currentTemp -= 256.0f;
                    } else {
                        currentTemp = (int16_t)raw * 0.0078125f;
                    }
                    LOG_DEBUG("FAN: MCP/LM rilevato: %.1f C", currentTemp);
                }
                break;
        }
    }

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
#endif
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
