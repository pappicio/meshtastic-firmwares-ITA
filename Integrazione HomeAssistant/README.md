# meshtastic-firmwares-ITA
## Sensori MQTT per Homeassistant

### 📊 Home Assistant Configuration
aggiungi questa configurazione di sensori mqtt, ad esempio in;  `mqtts/mqtt_sensors.yaml` file:

```yaml

  # =====================================================
  #  NODO  !tuo_nodo_id
  # =====================================================
  - name: "Nodo Long Name"
    unique_id: "nodo_long_name_root"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.long_name is defined  %}
        {{ value_json.long_name }}
      {% else %}
        {{ this.state }}
      {% endif %}
    icon: "mdi:tag-text"

  - name: "Nodo Short Name"
    unique_id: "nodo_short_name_root"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.short_name is defined   %}
        {{ value_json.short_name }}
      {% else %}
        {{ this.state }}
      {% endif %}
    icon: "mdi:tag-outline"

  # --- BATTERIA E SISTEMA ---
  - name: "Nodo Battery Voltage"
    unique_id: "nodo_battery_voltage"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    state_class: measurement
    value_template: >-
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.voltage is defined  %}
        {{ (value_json.payload.voltage | float) | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "voltage"
    unit_of_measurement: "V"

  - name: "Nodo Battery Percent"
    unique_id: "nodo_battery_percent"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    state_class: measurement
    value_template: >-
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.battery_level is defined   %}
        {{ (value_json.payload.battery_level | float) | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "battery"
    unit_of_measurement: "%"

  # --- STATISTICHE CANALE ---
  - name: "Nodo Channel Utilization"
    unique_id: "nodo_chutil"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    state_class: measurement
    value_template: >-
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.channel_utilization is defined   %}
        {{ (value_json.payload.channel_utilization | float) | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "%"
    icon: "mdi:chart-line"

  - name: "Nodo Air Utilization TX"
    unique_id: "nodo_airutiltx"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    state_class: measurement
    value_template: >-
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.air_util_tx is defined %}
        {{ (value_json.payload.air_util_tx | float) | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "%"
    icon: "mdi:radio-tower"

  - name: "Nodo Uptime"
    unique_id: "nodo_uptime"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    state_class: total_increasing
    value_template: >-
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.uptime_seconds is defined %}
        {{ value_json.payload.uptime_seconds | int }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "s"
    icon: "mdi:timer-outline"

  - name: "Nodo Uptime Formattato"
    unique_id: "nodo_uptime_formattato"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >-
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.uptime_seconds is defined %}
        {% set s = value_json.payload.uptime_seconds | int %}
        {% set g = (s // 86400) %}
        {% set h = (s % 86400) // 3600 %}
        {% set m = (s % 3600) // 60 %}
        {{ "%d giorni %02d ore %02d min" | format(g, h, m) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    icon: "mdi:timer-outline"

  # --- METEO BME680 ---
  - name: "Nodo Temperatura Esterna"
    unique_id: "nodo_temperature"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.temperature is defined   %}
        {{ value_json.payload.temperature | float | round(1) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "temperature"
    unit_of_measurement: "°C"

  - name: "Nodo Umidita'"
    unique_id: "nodo_humidity"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.relative_humidity is defined  %}
        {{ value_json.payload.relative_humidity | float | round(1) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "humidity"
    unit_of_measurement: "%"

  - name: "Nodo Pressione"
    unique_id: "nodo_pressure"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.barometric_pressure is defined   %}
        {{ value_json.payload.barometric_pressure | float | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "pressure"
    unit_of_measurement: "hPa"

  - name: "Nodo Gas Resistance"
    unique_id: "nodo_gas_resistance"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.gas_resistance is defined  %}
        {{ value_json.payload.gas_resistance | float | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "MOhms"

  # --- VENTO E PIOGGIA ---
  - name: "Nodo Velocita' Vento"
    unique_id: "nodo_wind_speed"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.wind_speed is defined %}
        {{ (value_json.payload.wind_speed | float * 3.6) | round(1) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "km/h"

  - name: "Nodo Direzione Vento"
    unique_id: "nodo_wind_direction"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.wind_direction is defined %}
        {{ value_json.payload.wind_direction | int }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "°"

  - name: "Nodo Raffica Vento"
    unique_id: "nodo_wind_gust"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.wind_gust is defined %}
        {{ (value_json.payload.wind_gust | float * 3.6) | round(1) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "km/h"

  - name: "Nodo bonaccia Vento"
    unique_id: "nodo_wind_lull"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.wind_lull is defined %}
        {{ (value_json.payload.wind_lull | float * 3.6) | round(1) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "km/h"

  - name: "Nodo Pioggia 1h"
    unique_id: "nodo_rainfall_1h"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.rainfall_1h is defined  %}
        {{ value_json.payload.rainfall_1h | float | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "mm"

  - name: "Nodo Pioggia 24h"
    unique_id: "nodo_rainfall_24h"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.rainfall_24h is defined  %}
        {{ value_json.payload.rainfall_24h | float | round(2) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "mm"

  # --- TEMPERATURA BOX & LUX ---
  - name: "Nodo Temperatura BOX"
    unique_id: "nodo_temperatura_box"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.voltagex is defined %}
        {{ (value_json.payload.voltagex | float) | int }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "temperature"
    unit_of_measurement: "°C"

  - name: "Nodo Umidità BOX"
    unique_id: "nodo_umidita_box"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.voltagex is defined %}
        {% set hum = ((value_json.payload.voltagex | float) % 1 * 100) | round(0) | int %}
        {% if hum == 0 %}
          non presente
        {% else %}
          {{ hum }}
        {% endif %}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: "%"

  - name: "Nodo Stato Nodo"
    unique_id: "nodo_stato_nodo"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.currentx is defined %}
        {% set val = value_json.payload.currentx | int %}
        {% set stato = (val // 1000) %}
        {{ {5: "OK", 8: "DeepSleep", 9: "Err.Sensore"}[stato] | default("?") }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: ""

  - name: "Nodo Relay FAN"
    unique_id: "nodo_relay_fan"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.currentx is defined %}
        {% set val = value_json.payload.currentx | int %}
        {% set fan = (val % 1000) // 100 %}
        {{ {0: "Spento", 1: "Acceso"}[fan] | default("?") }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: ""

  - name: "Nodo Relay R1"
    unique_id: "nodo_relay_r1"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.currentx is defined %}
        {% set val = value_json.payload.currentx | int %}
        {% set r1 = (val % 100) // 10 %}
        {{ {0: "OFF", 1: "ON", 2: "N/C"}[r1] | default("?") }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: ""

  - name: "Nodo Relay R2"
    unique_id: "nodo_relay_r2"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id" and value_json.payload.currentx is defined %}
        {% set val = value_json.payload.currentx | int %}
        {% set r2 = val % 10 %}
        {{ {0: "OFF", 1: "ON", 2: "N/C"}[r2] | default("?") }}
      {% else %}
        {{ this.state }}
      {% endif %}
    unit_of_measurement: ""

  

  - name: "Nodo Lux"
    unique_id: "nodo_lux"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.lux is defined     %}
        {{ value_json.payload.lux | float | round(0) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "illuminance"
    unit_of_measurement: "lx"

  - name: "Nodo White Lux"
    unique_id: "nodo_white_lux"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/telemetry"
    value_template: >
      {% if value_json is defined and value_json.sender == "!!tuo_nodo_id"  and value_json.payload.white_lux is defined   %}
        {{ value_json.payload.white_lux | float | round(0) }}
      {% else %}
        {{ this.state }}
      {% endif %}
    device_class: "illuminance"
    unit_of_measurement: "lx"
    
  - name: "Nodo Risposta Comandi"
    unique_id: "nodo_risposta_comandi"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/risposta"
    # Poiché il valore è testo grezzo, lo prendiamo tutto così com'è
    value_template: "{{ value }}"
    icon: "mdi:message-text"
      

# SENSORE PARTE 1 (O COMANDI SINGOLI)
# SENSORE PARTE 1 (O COMANDI SINGOLI)
  - name: "R1:"
    unique_id: "nodo_risposta_comandi_parte1"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/risposta"
    value_template: >-
      {% if '1/' in value %}
        {{ value | replace('1/3 ', '') }}
      {% elif '2/' not in value and '3/' not in value %}
        {{ value }}
      {% else %}
        {{ this.state if this.state is defined and this.state not in ['unknown', 'unavailable'] else '' }}
      {% endif %}
    icon: "mdi:numeric-1-box"

  # SENSORE PARTE 2
  - name: "R2:"
    unique_id: "nodo_risposta_comandi_parte2"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/risposta"
    value_template: >-
      {% if '2/' in value %}
        {{ value | replace('2/3 ', '') }}
      {% elif '1/' in value or ('1/' not in value and '2/' not in value and '3/' not in value) %}
        {# Si cancella se inizia la sequenza (1/) o se arriva un comando singolo generico #}
        {{ '' }}
      {% else %}
        {{ this.state if this.state is defined and this.state not in ['unknown', 'unavailable'] else '' }}
      {% endif %}
    icon: "mdi:numeric-2-box"

  # SENSORE PARTE 3
  - name: "R3:"
    unique_id: "nodo_risposta_comandi_parte3"
    state_topic: "mesh/SA01/privato/!!tuo_nodo_id/risposta"
    value_template: >-
      {% if '3/' in value %}
        {{ value | replace('3/3 ', '') }}
      {% elif '1/' in value or ('1/' not in value and '2/' not in value and '3/' not in value) %}
        {# Si cancella se inizia la sequenza (1/) o se arriva un comando singolo generico #}
        {{ '' }}
      {% else %}
        {{ this.state if this.state is defined and this.state not in ['unknown', 'unavailable'] else '' }}
      {% endif %}
    icon: "mdi:numeric-3-box"

```

