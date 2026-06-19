
#pragma once
#ifndef COMANDIREMOTI_H
#define COMANDIREMOTI_H

#include "mesh/generated/meshtastic/mesh.pb.h"
#include "main.h"
#include "configuration.h"

// --- 1. DEFAULT DI SICUREZZA (Solo Preprocessore) ---
#ifndef FAN_TEMP_START
    #define FAN_TEMP_START -999.0f
    #define FAN_TEMP_STOP  -999.0f
#endif
#ifndef FAN_HUM_START
    #define FAN_HUM_START 0.0f
    #define FAN_HUM_STOP  0.0f
#endif

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
inline float RAIN_GAUGE_FACTOR = 337.876f;
inline float WIND_NORTH_OFFSET = 0.0f;
inline bool WIND_DIRECTION_INVERT = false;

inline float fan_temp_start = FAN_TEMP_START;
inline float fan_temp_stop  = FAN_TEMP_STOP;
inline float fan_hum_start  = FAN_HUM_START;
inline float fan_hum_stop   = FAN_HUM_STOP;

inline int force_sleep_mv = FORCE_SLEEP_MV;
inline int force_wakeup_mv = FORCE_WAKEUP_MV;
inline int force_wakeup_hr = FORCE_WAKEUP_HR;

// --- 4. FUNZIONI ---
void caricavariabili();
void salvavariabili();
bool checkcomandi(const meshtastic_MeshPacket *p);

#endif // COMANDIREMOTI_H