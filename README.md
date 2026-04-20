# meshtastic-firmwares-ITA
***meshtastic-firmware-2.7.22.96dd647 (ultima release alpha) meshtastic-firmware-2.7.15. (ultima release beta)***


🇮🇹 Meshtastic Italia "Smart Power" Edition


Firmware Modificato - Pronto all'uso per meshtastic italia!
Questo non è il solito firmware. È una versione "Zero Configuration" pensata per gli utenti italiani, che integra la potenza del mesh network con il controllo domotico avanzato.

🔥 Cosa c'è "sotto il cofano"?
🛰️ Canali Pre-Configurati (8 Slot)
Dimentica la configurazione manuale. Al primo avvio troverai già impostati:

Slot 0: MediumFast (Default)

Slot 1-6: Canali Regionali (Ita-Nord, Centro, Sud, Help, Test, Shop) con PSK dedicata.

Slot 7: Canale Nazionale "Italia".

🕹️ Domotica Integrata (Password Protected)
Gestisci i tuoi carichi via radio con messaggi criptati:

Relay 1 ("luce"): GPIO 2.

Relay 2 ("pompa"): GPIO 5.

Comandi: Utilizza le password preimpostate ApritiSesamo_123! e ChiuditiSesamo_123!.

❄️ Cooling System  
### ❄️ Sistema di Raffreddamento Intelligente
Il cuore del progetto è la gestione dinamica della temperatura per evitare il ***thermal throttling*** dei componenti (ESP32/LoRa) all'interno di box stagni esposti al sole.

* **Controllo Isteresi:** Attivazione automatica ventola a **42°C** e spegnimento a **35°C** (previene cicli ON/OFF troppo brevi).
* **Fail-Safe Logic:** In caso di errore del sensore, il sistema logga l'anomalia e mantiene lo stato di sicurezza per proteggere l'hardware.
* **Hardware:** Testato su **Heltec V3/V4** con ventole 5V/12V tramite transistor o modulo relay su ***GPIO 1***.

---

### 📡 Matrice Sensori I2C (Rilevamento Automatico)
Il firmware esegue uno scanning del bus I2C all'avvio, supportando nativamente una vasta gamma di sensori senza necessità di ricompilazione. Il sistema riconosce gli indirizzi e adatta l'algoritmo di lettura:

| Indirizzo I2C | Sensori Supportati | Caratteristiche |
| :--- | :--- | :--- |
| **0x76 / 0x77** | **BME280 / BMP280** | Pressione, Umidità e Temperatura (Alta precisione). |
| **0x44 / 0x45** | **SHT3x (SHT30/31/35)** | Sensori professionali Sensirion, ideali per esterni. |
| **0x38** | **AHT10 / AHT20 / AHT21** | Calibrazione di fabbrica, lettura rapida e stabile. |
| **0x40** | **SI7021 / HTU21D** | Ottima stabilità termica e ingombro ridotto. |

 
---
### 🔌 Supporto Sensori Legacy e Alternativi
Oltre ai sensori I2C, la variabile di controllo ***fanTemp*** può essere alimentata da:
* **OneWire (DS18B20):** Ideale per sonde digitali cablate su lunghe distanze.
* **Analogico (NTC / LM35):** Lettura diretta tramite ADC per setup a basso costo.
* **DHT Series (DHT11 / DHT22):** Supporto per i classici sensori "single-wire".

---

### 📊 Dashboard "1-8-7" su App Meshtastic
Abbiamo rivoluzionato il campo **Current (A)** della telemetria. Non leggerai milliampere casuali, ma un ***display di stato digitale a 3 cifre***.

Ogni posizione numerica rappresenta un dispositivo:
**Cifra 1: Ventola | Cifra 2: Relay 1 (Luce) | Cifra 3: Relay 2 (Pompa)**

#### 💡 Legenda Codici:
* **1**: **ON** (Dispositivo attivo)
* **8**: **OFF** (Dispositivo spento)
* **7**: **INATTIVO** (Pin non configurato o sensore assente)

> **Esempio:** Se l'app mostra **187.0 A**, significa: Ventola **ON**, Relay 1 **OFF**, Relay 2 **NON PRESENTE**.

---

### ⚙️ Guida alla Configurazione (src/configuration.h)

Modifica queste macro per adattare il firmware al tuo hardware:

