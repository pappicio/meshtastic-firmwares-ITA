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


#include "modules/Telemetry/EnvironmentTelemetry.h"


// -------------------------------------------------------------
// INCLUDE UNIVERSALI PER MEMORIA NON VOLATILE (METEO TARATURE)
// -------------------------------------------------------------
#if defined(ARCH_ESP32) || defined(ESP32)
    #include <Preferences.h>
#elif defined(NRF52_SERIES)
    #include <InternalFileSystem.h>
    // Usiamo il namespace di Adafruit per i chip Nordic nRF52 di Meshtastic
    using namespace Adafruit_LittleFS_Namespace; 
#endif
// -------------------------------------------------------------
///////////////////////////////////////////////
 

TextMessageModule *textMessageModule;

std::string globalPrivateBuffer = "";


extern EnvironmentTelemetryModule *environmentTelemetryModule;

///////////////////////////////////////////////
/**
 * Gestisce i comandi remoti per Relay e Anemometro in modo atomico e universale.
 */
///////////////////////////////////////////////
/**
 * Gestisce i comandi remoti per Relay e Anemometro.
 * Funzione light e atomica per integrazione in FloodingRouter.
 **/
static void sendConfirm(const meshtastic_MeshPacket *req, const char *msg)
{
    if (!req) return;

    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p) return;


    p->to = req->from;
    p->from = nodeDB->getNodeNum();
    p->channel = req->channel;

    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;

    size_t len = strnlen(msg, sizeof(p->decoded.payload.bytes));
    memcpy(p->decoded.payload.bytes, msg, len);
    p->decoded.payload.size = (pb_size_t)len;

    // --- DEBUG LOG ---
    LOG_INFO("DEBUG SEND: Packet prepared. From: 0x%X, To: 0x%X, Channel: %d", p->from, p->to, p->channel);
    bool isLocal = isFromUs(req);
    LOG_INFO("DEBUG SEND: isFromUs result = %d", isLocal);

    if (isLocal) {
        p->to = nodeDB->getNodeNum();
        LOG_INFO("DEBUG SEND: Routing to Phone/CLI (sendToPhone)");
        service->sendToPhone(p);
    } else {
        LOG_INFO("DEBUG SEND: Routing to Radio (router->send)");
        router->send(p);
    }

////////////////////////////// iniettiamo testo per mqtt
    MeshService::globalPrivateBuffer = std::string(msg).substr(0, 200);
    
    if (EnvironmentTelemetryModule::instance != nullptr) {
        EnvironmentTelemetryModule::pendingMqttPublish = true; // solo flag, niente chiamate
    }
