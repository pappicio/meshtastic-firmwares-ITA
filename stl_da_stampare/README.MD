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

```

---

## 🖨️ Guida alla Stampa 3D e Assemblaggio Meccanico

Il design meccanico è completamente modulare. Utilizza componenti comuni che possono essere stampati sia per configurare l'Anemometro (velocità) sia la Banderuola (direzione), riducendo la necessità di progettare parti diverse da zero.

---

### 📦 Componenti da Stampare (Lista dei Pezzi)

A seconda del sensore che si vuole costruire, stampare i pezzi indicati nelle quantità specifiche:

| Nome File STL / Pezzo | Quantità (Velocità) | Quantità (Direzione) | Note di Stampa |
| :--- | :---: | :---: | :--- |
| **Corpo Centrale** | 1 | 1 | Struttura esterna con diametro da **4.5 cm (45 mm)**. Ospita i cuscinetti e il sensore (US1881 o AS5600). |
| **Rotore** | 1 | 1 | Il cilindro da **4.5 cm (45 mm)** che ruota sull'asse. All'interno include **4 alloggiamenti per i magneti**. |
| **Coppetta Anemometro** | **3** | 0 | Le tazze per catturare il vento. |
| **Banderuola** | 0 | **1** | La freccia direzionale per l'orientamento del vento. |
| **Tappo Rotore (Chiusura)** | 0 | **3** | **Solo per Direzione:** Chiude i 3 slot sul rotore destinati alle coppette. |
| **Tappo Superiore** | 1 | 1 | Chiude il blocco. Sulla Banderuola, la freccia va avvitata sopra di esso. |
| **Tappo Inferiore** | 1 | 1 | Sigilla la base del corpo centrale per proteggere i collegamenti. |

---

### 🧲 Configurazione e Allineamento dei Magneti nel Rotore

Il **Rotore da 4.5 cm** è ingegnerizzato con una doppia predisposizione interna che prevede **4 alloggiamenti totali**:
* **2 slot** dedicati a magneti al neodimio da **8x3 mm**
* **2 slot** dedicati a magneti da **4x2 mm**

#### ⚠️ Regola di Montaggio Fondamentale per l'US1881 (Anemometro):
I magneti scelti (siano essi i componenti da 8x3 mm o quelli da 4x2 mm) vanno installati obbligatoriamente in **senso contrapposto l'uno dall'altro** (sfasati di $180^\circ$ sulla circonferenza interna):
1. Il primo magnete deve mostrare la faccia con polo **NORD** rivolta verso il sensore.
2. Il secondo magnete, sul lato opposto, deve mostrare la faccia con polo **SUD** rivolta verso il sensore.

Grazie a questa alternanza geometrica perfetta, a ogni giro della girandola il sensore US1881 rileverà prima il passaggio del NORD (attivando l'output a 0) e poi quello del SUD (resettando l'output a 1). Questo sistema latch meccanico genera un'onda quadra pulitissima e **azzera completamente i falsi impulsi o i sobbalzi software** in caso di oscillazioni della girandola dovute a turbolenze.

---

### 🔩 Ferramenta e Componenti Non Stampati (BOM)

I componenti meccanici ed elettronici sono facilmente reperibili online o presso i ricambisti locali a basso costo.

* **Cuscinetti ad Alta Scorrevolezza (Modello 688 / SKF 628):**
  * **Quantità:** 2 pezzi per ogni strumento (totale 4 se si realizzano entrambi).
  * **Misure Esatte:** Diametro interno $\mathbf{8\text{ mm}}$, diametro esterno $\mathbf{16\text{ mm}}$, spessore $\mathbf{5\text{ mm}}$.
  * *Consiglio:* Si trovano a pochissimi euro nei negozi di e-commerce o dai ricambisti auto/moto. Per l'anemometro, sgrassare il grasso di fabbrica interno con diluente o avio e lubrificare con una goccia di olio leggerissimo (tipo Singer) per azzerare l'attrito d'avvio.
* **Viteria:**
  * **Modello:** Viti **M3 a testa piatta svasata** (ISO 10642 / DIN 7991).
  * Servono per fissare i tappi, le coppette al rotore e la banderuola sul tappo superiore.

---

### 📐 Impostazioni Consigliate per lo Slicer (Orca Slicer / Cura / PrusaSlicer)

Per garantire la massima resistenza agli agenti atmosferici (pioggia, raggi UV, gelo) e stabilità dimensionale per le sedi dei cuscinetti:

* **Materiale Consigliato:** **PETG** (ottimo il PETG effetto marmo per estetica e resistenza) o **ASA**. Evitare il PLA poiché si deforma sotto il sole estivo sul tetto.
* **Altezza Strato:** 0.20 mm (per un buon compromesso tra precisione e tempo).
* **Perimetri (Gusci):** Almeno **3 o 4 perimetri** per garantire la robustezza strutturale dove alloggiano le viti M3.
* **Infill (Riempimento):** 20% - 30% di tipo **Giroide** (Gyroid) per una resistenza uniforme in tutte le direzioni senza appesantire la struttura.
* **Tolleranze delle Sedi:** Le sedi dei cuscinetti da 16 mm e dell'albero da 8 mm sono progettate a tolleranza fissa. Se la tua stampante stringe i fori, calibrare il flusso di estrusione o utilizzare la compensazione dimensionale dei fori nello slicer (es. `X-Y Hole Compensation` a +0.1 mm).