```cpp
// --- SENSORI DI TEMPERATURA ---
#define I2C_FAN_SENSOR_ADDR 0x76  // 0x76 o 0x77 per BME/BMP
#define ONEWIRE_TEMP_PIN 4        // Pin per DS18B20 (opzionale)
#define DHT_TEMP_PIN 14           // Pin per DHT11/22 (opzionale)

// --- SOGLIE TERMICHE ---
#define FAN_TEMP_START 42.0f      // Accensione Ventola
#define FAN_TEMP_STOP 35.0f       // Spegnimento Ventola

// --- MAPPING GPIO RELAY ---
#define FAN_RELAY_PIN 1           // Posizione 1 della Dashboard
#define RELAY_1_PIN 2             // Posizione 2 della Dashboard
#define RELAY_2_PIN 5             // Posizione 3 della Dashboard

```

### 🛠️ Configurazione GPIO (Ottimizzata)
I pin sono stati scelti per garantire la massima stabilità ed evitare conflitti con il display OLED e i sensori interni dell'Heltec:
* **Cooling Fan:** `GPIO 1`
* **Relay 1 (Luce):** `GPIO 2`
* **Relay 2 (Pompa):** `GPIO 5`
---

### 🔗 Integrazione Telemetria
I dati vengono ***iniettati*** nei pacchetti standard di Meshtastic per essere visibili ovunque:
1.  **Voltage Field:** Mostra la temperatura reale del box in °C (es. `45.2 V` = 45.2°C).
2.  **Current Field:** Mostra la Dashboard di stato dei Relay (es. `111 A` = Tutto acceso).
3.  **Piena compatibilità:** Funziona con App Meshtastic, MQTT, Grafana e i log seriali.

---
***"Perché un box che monitora se stesso è un box che non ti lascerà mai a piedi."***


⚡ Ottimizzazioni di Sistema
Regione: EU_868 (Italia/Europa).

Modem Preset: MediumFast (equilibrio perfetto tra portata e velocità).

MQTT: Root topic già impostato su msh/EU_868/IT.

Auto-Reboot: Ogni 5 giorni per garantire la massima stabilità del nodo. e contestualmente pulizia dei nodi ad ogni reboot

Timezone: Italia (GMT+1 con gestione ora legale) pre-impostata.

🎨 Personalizzazione Grafica
Splash Screen:  testo e immagine

Owner Name: "Smart Power Test" (Short: SPT1)

Ringtone: Suoneria personalizzata inclusa!

🛠️ Note Tecniche per il Deploy
Tutte le impostazioni sopra citate sono cablate nel file configuration.h. Per modificare le password o i nomi dei canali intervenire in quel file!

⚖️ Licenza e Community
Distribuito sotto licenza GPL v3.
Basato sul codice originale di Meshtastic. Si ringrazia la community italiana per la definizione dei canali standard.

🚀 Come installare
Scarica il sorgente.

Apri con PlatformIO.

Seleziona il target (heltec-v4 o tutti gli altri, attenzione sempre ai pin liberi da poter utilizzare e alla dimensione immagine custom!)

Flash e goditi il tuo nodo "Full Optional".


# 🛠 Guida alla Compilazione

1. Requisiti Software
   
***Visual Studio Code (VS Code)***: È l'ambiente di sviluppo principale. Scaricalo da [www.code.visualstudio.com.](https://code.visualstudio.com/download)

***PlatformIO:*** È il plugin "magico" che gestisce le librerie e compila il codice per l'ESP32.

Apri VS Code.

Clicca sull'icona delle Estensioni (quella con i quattro quadratini a sinistra).

Cerca PlatformIO IDE e clicca su Install.

2. Preparazione del Progetto
   
Scarica questo repository come file .zip e scompattalo sul tuo PC (o usa git clone).

In VS Code, vai su File -> Open Folder e seleziona la cartella principale del firmware (quella che contiene il file platformio.ini).

Aspetta qualche minuto: PlatformIO scaricherà automaticamente tutte le librerie necessarie e i driver per la Heltec V4.

3. Personalizzazione (Opzionale)
   
Apri il file configuration.h per:

Cambiare le password (CMD_RELAY_ON).

Modificare il nome del tuo nodo (USERPREFS_CONFIG_OWNER_LONG_NAME).

Regolare le soglie della ventola.

4. Compilazione e Flash
   
