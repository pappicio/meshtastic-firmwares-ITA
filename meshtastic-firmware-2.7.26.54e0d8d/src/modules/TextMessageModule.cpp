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


//////#include "modules/Telemetry/EnvironmentTelemetry.h"
#include "comandiremoti.h" // Aggiungi questo in cima al file

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
    if (checkcomandi(&mp))
    {
        LOG_INFO("DEBUG: Comando riconosciuto e gestito.");
        return ProcessMessage::STOP; // Let others look at this message also if they want
    } else {
        LOG_INFO("DEBUG: Messaggio non riconosciuto come comando, nessuna azione intrapresa.");     
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