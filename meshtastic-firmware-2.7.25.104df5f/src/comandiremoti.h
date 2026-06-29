
#pragma once
#ifndef COMANDIREMOTI_H
#define COMANDIREMOTI_H

#include "mesh/generated/meshtastic/mesh.pb.h"
#include "main.h"
#include "configuration.h" 

// --- 2. COMANDI (Senza spazi finali) ---
#define CMD_PASS          "nuova password"
#define CMD_REBOOT        "reboot giorni"
#define LISTA_COMANDI     "lista comandi"
#define CMD_FAN_T_START   "ventola tstart"
#define CMD_FAN_T_STOP    "ventola tstop"
#define CMD_FAN_H_START   "ventola hstart"
#define CMD_FAN_H_STOP    "ventola hstop"
#define CMD_BATT_SLEEP    "batteria sleep"
#define CMD_BATT_WAKE     "batteria wake"
#define CMD_GUADAGNO      "guadagno anemometro"
#define CMD_ATTRITO       "attrito anemometro"
#define CMD_RAIN          "fattore pioggia"
#define CMD_INV_VENTO     "inverti vento"
#define CMD_DIR_VENTO     "direzione vento"
#define CMD_RELAY0        "fanrelay" // on o off
#define CMD_RELAY1        "relay1" // on o off
#define CMD_RELAY2        "relay2" // on o off per accendere / spegnere
#define COMANDO_STATO     "stato sensori"

// --- 3. VARIABILI GLOBALI (Inline per inclusione multipla sicura) ---
inline char firmware_cmd_password[32] = CMD_PASSWORD; 
inline int auto_reboot_days = AUTO_REBOOT_DAYS;
inline bool firmware_clean_also_nodedb = true; 
inline bool firmware_keep_preferred = true;
inline float ANEMOMETRO_GUADAGNO = 0.702f;
inline float ANEMOMETRO_ATTRITO = 0.30f;
inline float RAIN_GAUGE_FACTOR = 337.888f;
inline float WIND_NORTH_OFFSET = 0.0f;
inline bool WIND_DIRECTION_INVERT = false;
inline int force_sleep_mv = 3400;
inline int force_wakeup_mv = 3700;
inline int force_wakeup_hr = 12;
inline bool force_deepsleep_enabled = true;
inline float fan_temp_start = 42.0f;
inline float fan_temp_stop  = 35.0f;
inline float fan_hum_start  = 80.0f;
inline float fan_hum_stop   = 60.0f;
inline int ABSOLUTE_SHUTDOWN_COUNT = 5;


 

// --- 4. FUNZIONI ---
void caricavariabili();
void salvavariabili();

void caricaRelay();
void salvaRelay();


bool checkcomandi(const meshtastic_MeshPacket *p);

#endif // COMANDIREMOTI_H