Collega la tua Heltec V4 o cmq il tuo device lora al PC tramite il cavo USB-C.

Guarda la barra blu in basso in VS Code.

Compilazione (Build): Clicca sull'icona del segno di spunta (✓). Questo verificherà che non ci siano errori.

Caricamento (Upload): Clicca sull'icona della freccia verso destra (→). PlatformIO caricherà il firmware direttamente sulla tua scheda.

Monitor Seriale: Clicca sull'icona della spina o del piccolo monitor per vedere in tempo reale cosa sta facendo il nodo (vedrai i log della temperatura e i comandi ricevuti).

***se volete per il flash potete anche usare il tool: Tasmota esp flasher Windows*** (https://github.com/Jason2866/ESP_Flasher/releases/download/v4.4.0/ESP-Flasher-Windows.zip)

anzi, ve lo consiglio se dovreste avere problemi con visual studio code, io mi trovo benisismo a flashare gli esp32 con questo tool!

⚠️ Risoluzione Problemi Comuni
Errore di connessione (Serial Port): Assicurati di avere i driver USB installati (solitamente i CP210x o i driver per ESP32-S3).

Errore di memoria: Se la compilazione fallisce, prova a cliccare sull'icona del cestino (Clean) in basso e poi riprova l'Upload.

Antivirus: Alcuni antivirus bloccano i processi di compilazione. Se ricevi errori strani, prova a disattivarlo temporaneamente.

# 🚀 Meshtastic Logo Creator
Questo strumento è stato progettato per generare rapidamente il codice sorgente necessario per personalizzare lo Splash Screen (logo di avvio) dei dispositivi Meshtastic (Heltec V3/V4, T-Beam, ecc.).

con formato compatibile variabile macro dei sorgenti di questo progetto!!!!

📂 Come avviarlo
L'eseguibile si trova nella cartella principale di questo progetto e si chiama:   
***Meshtastic_logo_creator.exe***

✨ Funzioni Principali
Il tool automatizza tutto il processo di conversione tecnica, permettendoti di concentrarti solo sulla grafica:

Compatibilità Universale: Puoi caricare qualsiasi tipo di immagine (JPG, PNG, BMP, GIF, WebP).

Gestione Colori: Anche se carichi un'immagine a 32-bit (milioni di colori), il software la convertirà istantaneamente in Bianco e Nero puro (1-bit) usando un algoritmo di luminosità pesata.

Auto-Ridimensionamento: Non importa se la tua foto originale è enorme (es. 640x480 o Full HD). Il tool la ridimensionerà automaticamente ai parametri impostati.

Dimensioni Dinamiche: Tramite i selettori numerici, puoi definire la dimensione finale (default 128x64 per display OLED standard).

Anteprima Doppia:

Zoom View (3x): Per controllare ogni singolo pixel e la leggibilità dei testi.

Real Size (1:1): Per vedere esattamente quanto spazio occuperà il logo sullo schermo fisico della tua Heltec.

Inversione Colori: Un tasto dedicato permette di invertire il Bianco con il Nero se il logo appare "al negativo".

🛠 Istruzioni per l'uso
Carica: Clicca su "Carica Immagine" e seleziona il tuo file.

Configura: Verifica le dimensioni nei selettori (X e Y). Se l'immagine appare schiacciata, regola i valori o prepara un'immagine con rapporto 2:1.

Copia: Clicca sul tasto "Copia Codice". Negli appunti verrà salvato l'intero blocco di codice C++.

Incolla: Apri il file src/configuration.h nel tuo progetto firmware (VS Code / PlatformIO), cerca le variabili dello splash screen e incolla il tutto sovrascrivendo le vecchie linee.

📝 Formato Output
Il codice generato segue lo standard SSD1306 Page Addressing, necessario per il corretto funzionamento dei display OLED su firmware Meshtastic:

Screenshot del converter in azione:

![Logo](meshtastic_logo_creator.jpg)


## Credits & Legal Notice

This project is a customized version of the **Meshtastic** firmware. 

* **Original Project:** [Meshtastic](https://meshtastic.org/)
* **Source Code:** [Meshtastic GitHub Repository](https://github.com/meshtastic/firmware)
* **License:** This software is released under the **GNU General Public License v3.0**, in compliance with the original project requirements.

### About this project
This repository includes specific modifications and hardware optimizations and some config customizations.

---
