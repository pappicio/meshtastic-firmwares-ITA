# 🍃 Stazione Meteo LoRa - Anemometro & Banderuola

Questo modulo gestisce la lettura della velocità e della direzione del vento utilizzando schede di sviluppo LoRa (come la serie **Heltec V3/V4** basata su ESP32-S3). Il sistema combina un conteggio degli impulsi hardware a bassissima latenza per la velocità e una lettura digitale contactless via bus I2C per la direzione.

---

## 🛠️ Architettura Hardware & Sensori

La stazione meteo è divisa in due componenti principali alloggiate su supporti meccanici stampati in 3D con cuscinetti ad alta scorrevolezza (**SKF 628 / 688** con misure $8 \times 16 \times 5\text{ mm}$).

### 1. Velocità del Vento (Anemometro)
* **Sensore:** `US1881` (Sensore Effetto Hall di tipo **Latch**).
* **Meccanica:** Un cilindro rotante da **4 cm di diametro** ospita due magneti da $4 \times 2\text{ mm}$ posizionati ed incollati agli antipodi esatti ($180^\circ$).
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

## 🖨️ Guida alla Stampa 3D e Modularità Meccanica

Il design meccanico sfrutta componenti intelligenti ed intercambiabili che permettono di assemblare sia l'anemometro che la banderuola ottimizzando i pezzi da stampare. Le tolleranze di accoppiamento e scorrimento sono già calcolate all'interno dei modelli CAD.

---

### 📦 Componenti da Stampare (Lista dei Pezzi)

| Nome File STL / Pezzo | Quantità (Velocità) | Quantità (Direzione) | Note e Modularità |
| :--- | :---: | :---: | :--- |
| **Corpo Centrale** | 1 | 1 | Cilindro esterno da **4.5 cm (45 mm)**. Integra già nativamente la sede interna e i **4 fori di fissaggio per l'AS5600**. Se usato per la sola velocità, il sensore I2C non va montato. |
| **Rotore** | 1 | 1 | Cilindro rotante da **4.5 cm (45 mm)**. Presenta un perno/cilindro interno dove calza a pressione il singolo cuscinetto (ne serve **1 per ogni pezzo stampato**, non 2). All'estremità inferiore include l'alloggio da **4x2 mm** per il magnete dell'AS5600. |
| **Coppetta Anemometro** | **3** | 0 | Le tazze tachimetriche per catturare il vento. |
| **Banderuola** | 0 | **1** | La freccia direzionale per l'orientamento. |
| **Tappo Rotore (Chiusura)** | 0 | **3** | **Solo per Direzione:** Chiude i 3 slot sul rotore destinati alle coppette per renderlo aerodinamico. |
| **Tappo Superiore** | 1 | 1 | Chiude il blocco superiore. Nella configurazione banderuola, la freccia si avvita direttamente sopra di esso. |
| **Tappo Inferiore** | 1 | 1 | Sigilla la base inferiore del corpo centrale proteggendo i cavi. |

---

### 📐 Impostazioni Consigliate nello Slicer (Orca Slicer / Cura / PrusaSlicer)

* **Materiale:** PETG (o ASA).
* **Altezza Strato (Layer Height):** 0.20 mm (Asse Z).
* **Perimetri (Gusci):** Almeno **3 o 4 perimetri** per dare massima filettabilità e tenuta strutturale alle viti M3.
* **Infill (Riempimento):** 20% - 30% di tipo **Giroide** (Gyroid) per una resistenza uniforme e per evitare che la struttura risulti troppo pesante per girare con brezze leggere.
* **Tolleranze delle Sedi:** Se la tua stampante tende a stringere leggermente i fori interni dei cerchi, utilizza la compensazione dimensionale dei fori nello slicer (es. `X-Y Hole Compensation` a +0.1 mm) per far calzare a pennello il cuscinetto da 16 mm.

---

### 🧲 Configurazione dei Magneti e Logica di Funzionamento

Il **Rotore da 4.5 cm** è ingegnerizzato con una predisposizione interna flessibile per i magneti a seconda dell'uso dello strumento:

#### Per l'Anemometro (Velocità - US1881):
Il rotore dispone internamente di **4 alloggiamenti totali**: 2 slot per magneti al neodimio da **8x3 mm** e 2 slot per magneti da **4x2 mm**.
* **Regola di Montaggio:** I magneti scelti vanno inseriti in **senso contrapposto l'uno dall'altro (sfasati di 180°)** sulla circonferenza. Un lato deve esporre il polo **NORD** verso il sensore US1881 e il lato opposto il polo **SUD**. Questa alternanza geometrica pulisce il segnale da oscillazioni o sobbalzi software in caso di turbolenze, generando un'onda quadra perfetta ad ogni giro, senza attivazioni anomale.

#### Per la Banderuola (Direzione - AS5600):
Sfrutta l'alloggiamento dedicato da **4x2 mm** posizionato esattamente all'estremità dell'asse del cilindro interno del rotore.
* **Regola di Montaggio:** Ospita il magnete a magnetizzazione **diametrale**. Il magnete ruota direttamente sopra il chip AS5600 (alloggiato nei 4 fori del corpo centrale), mantenendo le tolleranze di distanza ideali per la lettura a 12-bit dell'angolo.

---

### 🔩 Ferramenta e Componenti Non Stampati (BOM)

* **Cuscinetti ad Alta Scorrevolezza (Modello 688 / SKF 628):**
  * **Quantità:** 1 pezzo inserito direttamente sul perno interno di ciascun rotore stampato.
  * **Misure Esatte:** Diametro interno **8 mm**, diametro esterno **16 mm**, spessore **5 mm**.
  * *Consiglio pratico:* Reperibili a basso costo online o presso i ricambisti auto. Per l'anemometro si consiglia di rimuovere lo schermo protettivo, lavare via il grasso denso di fabbrica con diluente o avio e lubrificare con una sola goccia di olio leggerissimo (tipo Singer) per massimizzare la sensibilità alle brezze leggere.
* **Viteria:**
  * **Modello:** Viti **M3 con la base testa piatta** (ISO 10642 / DIN 7991). Utilizzate per assemblare in modo solido e a filo i tappi, le coppette e la banderuola.


---

## 💻 Configurazione delle Direttive Firmware

Il comportamento del codice è regolato da direttive di precompilazione (`#define`) per isolare i moduli software e abilitare le funzioni solo quando i sensori sono fisicamente connessi.

```cpp
// --- CONFIGURAZIONE ANEMOMETRO (VELOCITÀ) ---
#define WIND_SPEED_PIN 47       // GPIO dedicato all'interruzione hardware (es. Heltec V4)

// --- CONFIGURAZIONE BANDERUOLA (DIREZIONE) ---
// Per attivare il sensore di direzione basta decommentare la riga seguente:
#define HAS_WIND_DIRECTION      

// Se la direzione è attiva, imposta un offset di default (es. 120°) se non già definito altrove
#ifdef HAS_WIND_DIRECTION
    #define WIND_NORTH_OFFSET 120.0f  // Macro di taratura del Nord (0.0 a 359.0)
#endif
```
---
## 💻 TESTATO con magneti 4x2 mm, per anemometro e funziona ameraviglia con sensore hall U18 

(presi su amazon, ma anche aliexpress ne ha a dozzine a 4 spicci)

TESTATO Direzione vento con sensore i2c AS5600, piu che funzionale 
(presi su amazon comprensivi di magneti 4x2 mm ma anche su aliexpress ce ne sono.)

The end!!!
---
