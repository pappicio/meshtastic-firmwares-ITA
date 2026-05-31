#include "TextMessageModule.h"
#include "MeshService.h"
#include "MessageStore.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "configuration.h"
#include "graphics/Screen.h"
#include "graphics/SharedUIDisplay.h"
#include "graphics/draw/MessageRenderer.h"
#include "main.h"


///////////////////////////////////////////////
// -------------------------------------------------------------
// INCLUDE UNIVERSALI PER MEMORIA NON VOLATILE (METEO TARATURE)
// -------------------------------------------------------------
#if defined(ARCH_ESP32) || defined(ESP32)
    #include <Preferences.h>
#elif defined(NRF52_SERIES)
    #include <InternalFileSystem.h>
    // Usiamo il namespace di Adafruit per i chip Nordic nRF52 di Meshtastic
    using namespace Adafruit_InternalFS; 
#endif
// -------------------------------------------------------------
///////////////////////////////////////////////
 

TextMessageModule *textMessageModule;



///////////////////////////////////////////////
/**
 * Gestisce i comandi remoti per Relay e Anemometro in modo atomico e universale.
 */
///////////////////////////////////////////////
/**
 * Gestisce i comandi remoti per Relay e Anemometro.
 * Funzione light e atomica per integrazione in FloodingRouter.
 **/
 
// ---- Sostituisci tutta la funzione sendConfirm con questa ----

static void sendConfirm(const meshtastic_MeshPacket *req, const char *msg)
{
    // Alloca un nuovo pacchetto da zero (non usa allocReply/myReply)
    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p) return;

    // Risposta privata al mittente
    p->to   = req->from;
    p->from = nodeDB->getNodeNum();

    // Stesso canale del messaggio ricevuto
    p->channel = req->channel;

    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack        = false;

    size_t len = strnlen(msg, sizeof(p->decoded.payload.bytes));
    memcpy(p->decoded.payload.bytes, msg, len);
    p->decoded.payload.size = (pb_size_t)len;

    // Invia direttamente senza passare per myReply
    router->send(p);
}

static void salvaMeteo() {

#if defined(ARCH_ESP32) || defined(ESP32)
    Preferences prefs;
    if (prefs.begin("meteo", false)) {
#ifdef WIND_VELOCITY_PIN
    prefs.putFloat("guadagno", ANEMOMETRO_GUADAGNO);
    prefs.putFloat("attrito", ANEMOMETRO_ATTRITO);
#endif
#ifdef RAIN_SENSOR_PIN
    prefs.putFloat("rainfactor", RAIN_GAUGE_FACTOR);
#endif
#ifdef HAS_WIND_DIRECTION
    prefs.putFloat("diroffset", WIND_NORTH_OFFSET);
    prefs.putBool("invertito", WIND_DIRECTION_INVERT);
#endif
        prefs.end();
    }
#elif defined(NRF52_SERIES)
    InternalFS.remove("/meteo.dat");
    File file = InternalFS.open("/meteo.dat", FILE_O_WRITE);
    if (file) {
#ifdef WIND_VELOCITY_PIN
    file.write((const uint8_t*)&ANEMOMETRO_GUADAGNO, sizeof(ANEMOMETRO_GUADAGNO));
    file.write((const uint8_t*)&ANEMOMETRO_ATTRITO, sizeof(ANEMOMETRO_ATTRITO));
#endif
#ifdef RAIN_SENSOR_PIN
    file.write((const uint8_t*)&RAIN_GAUGE_FACTOR, sizeof(RAIN_GAUGE_FACTOR));
#endif
#ifdef HAS_WIND_DIRECTION
    file.write((const uint8_t*)&WIND_NORTH_OFFSET, sizeof(WIND_NORTH_OFFSET));
    file.write((const uint8_t*)&WIND_DIRECTION_INVERT, sizeof(WIND_DIRECTION_INVERT));
#endif
        file.close();
    }
#endif
}

