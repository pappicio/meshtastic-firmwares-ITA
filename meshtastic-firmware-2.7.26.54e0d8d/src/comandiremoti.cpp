


#include "comandiremoti.h"
#include "main.h"
#include "mesh/MeshService.h"
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

// --- FUNZIONI CARICAMENTO E SALVATAGGIO ---
/////////////////////////////////////////////////

void caricavariabili() {

#if !defined(I2C_FAN_SENSOR_ADDR) && !defined(ONEWIRE_TEMP_PIN) && !defined(DHT_TEMP_PIN) && !defined(ANALOG_TEMP_PIN)
    fan_temp_start = -99.0f;
    fan_temp_stop  = -99.0f;

    #ifndef FAN_HUM_START
        fan_hum_start = 0.0f;
        fan_hum_stop  = 0.0f;
    #endif
#endif
 


#ifndef DEEPSLEEP
    // Valori "dummy" se la macro non è definita
    ABSOLUTE_SHUTDOWN_COUNT = 0;
    force_sleep_mv = -1;
    force_wakeup_mv = -1;
    force_wakeup_hr = -1;
    force_deepsleep_enabled = false;
#endif

#ifndef FAN_RELAY_PIN
    // Variabili "fantasma" impostate a -99 per evitare errori di compilazione
    // se altre parti del codice le leggono comunque
    fan_temp_start = -99.0f;
    fan_temp_stop  = -99.0f;
    fan_hum_start  = -99.0f;
    fan_hum_stop   = -99.0f;
#endif

#if defined(ARCH_ESP32) || defined(ESP32)
    Preferences prefs;
    if (prefs.begin("firmware", true)) {
        firmware_cmd_password[0] = '\0';
        prefs.getString("cmd_pass", CMD_PASSWORD).toCharArray(firmware_cmd_password, sizeof(firmware_cmd_password));
        auto_reboot_days = prefs.getInt("reb_days", 5);
        firmware_clean_also_nodedb = prefs.getBool("cln_ndb", true);
        firmware_keep_preferred = prefs.getBool("kp_pref", true);
        
        RAIN_GAUGE_FACTOR = prefs.getFloat("r_factor", 0.279f);
        ANEMOMETRO_GUADAGNO = prefs.getFloat("g_anemo", 1.38f);
        ANEMOMETRO_ATTRITO = prefs.getFloat("a_anemo", 0.30f);
        WIND_NORTH_OFFSET = prefs.getFloat("n_offset", 0.0f);
        WIND_DIRECTION_INVERT = prefs.getBool("w_invert", false);
        
        fan_temp_start = prefs.getFloat("f_t_start", 42.0f);
        fan_temp_stop = prefs.getFloat("f_t_stop", 35.0f);
        fan_hum_start = prefs.getFloat("f_h_start", 80.0f);
        fan_hum_stop = prefs.getFloat("f_h_stop", 60.0f);
        
        force_sleep_mv = prefs.getInt("s_mv", 3400);
        force_wakeup_mv = prefs.getInt("w_mv", 3700);
        force_wakeup_hr = prefs.getInt("w_hr", 12);
        prefs.end();
    }
#elif defined(NRF52_SERIES)
    if (InternalFS.exists("/firmware.dat")) {
        File file(InternalFS);
        if (file.open("/firmware.dat", FILE_O_READ)) {
            file.read(firmware_cmd_password, sizeof(firmware_cmd_password));
            file.read(&auto_reboot_days, sizeof(auto_reboot_days));
            file.read(&firmware_clean_also_nodedb, sizeof(firmware_clean_also_nodedb));
            file.read(&firmware_keep_preferred, sizeof(firmware_keep_preferred));
            file.read(&RAIN_GAUGE_FACTOR, sizeof(RAIN_GAUGE_FACTOR));
            file.read(&ANEMOMETRO_GUADAGNO, sizeof(ANEMOMETRO_GUADAGNO));
            file.read(&ANEMOMETRO_ATTRITO, sizeof(ANEMOMETRO_ATTRITO));
            file.read(&WIND_NORTH_OFFSET, sizeof(WIND_NORTH_OFFSET));
            file.read(&WIND_DIRECTION_INVERT, sizeof(WIND_DIRECTION_INVERT));
            file.read(&fan_temp_start, sizeof(fan_temp_start));
            file.read(&fan_temp_stop, sizeof(fan_temp_stop));
            file.read(&fan_hum_start, sizeof(fan_hum_start));
            file.read(&fan_hum_stop, sizeof(fan_hum_stop));
            file.read(&force_sleep_mv, sizeof(force_sleep_mv));
            file.read(&force_wakeup_mv, sizeof(force_wakeup_mv));
            file.read(&force_wakeup_hr, sizeof(force_wakeup_hr));
            file.close();
        }
    }
#endif
    // =============================================================
    // 6. COSTRUZIONE LOG DI AVVIO AGGIORNATO (Stringa dinamica)
    // =============================================================
    String logMsg = "[Variabili] Config attiva -> ";
    
    // Log Sistema e Reboot
    logMsg += "Pass: " + String(firmware_cmd_password) + " | ";
    logMsg += "Reboot: " + String(auto_reboot_days) + "gg (CleanDB: " + String(firmware_clean_also_nodedb ? "SI" : "NO") + ", KeepPref: " + String(firmware_keep_preferred ? "SI" : "NO") + ") | ";

    // Log Meteo
#ifdef WIND_VELOCITY_PIN
    logMsg += "Anem G: " + String(ANEMOMETRO_GUADAGNO, 2) + " A: " + String(ANEMOMETRO_ATTRITO, 2) + " | ";
#endif
#ifdef RAIN_SENSOR_PIN
    logMsg += "Rain Fact: " + String(RAIN_GAUGE_FACTOR, 3) + " | ";
#endif
#ifdef HAS_WIND_DIRECTION
    logMsg += "Nord Off: " + String(WIND_NORTH_OFFSET, 1) + " Inv: " + String(WIND_DIRECTION_INVERT ? "SI" : "NO") + " | ";
#endif

    // Log Ventola
#ifdef FAN_RELAY_PIN
    logMsg += "Ventola T: " + String(fan_temp_start, 1) + "/" + String(fan_temp_stop, 1) + "C ";
    #if defined(HAS_HUMIDITY)
    logMsg += "U: " + String(fan_hum_start, 0) + "/" + String(fan_hum_stop, 0) + "% ";
    #endif
    logMsg += "| ";
#endif

    // Log Batteria
#ifdef FORCE_SLEEP_MV
    logMsg += "Batt sleep: " + String(force_sleep_mv) + "mV Wakeup: " + String(force_wakeup_mv) + "mV (sleep per: " + String(force_wakeup_hr) + "h)";
#endif

    // Invio definitivo della stringa al terminale di Meshtastic
    LOG_INFO("%s", logMsg.c_str());
}