//////////////////////////////

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
    using namespace Adafruit_LittleFS_Namespace;
    InternalFS.remove("/meteo.dat");
    File file = InternalFS.open("/meteo.dat", FILE_O_WRITE);
    if (file)  {
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
bool checkMultiRelayCommand(const meshtastic_MeshPacket *p) {

    if (p == nullptr)
        return false;

    if (p->which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return false;

    if (p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return false;

    ////if (p->to != nodeDB->getNodeNum())
    ////    return false; --- IGNORE ---

// ==========================================================
    // MODIFICA CON LOG DI DEBUG PER OGNI VARIABILE
    // ==========================================================
    bool direttoANoi = (p->to == nodeDB->getNodeNum());
    bool localeDaAPI = (p->from == nodeDB->getNodeNum());
    bool sistemaLocale = (p->from == 0);

    // LOG DI DEBUG: Vediamo cosa cavolo sta succedendo
    LOG_INFO("DEBUG: Filtro - From: 0x%X, To: 0x%X", p->from, p->to);
    LOG_INFO("DEBUG: Analisi - DirettoANoi: %d, LocaleDaAPI: %d, SistemaLocale: %d", 
             direttoANoi, localeDaAPI, sistemaLocale);

    if (!direttoANoi && !localeDaAPI && !sistemaLocale) {
        LOG_INFO("DEBUG: --- FILTRO SCARTATO (Tutte le condizioni false) ---");
        return false;
    }
    
    LOG_INFO("DEBUG: --- FILTRO PASSATO (Condizione valida) ---");
    // ==========================================================
  

 const size_t len = p->decoded.payload.size;

    if (len == 0 || len > 200)
        return false;

    char tmpbuf[201];
    memcpy(tmpbuf, p->decoded.payload.bytes, len);
    tmpbuf[len] = '\0';
    String msg(tmpbuf);
    msg.trim();

    if (msg.length() == 0)
        return false;

// =====================================================
//  IL TUO GRANDE BLOCCO UNICO DI SICUREZZA
// =====================================================
#ifdef CMD_PASSWORD

// 1. FILTRO PASSWORD: Silenzio totale se errata
    if (!msg.startsWith(CMD_PASSWORD)) {
        return false; 
    }

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
                "Guadagno: %.3f | Attrito: %.3f | ", 
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
            return true;
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
            return true;
        }
#endif

        // --- GUADAGNO ---
#if defined(WIND_VELOCITY_PIN) && defined(COMANDO_GUADAGNO)
        if (msg.startsWith(COMANDO_GUADAGNO)) {
            float value = msg.substring(strlen(COMANDO_GUADAGNO) + 1).toFloat();
            if (!isnan(value) && value > 0.0f && value < 1000.0f) {
                ANEMOMETRO_GUADAGNO = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Guadagno %.3f", ANEMOMETRO_GUADAGNO);
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo guadagno: %.3f salvato", ANEMOMETRO_GUADAGNO);
                sendConfirm(p, buf);
                return true;
            }
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
                snprintf(buf, sizeof(buf), "OK nuovo attrito: %.3f salvato", ANEMOMETRO_ATTRITO);
                sendConfirm(p, buf);
                return true;
            }
        }
#endif

        // --- OFFSET DIREZIONE ---
#if defined(HAS_WIND_DIRECTION) && defined(COMANDO_DIREZIONE)
        if (msg.startsWith(COMANDO_DIREZIONE)) {
            float value = msg.substring(strlen(COMANDO_DIREZIONE) + 1).toFloat();
            if (!isnan(value) && value >= -360.0f && value <= 360.0f) {
                WIND_NORTH_OFFSET = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Offset %.3f", WIND_NORTH_OFFSET);
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo offset direzione vento: %.3f salvato", WIND_NORTH_OFFSET);
                sendConfirm(p, buf);
                return true;
            }
        }
#endif

// --- CALIBRAZIONE PLUVIOMETRO ---
#if defined(RAIN_SENSOR_PIN) && defined(COMANDO_RAINOFFSET)
        if (msg.startsWith(COMANDO_RAINOFFSET)) {
            // Usa strlen(COMANDO_RAINOFFSET) invece di "11" per essere dinamico
            float value = msg.substring(strlen(COMANDO_RAINOFFSET) + 1).toFloat();
            
            if (!isnan(value) && value > 0.0f && value < 1000.0f) {
                RAIN_GAUGE_FACTOR = value;
                salvaMeteo();
                LOG_INFO("METEO REMOTE: Nuovo Rain Factor %.3f", RAIN_GAUGE_FACTOR);
                
                char buf[64];
                snprintf(buf, sizeof(buf), "OK nuovo rain factor: %.3f salvato", RAIN_GAUGE_FACTOR);
                sendConfirm(p, buf);
                return true;
            }
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
            return true;
        }

        if (msg.equals(String(CMD_RELAY_OFF) + " " + RELAY_1_NAME)) {
            digitalWrite(RELAY_1_PIN, LOW);
            LOG_INFO("REMOTE: %s OFF", RELAY_1_NAME);
            char buf[64];
            snprintf(buf, sizeof(buf), "OK %s OFF", RELAY_1_NAME);
            sendConfirm(p, buf);
            return true;
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
            return true;
        }

        if (msg.equals(String(CMD_RELAY_OFF) + " " + RELAY_2_NAME)) {
            digitalWrite(RELAY_2_PIN, LOW);
            LOG_INFO("REMOTE: %s OFF", RELAY_2_NAME);
            char buf[64];
            snprintf(buf, sizeof(buf), "OK %s OFF", RELAY_2_NAME);
            sendConfirm(p, buf);
            return true;
        }
#endif


 
    // 3. FEEDBACK ERRORE DEFAULT
    // Se la password era giusta ma il comando non è tra quelli sopra:
    sendConfirm(p, "ERRORE: Comando sconosciuto o paramentro non valido");
    return true;


    } // Chiude l'if password

    //dddddddddddddddddd
    return false;
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

 	LOG_INFO("DEBUG: Messaggio gestito dal modulo.");
    if (checkMultiRelayCommand(&mp)) 
    {
        LOG_INFO("DEBUG: Comando multi-relay riconosciuto e gestito.");
        return ProcessMessage::STOP; // Let others look at this message also if they want
    } else {
        LOG_INFO("DEBUG: Messaggio non riconosciuto come comando multi-relay, nessuna azione intrapresa.");     
    }
 
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