### 📊 Home Assistant Configuration 2 - Plancia
aggiungi questa configurazione nella plancia gomeassistant:

```yaml

type: entities
title: NODO meshtastic
show_header_toggle: true
entities:
  - type: section
    label: Nodo Risposte ai comandi
  - entity: sensor.nodo_risposta_comandi_parte_1
  - entity: sensor.nodo_risposta_comandi_parte_2
  - entity: sensor.nodo_risposta_comandi_parte_3
  - type: section
    label: Nodo Info
  - entity: sensor.nodo_long_name
  - entity: sensor.nodo_short_name
  - entity: sensor.nodo_uptime
  - entity: sensor.nodo_uptime_formattato
  - type: section
    label: Energia e Batteria
  - entity: sensor.nodo_battery_percent
  - entity: sensor.nodo_battery_voltage
  - type: section
    label: Nodo Info BOX
  - entity: sensor.nodo_temperatura_box_2
  - entity: sensor.nodo_umidita_box
  - entity: sensor.nodo_stato_nodo
  - entity: sensor.nodo_relay_fan
  - entity: sensor.nodo_relay_r1
  - entity: sensor.nodo_relay_r2
  - type: section
    label: Nodo Info Canale
  - entity: sensor.nodo_channel_utilization
  - entity: sensor.nodo_air_utilization_tx
  - type: section
    label: Ambiente (BME680 & Luce)
  - entity: sensor.nodo_temperatura_esterna
  - entity: sensor.nodo_umidita
  - entity: sensor.nodo_pressione
  - entity: sensor.nodo_gas_resistance
  - entity: sensor.nodo_lux
  - entity: sensor.nodo_white_lux
  - type: section
    label: Vento e Pioggia
  - entity: sensor.nodo_velocita_vento
  - entity: sensor.nodo_direzione_vento
  - entity: sensor.nodo_bonaccia_vento_2
  - entity: sensor.nodo_raffica_vento
  - entity: sensor.nodo_pioggia_1h
  - entity: sensor.nodo_pioggia_24h
state_color: true


```