void salvavariabili() {
#if defined(ARCH_ESP32) || defined(ESP32)
    Preferences prefs;
    if (prefs.begin("firmware", false)) { 
        prefs.putString("cmd_pass", firmware_cmd_password);
        prefs.putInt("reb_days", auto_reboot_days);
        prefs.putBool("cln_ndb", firmware_clean_also_nodedb);
        prefs.putBool("kp_pref", firmware_keep_preferred);
        prefs.putFloat("r_factor", RAIN_GAUGE_FACTOR);
        prefs.putFloat("g_anemo", ANEMOMETRO_GUADAGNO);
        prefs.putFloat("a_anemo", ANEMOMETRO_ATTRITO);
        prefs.putFloat("n_offset", WIND_NORTH_OFFSET);
        prefs.putBool("w_invert", WIND_DIRECTION_INVERT);
        prefs.putFloat("f_t_start", fan_temp_start);
        prefs.putFloat("f_t_stop", fan_temp_stop);
        prefs.putFloat("f_h_start", fan_hum_start);
        prefs.putFloat("f_h_stop", fan_hum_stop);
        prefs.putInt("s_mv", force_sleep_mv);
        prefs.putInt("w_mv", force_wakeup_mv);
        prefs.putInt("w_hr", force_wakeup_hr);
        prefs.end();
    }
#elif defined(NRF52_SERIES)
    InternalFS.remove("/firmware.dat");
    File file(InternalFS);
    if (file.open("/firmware.dat", FILE_O_WRITE)) {
        file.write((const uint8_t*)firmware_cmd_password, sizeof(firmware_cmd_password));
        file.write((const uint8_t*)&auto_reboot_days, sizeof(auto_reboot_days));
        file.write((const uint8_t*)&firmware_clean_also_nodedb, sizeof(firmware_clean_also_nodedb));
        file.write((const uint8_t*)&firmware_keep_preferred, sizeof(firmware_keep_preferred));
        file.write((const uint8_t*)&RAIN_GAUGE_FACTOR, sizeof(RAIN_GAUGE_FACTOR));
        file.write((const uint8_t*)&ANEMOMETRO_GUADAGNO, sizeof(ANEMOMETRO_GUADAGNO));
        file.write((const uint8_t*)&ANEMOMETRO_ATTRITO, sizeof(ANEMOMETRO_ATTRITO));
        file.write((const uint8_t*)&WIND_NORTH_OFFSET, sizeof(WIND_NORTH_OFFSET));
        file.write((const uint8_t*)&WIND_DIRECTION_INVERT, sizeof(WIND_DIRECTION_INVERT));
        file.write((const uint8_t*)&fan_temp_start, sizeof(fan_temp_start));
        file.write((const uint8_t*)&fan_temp_stop, sizeof(fan_temp_stop));
        file.write((const uint8_t*)&fan_hum_start, sizeof(fan_hum_start));
        file.write((const uint8_t*)&fan_hum_stop, sizeof(fan_hum_stop));
        file.write((const uint8_t*)&force_sleep_mv, sizeof(force_sleep_mv));
        file.write((const uint8_t*)&force_wakeup_mv, sizeof(force_wakeup_mv));
        file.write((const uint8_t*)&force_wakeup_hr, sizeof(force_wakeup_hr));
        file.close();
    }
#endif
}



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

    MeshService::globalPrivateBuffer = std::string(msg).substr(0, 200);
    
    if (EnvironmentTelemetryModule::instance != nullptr) {
        EnvironmentTelemetryModule::pendingMqttPublish = true; // solo flag, niente chiamate
    }

}