// =====================================================
//  PARSER COMANDI
// =====================================================
void checkMultiRelayCommand(const meshtastic_MeshPacket *p) {

    if (p == nullptr)
        return;

    if (p->which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return;

    if (p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return;

    if (p->to != nodeDB->getNodeNum())
        return;

    const size_t len = p->decoded.payload.size;

    if (len == 0 || len > 200)
        return;

    String msg((const char*)p->decoded.payload.bytes, len);
    msg.trim();

    if (msg.isEmpty())
        return;

// =====================================================
//  IL TUO GRANDE BLOCCO UNICO DI SICUREZZA
// =====================================================
#ifdef CMD_PASSWORD
    if (msg.startsWith(CMD_PASSWORD " ")) {
        
        msg = msg.substring(strlen(CMD_PASSWORD) + 1);
        msg.trim();

        // --- COMANDO STATO ---
        if (msg.equals(COMANDO_STATO)) {
            char stato[180];
            int offset = 0;

#ifdef HAS_WIND_DIRECTION
            offset += snprintf(stato + offset, sizeof(stato) - offset,
                "Offset Nord: %.0f | Magnete Invertito: %s | ", 
                WIND_NORTH_OFFSET, WIND_DIRECTION_INVERT ? "SI" : "NO");
#endif

#ifdef WIND_VELOCITY_PIN
            offset += snprintf(stato + offset, sizeof(stato) - offset,
                "Guadagno: %.2f | Attrito: %.2f | ", 
                ANEMOMETRO_GUADAGNO, ANEMOMETRO_ATTRITO);
#endif

#ifdef RAIN_SENSOR_PIN
    offset += snprintf(stato + offset, sizeof(stato) - offset,
        "Rain Factor: %.3f | ", RAIN_GAUGE_FACTOR);
#endif

#if defined(RELAY_1_PIN) && defined(RELAY_1_NAME)
            offset += snprintf(stato + offset, sizeof(stato) - offset,
                "%s: %s | ", RELAY_1_NAME, digitalRead(RELAY_1_PIN) == HIGH ? "ON" : "OFF");
#endif

#if defined(RELAY_2_PIN) && defined(RELAY_2_NAME)
            offset += snprintf(stato + offset, sizeof(stato) - offset,
                "%s: %s", RELAY_2_NAME, digitalRead(RELAY_2_PIN) == HIGH ? "ON" : "OFF");
#endif

            sendConfirm(p, stato);
            return;
        }

        // --- INVERTI DIREZIONE ---
#if defined(HAS_WIND_DIRECTION) && defined(COMANDO_INVERTI)
        if (msg.equals(COMANDO_INVERTI)) {
            WIND_DIRECTION_INVERT = !WIND_DIRECTION_INVERT;
            salvaMeteo();
            char buf[64];
            snprintf(buf, sizeof(buf), "OK Magnete invertito: %s", 
                     WIND_DIRECTION_INVERT ? "SI" : "NO");
            sendConfirm(p, buf);
            return;
        }
#endif

        // --- GUADAGNO ---
#if defined(WIND_VELOCITY_PIN) && defined(COMANDO_GUADAGNO)
        if (msg.startsWith(COMANDO_GUADAGNO)) {
            float value = msg.substring(strlen(COMANDO_GUADAGNO) + 1).toFloat();
            if (!isnan(value) && value > 0.0f && value < 100.0f) {
                ANEMOMETRO_GUADAGNO = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Guadagno %.3f", ANEMOMETRO_GUADAGNO);
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo guadagno: %.1f salvato", ANEMOMETRO_GUADAGNO);
                sendConfirm(p, buf);
            }
            return;
        }
#endif

        // --- ATTRITO ---
#if defined(WIND_VELOCITY_PIN) && defined(COMANDO_ATTRITO)
        if (msg.startsWith(COMANDO_ATTRITO)) {
            float value = msg.substring(strlen(COMANDO_ATTRITO) + 1).toFloat();
            if (!isnan(value) && value >= 0.0f && value < 100.0f) {
                ANEMOMETRO_ATTRITO = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Attrito %.3f", ANEMOMETRO_ATTRITO);
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo attrito: %.1f salvato", ANEMOMETRO_ATTRITO);
                sendConfirm(p, buf);
            }
            return;
        }
#endif

        // --- OFFSET DIREZIONE ---
#if defined(HAS_WIND_DIRECTION) && defined(COMANDO_DIREZIONE)
        if (msg.startsWith(COMANDO_DIREZIONE)) {
            float value = msg.substring(strlen(COMANDO_DIREZIONE) + 1).toFloat();
            if (!isnan(value) && value >= -360.0f && value <= 360.0f) {
                WIND_NORTH_OFFSET = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Offset %.1f", WIND_NORTH_OFFSET);
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo offset direzione vento: %.1f salvato", WIND_NORTH_OFFSET);
                sendConfirm(p, buf);
            }
            return;
        }
#endif

// --- CALIBRAZIONE PLUVIOMETRO ---
#if defined(RAIN_SENSOR_PIN) && defined(COMANDO_RAINOFFSET)
        if (msg.startsWith(COMANDO_RAINOFFSET)) {
            // Usa strlen(COMANDO_RAINOFFSET) invece di "11" per essere dinamico
            float value = msg.substring(strlen(COMANDO_RAINOFFSET) + 1).toFloat();
            
            if (!isnan(value) && value > 0.0f && value < 1.0f) {
                RAIN_GAUGE_FACTOR = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Nuovo Rain Factor %.3f", RAIN_GAUGE_FACTOR);
                
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo rain factor: %.3f salvato", RAIN_GAUGE_FACTOR);
                sendConfirm(p, buf);
            }
            return;
        }
#endif

        // --- RELAY 1 ---
#if defined(RELAY_1_PIN) && defined(RELAY_1_NAME)
        if (msg.equals(String(CMD_RELAY_ON) + " " + RELAY_1_NAME)) {
            digitalWrite(RELAY_1_PIN, HIGH);
            LOG_INFO("REMOTE: %s ON", RELAY_1_NAME);
            char buf[64];
            snprintf(buf, sizeof(buf), "OK %s ON", RELAY_1_NAME);
            sendConfirm(p, buf);
            return;
        }

        if (msg.equals(String(CMD_RELAY_OFF) + " " + RELAY_1_NAME)) {
            digitalWrite(RELAY_1_PIN, LOW);
            LOG_INFO("REMOTE: %s OFF", RELAY_1_NAME);
            char buf[64];
            snprintf(buf, sizeof(buf), "OK %s OFF", RELAY_1_NAME);
            sendConfirm(p, buf);
            return;
        }
#endif

        // --- RELAY 2 ---
#if defined(RELAY_2_PIN) && defined(RELAY_2_NAME)
        if (msg.equals(String(CMD_RELAY_ON) + " " + RELAY_2_NAME)) {
            digitalWrite(RELAY_2_PIN, HIGH);
            LOG_INFO("REMOTE: %s ON", RELAY_2_NAME);
            char buf[64];
            snprintf(buf, sizeof(buf), "OK %s ON", RELAY_2_NAME);
            sendConfirm(p, buf);
            return;
        }

        if (msg.equals(String(CMD_RELAY_OFF) + " " + RELAY_2_NAME)) {
            digitalWrite(RELAY_2_PIN, LOW);
            LOG_INFO("REMOTE: %s OFF", RELAY_2_NAME);
            char buf[64];
            snprintf(buf, sizeof(buf), "OK %s OFF", RELAY_2_NAME);
            sendConfirm(p, buf);
            return;
        }
#endif

    } // Chiude l'if password
#endif // Chiude la macro globale CMD_PASSWORD

} // Chiude la funzione checkMultiRelayCommand

/////////////////////////////////////////////////////


ProcessMessage TextMessageModule::handleReceived(const meshtastic_MeshPacket &mp)
{
#if defined(DEBUG_PORT) && !defined(DEBUG_MUTE)
    auto &p = mp.decoded;
    LOG_INFO("Received text msg from=0x%0x, id=0x%x, msg=%.*s", mp.from, mp.id, p.payload.size, p.payload.bytes);
#endif



///////////////////////////////////////////////
    // 👉 TELECOMANDO REMOTO (QUI È IL PUNTO GIUSTO)
if (mp.to == nodeDB->getNodeNum()) {

    checkMultiRelayCommand(&mp);
 
}
///////////////////////////////////////////////


    // add packet ID to the rolling list of packets
    textPacketList[textPacketListIndex] = mp.id;
    textPacketListIndex = (textPacketListIndex + 1) % TEXT_PACKET_LIST_SIZE;

    // We only store/display messages destined for us.
    devicestate.rx_text_message = mp;
    devicestate.has_rx_text_message = true;
    IF_SCREEN(
        // Guard against running in MeshtasticUI or with no screen
        if (config.display.displaymode != meshtastic_Config_DisplayConfig_DisplayMode_COLOR) {
            // Store in the central message history
            const StoredMessage &sm = messageStore.addFromPacket(mp);

            // Pass message to renderer (banner + thread switching + scroll reset)
            // Use the global Screen singleton to retrieve the current OLED display
            auto *display = screen ? screen->getDisplayDevice() : nullptr;
            graphics::MessageRenderer::handleNewMessage(display, sm, mp);
        })
    // Only trigger screen wake if configuration allows it
    if (shouldWakeOnReceivedMessage()) {
        powerFSM.trigger(EVENT_RECEIVED_MSG);
    }

    // Notify any observers (e.g. external modules that care about packets)
    notifyObservers(&mp);

    return ProcessMessage::CONTINUE; // Let others look at this message also if they want
}

bool TextMessageModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}

bool TextMessageModule::recentlySeen(uint32_t id)
{
    for (size_t i = 0; i < TEXT_PACKET_LIST_SIZE; i++) {
        if (textPacketList[i] != 0 && textPacketList[i] == id) {
            return true;
        }
    }
    return false;
}