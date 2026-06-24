# meshtastic-firmwares-ITA
## Sensori MQTT per Homeassistant

### 📊 Home Assistant Configuration
Add the following configuration to your `mqtts/mqtt_sensors.yaml` file (or under the `mqtt: - sensor:` section in your `configuration.yaml`):

```yaml
# ------------------------------------------------------------------------------
# MESHTASTIC REMOTE COMMANDS - MULTI-SLOT RESPONSES
# ------------------------------------------------------------------------------
# Sensor Part 1 (Or Single Response Commands)
- name: "Nodo Risposta Comandi Parte 1"
  unique_id: "nodo_risposta_comandi_parte1"
  state_topic: "mesh/SA01/privato/!699c0a00/risposta"
  device_class: enum
  value_template: >-
    {% if '1/' in value %}
      {{ value | replace('1/3 ', '') }}
    {% elif '2/' not in value and '3/' not in value %}
      {{ value }}
    {% else %}
      {{ this.state if this.state is defined and this.state not in ['unknown', 'unavailable'] else '' }}
    {% endif %}
  icon: "mdi:text-start"

# Sensor Part 2
- name: "Nodo Risposta Comandi Parte 2"
  unique_id: "nodo_risposta_comandi_parte2"
  state_topic: "mesh/SA01/privato/!699c0a00/risposta"
  device_class: enum
  value_template: >-
    {% if '2/' in value %}
      {{ value | replace('2/3 ', '') }}
    {% elif '1/' in value or ('1/' not in value and '2/' not in value and '3/' not in value) %}
      {{ '' }}
    {% else %}
      {{ this.state if this.state is defined and this.state not in ['unknown', 'unavailable'] else '' }}
    {% endif %}
  icon: "mdi:text-start"

# Sensor Part 3
- name: "Nodo Risposta Comandi Parte 3"
  unique_id: "nodo_risposta_comandi_parte3"
  state_topic: "mesh/SA01/privato/!699c0a00/risposta"
  device_class: enum
  value_template: >-
    {% if '3/' in value %}
      {{ value | replace('3/3 ', '') }}
    {% elif '1/' in value or ('1/' not in value and '2/' not in value and '3/' not in value) %}
      {{ '' }}
    {% else %}
      {{ this.state if this.state is defined and this.state not in ['unknown', 'unavailable'] else '' }}
    {% endif %}
  icon: "mdi:text-start"
```