bool checkcomandi(const meshtastic_MeshPacket *p) {
    if (p == nullptr || p->which_payload_variant != meshtastic_MeshPacket_decoded_tag || 
        p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP) return false;

    bool direttoANoi = (p->to == nodeDB->getNodeNum());
    bool localeDaAPI = (p->from == nodeDB->getNodeNum());
    bool sistemaLocale = (p->from == 0);
    if (!direttoANoi && !localeDaAPI && !sistemaLocale) return false;
    
    const size_t len = p->decoded.payload.size;
    if (len == 0 || len > 200) return false;

    char tmpbuf[201];
    memcpy(tmpbuf, p->decoded.payload.bytes, len);
    tmpbuf[len] = '\0';
    String msg(tmpbuf);
    msg.trim();

    if (!msg.startsWith(firmware_cmd_password)) return false;

    String cmd = msg.substring((unsigned int)strlen(firmware_cmd_password));
    cmd.trim();
    if (cmd.length() == 0) return false;

    // --- FUNZIONI HELPER DINAMICHE ---
    auto updateFloat = [&](const char* c, float& var, const char* label) -> bool {
        String prefix = String(c) + " ";
        if (cmd.startsWith(prefix)) {
            var = cmd.substring(prefix.length()).toFloat();
            salvavariabili();
            sendConfirm(p, ("OK: " + String(c) + " a: " + String(var)).c_str());
            return true;
        }
        return false;
    };

    auto updateInt = [&](const char* c, int& var, const char* label) -> bool {
        String prefix = String(c) + " ";
        if (cmd.startsWith(prefix)) {
            var = cmd.substring(prefix.length()).toInt();
            salvavariabili();
            sendConfirm(p, ("OK: " + String(c) + " a: " + String(var)).c_str());
            return true;
        }
        return false;
    };

    // --- LOGICA COMANDI ---
if (cmd.equals(LISTA_COMANDI)) {
        // Struttura dati per passare i messaggi e il pacchetto al task asincrono
        struct TaskArgs {
            meshtastic_MeshPacket pkt;
            String msg1;
            String msg2;
            String msg3;
        };

        // Allociamo dinamicamente gli argomenti per il task
        TaskArgs *args = new TaskArgs();
        if (args != nullptr) {
            // Copiamo il pacchetto di richiesta per mantenere intatti i riferimenti a canali e nodi
            if (p != nullptr) {
                args->pkt = *p;
            }
            
            // Componiamo le stringhe
            args->msg1 = "1/3 Sys: " + String(COMANDO_STATO) + ", " + String(CMD_PASS) + ", " + String(CMD_REBOOT);
            args->msg2 = "2/3 Fan: " + String(CMD_FAN_T_START) + ", " + String(CMD_FAN_T_STOP) + ", " + String(CMD_FAN_H_START) + ", " + String(CMD_FAN_H_STOP) + ", Batt: " + String(CMD_BATT_SLEEP) + ", " + String(CMD_BATT_WAKE);
            args->msg3 = "3/3 Meteo/Relay: " + String(CMD_GUADAGNO) + ", " + String(CMD_ATTRITO) + ", " + String(CMD_RAIN) + ", " + String(CMD_INV_VENTO) + ", " + String(CMD_DIR_VENTO) + ", " + String(CMD_RELAY0) + " on/off, " + String(CMD_RELAY1) + " on/off" + ", " + String(CMD_RELAY2) + " on/off";

            // Creiamo un task FreeRTOS a bassissima priorità che gira in background
            xTaskCreate(
                [](void *pvParameters) {
                    TaskArgs *dati = static_cast<TaskArgs*>(pvParameters);
                    
                    if (dati != nullptr) {
                        
                        // Invia il primo messaggio immediatamente
                        sendConfirm(&(dati->pkt), dati->msg1.c_str());
                        vTaskDelay(pdMS_TO_TICKS(1500)); // Attesa non bloccante per il core (400 ms)
                        
                        // Invia il secondo
                        sendConfirm(&(dati->pkt), dati->msg2.c_str());
                        vTaskDelay(pdMS_TO_TICKS(1500));
                        
                        // Invia il terzo
                        sendConfirm(&(dati->pkt), dati->msg3.c_str());
                        
                        // Liberiamo la memoria allocata
                        delete dati;
                    }
                    
                    // Un task FreeRTOS deve obbligatoriamente auto-eliminarsi alla fine
                    vTaskDelete(NULL);
                },
                "async_send_cmd",   // Nome del task
                4096,               // Stack size (4KB sono sicuri per le conversioni String/memcpy)
                args,               // Parametri passati al task
                1,                  // Priorità molto bassa (lascia respirare la radio e il resto del sistema)
                NULL                // Task handle non necessario
            );
        }
        return true;
    }
	
	

    if (cmd.equals(COMANDO_STATO)) {
        char stato[256];
        snprintf(stato, sizeof(stato), "Sys: Reb %dg, Cln %s | Vento: Off %.0f Inv %s | Anem: G%.2f A%.2f | Rain: %.3f | Fan: T%.1f/%.1f U%.0f/%.0f | Bat: S%d/W%d mV", 
                 auto_reboot_days, firmware_clean_also_nodedb ? "SI" : "NO", WIND_NORTH_OFFSET, WIND_DIRECTION_INVERT ? "SI" : "NO", 
                 ANEMOMETRO_GUADAGNO, ANEMOMETRO_ATTRITO, RAIN_GAUGE_FACTOR, fan_temp_start, fan_temp_stop, fan_hum_start, fan_hum_stop, force_sleep_mv, force_wakeup_mv);
        sendConfirm(p, stato);
        return true;
    }
	
	
    if (cmd.startsWith(String(CMD_PASS) + " ")) {
        String nPass = cmd.substring(strlen(CMD_PASS) + 1);
        nPass.trim();
        if(nPass.length() >= 4 && nPass.length() < 32) {
            strncpy(firmware_cmd_password, nPass.c_str(), sizeof(firmware_cmd_password) - 1);
            firmware_cmd_password[sizeof(firmware_cmd_password) - 1] = '\0';
            salvavariabili();
            sendConfirm(p, "OK: Password aggiornata");
            return true;
        } else {
            sendConfirm(p, "ERRORE: Pass tra 4-31 caratteri");
            return true;
        }
    }

    // --- UPDATE VALORI ---
    if (updateInt(CMD_REBOOT, auto_reboot_days, "")) return true;
    if (updateInt(CMD_BATT_SLEEP, force_sleep_mv, "")) return true;
    if (updateInt(CMD_BATT_WAKE, force_wakeup_mv, "")) return true;
    
    if (updateFloat(CMD_FAN_T_START, fan_temp_start, "")) return true;
    if (updateFloat(CMD_FAN_T_STOP, fan_temp_stop, "")) return true;
    if (updateFloat(CMD_FAN_H_START, fan_hum_start, "")) return true;
    if (updateFloat(CMD_FAN_H_STOP, fan_hum_stop, "")) return true;
    if (updateFloat(CMD_GUADAGNO, ANEMOMETRO_GUADAGNO, "")) return true;
    if (updateFloat(CMD_ATTRITO, ANEMOMETRO_ATTRITO, "")) return true;
    if (updateFloat(CMD_RAIN, RAIN_GAUGE_FACTOR, "")) return true;
    if (updateFloat(CMD_DIR_VENTO, WIND_NORTH_OFFSET, "")) return true;

    if (cmd.equals(CMD_INV_VENTO)) {
        WIND_DIRECTION_INVERT = !WIND_DIRECTION_INVERT;
        salvavariabili();
        sendConfirm(p, ("OK: " + String(CMD_INV_VENTO) + " a: " + String(WIND_DIRECTION_INVERT ? "SI" : "NO")).c_str());
        return true;
    }

#ifdef RELAY_1_PIN
    if (cmd.startsWith(String(CMD_RELAY1) + " ")) {
        bool stato = cmd.substring(strlen(CMD_RELAY1) + 1).equals("on");
        digitalWrite(RELAY_1_PIN, stato ? HIGH : LOW);
        sendConfirm(p, ("OK: " + String(CMD_RELAY1) + " su: " + String(stato ? "ON" : "OFF")).c_str());
        return true;
    }
#endif
#ifdef RELAY_2_PIN
    if (cmd.startsWith(String(CMD_RELAY2) + " ")) {
        bool stato = cmd.substring(strlen(CMD_RELAY2) + 1).equals("on");
        digitalWrite(RELAY_2_PIN, stato ? HIGH : LOW);
        sendConfirm(p, ("OK: " + String(CMD_RELAY2) + " su: " + String(stato ? "ON" : "OFF")).c_str());
        return true;
    }
#endif

#ifdef RELAY_0_PIN
    if (cmd.startsWith(String(CMD_RELAY0) + " ")) {
        bool stato = cmd.substring(strlen(CMD_RELAY0) + 1).equals("on");
        digitalWrite(RELAY_0_PIN, stato ? HIGH : LOW);
        sendConfirm(p, ("OK: " + String(CMD_RELAY0) + " su: " + String(stato ? "ON" : "OFF")).c_str());
        return true;
    }
#endif

#ifdef FAN_RELAY_PIN
    if (cmd.startsWith(String(CMD_RELAY0) + " ")) {
        bool stato = cmd.substring(strlen(CMD_RELAY0) + 1).equals("on");
        digitalWrite(FAN_RELAY_PIN, stato ? HIGH : LOW);
        sendConfirm(p, ("OK: " + String(CMD_RELAY0) + " su: " + String(stato ? "ON" : "OFF")).c_str());
        return true;
    }
#endif

    sendConfirm(p, "ERRORE: Comando non trovato");
    return true;
}
