# 🍃 Stazione Meteo LoRa - Anemometro & Banderuola

Questo modulo gestisce la lettura della velocità e della direzione del vento utilizzando schede di sviluppo LoRa (come la serie **Heltec V3/V4** basata su ESP32-S3). Il sistema combina un conteggio degli impulsi hardware a bassissima latenza per la velocità e una lettura digitale contactless via bus I2C per la direzione.

---

## 🛠️ Architettura Hardware & Sensori

La stazione meteo è divisa in due componenti principali alloggiate su supporti meccanici stampati in 3D con cuscinetti ad alta scorrevolezza (**SKF 628 / 688** con misure $8 \times 16 \times 5\text{ mm}$).

### 1. Velocità del Vento (Anemometro)
* **Sensore:** `US1881` (Sensore Effetto Hall di tipo **Latch**).
* **Meccanica:** Un cilindro rotante da **2 cm di diametro** ospita due magneti da $4 \times 2\text{ mm}$ posizionati ed incollati agli antipodi esatti ($180^\circ$).
* **Funzionamento:** Il sensore ha memoria di stato. Quando passa il polo **NORD**, l'uscita si attiva (va a GND). Rimane congelata a GND finché la girandola non compie mezzo giro e il polo **SUD** resetta il sensore. Questo genera un'onda quadra perfetta priva di rimbalzi meccanici, ideale per gestire le alte velocità di rotazione senza falsi trigger.

### 2. Direzione del Vento (Banderuola)
* **Sensore:** `AS5600` (Encoder Magnetico Rotativo Contactless a **12-bit**).
* **Meccanica:** Un magnete specifico a magnetizzazione **diametrale** posizionato sulla punta dell'asse rotante della banderuola, allineato a circa 1 mm di distanza dal chip.
* **Funzionamento:** Comunica via bus **I2C**. Il sensore calcola l'angolo esatto del campo magnetico da $0^\circ$ a $360^\circ$ con una risoluzione di 4096 posizioni, permettendo di individuare la direzione millimetrica del vento senza attriti.

---

## 🔌 Collegamenti Elettrici e Mappatura PIN

I sensori lavorano direttamente sui livelli logici a **3.3V** della Heltec per evitare danni ai GPIO del microcontrollore. Di seguito lo schema di collegamento testato e funzionante.

| Sensore | Pin Sensore | Pin Heltec LoRa V4 | Note e Stato Logico |
| :--- | :--- | :--- | :--- |
| **US1881** (Velocità) | VCC | **3.3V** | Alimentazione logica del chip |
| | GND | **GND** | Massa comune |
| | OUT | **GPIO 47** (`WIND_SPEED_PIN`) | Configurato in `INPUT_PULLUP` / Fronte `FALLING` |
| **AS5600** (Direzione) | VCC | **3.3V** | Alimentazione logica del chip |
| | GND | **GND** | Massa comune |
| | SDA | **GPIO 21** (SDA) | Condiviso sul bus I2C (OLED/Sensori) |
| | SCL | **GPIO 22** (SCL) | Condiviso sul bus I2C (OLED/Sensori) |
| | DIR | **GND** | Fissato a massa per incremento gradi in senso orario |

> ⚠️ **Nota di Installazione sul Tetto:** Poiché l'US1881 lavora in modalità Open-Drain (tira a massa quando attivo), l'uso del pull-up è obbligatorio. Per installazioni all'esterno con cavi lunghi, è fortemente consigliato aggiungere una resistenza di **Pull-Up esterna fisica da 4.7 kΩ** tra il pin di OUT e la linea 3.3V per tagliare alla radice i disturbi elettromagnetici indotti dal cavo.

---

## 💻 Configurazione delle Direttive Firmware

Il comportamento del codice è regolato da direttive di precompilazione (`#define`) per isolare i moduli software e risparmiare risorse sulla MCU quando i sensori non sono fisicamente connessi:

```cpp
// --- CONFIGURAZIONE ANEMOMETRO (VELOCITÀ) ---
#define WIND_SPEED_PIN 47       // GPIO dedicato all'interruzione hardware (es. Heltec V4)

// --- CONFIGURAZIONE BANDERUOLA (DIREZIONE) ---
#define HAS_WIND_DIRECTION      // Abilita il modulo software I2C per l'AS5600
