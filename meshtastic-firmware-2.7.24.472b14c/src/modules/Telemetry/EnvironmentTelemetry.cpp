#include "configuration.h"

#if HAS_TELEMETRY && !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "Default.h"
#include "EnvironmentTelemetry.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "RTC.h"
#include "Router.h"
#include "TransmitHistory.h"
#include "UnitConversions.h"
#include "buzz.h"
#include "graphics/SharedUIDisplay.h"
#include "graphics/images.h"
#include "main.h"
#include "modules/ExternalNotificationModule.h"
#include "power.h"
#include "sleep.h"
#include "target_specific.h"
#include <OLEDDisplay.h>

 



///////////////////////////////////////////////
#include "detect/ScanI2C.h"

#include <Wire.h>

///////////////////////////////////////////////

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL

// Sensors
#include "Sensor/CGRadSensSensor.h"
#include "Sensor/RCWL9620Sensor.h"
#include "Sensor/nullSensor.h"

namespace graphics
{
extern void drawCommonHeader(OLEDDisplay *display, int16_t x, int16_t y, const char *titleStr, bool force_no_invert,
                             bool show_date);
}
#if __has_include(<Adafruit_AHTX0.h>)
#include "Sensor/AHT10.h"
#endif

#if __has_include(<Adafruit_BME280.h>)
#include "Sensor/BME280Sensor.h"
#endif

#if __has_include(<Adafruit_BMP085.h>)
#include "Sensor/BMP085Sensor.h"
#endif

#if __has_include(<Adafruit_BMP280.h>)
#include "Sensor/BMP280Sensor.h"
#endif

#if __has_include(<Adafruit_LTR390.h>)
#include "Sensor/LTR390UVSensor.h"
#endif

#if __has_include(<bsec2.h>) || __has_include(<Adafruit_BME680.h>)
#include "Sensor/BME680Sensor.h"
#endif

#if __has_include(<Adafruit_DPS310.h>)
#include "Sensor/DPS310Sensor.h"
#endif

#if __has_include(<Adafruit_MCP9808.h>)
#include "Sensor/MCP9808Sensor.h"
#endif

#if __has_include(<Adafruit_SHT31.h>)
#include "Sensor/SHT31Sensor.h"
#endif

#if __has_include(<Adafruit_LPS2X.h>)
#include "Sensor/LPS22HBSensor.h"
#endif

#if __has_include(<Adafruit_SHTC3.h>)
#include "Sensor/SHTC3Sensor.h"
#endif

#if __has_include("RAK12035_SoilMoisture.h") && defined(RAK_4631) && RAK_4631 == 1
#include "Sensor/RAK12035Sensor.h"
#endif

#if __has_include(<Adafruit_VEML7700.h>)
#include "Sensor/VEML7700Sensor.h"
#endif

#if __has_include(<Adafruit_TSL2591.h>)
#include "Sensor/TSL2591Sensor.h"
#endif

#if __has_include(<ClosedCube_OPT3001.h>)
#include "Sensor/OPT3001Sensor.h"
#endif

#if __has_include(<Adafruit_SHT4x.h>)
#include "Sensor/SHT4XSensor.h"
#endif

#if __has_include(<SparkFun_MLX90632_Arduino_Library.h>)
#include "Sensor/MLX90632Sensor.h"
#endif

#if __has_include(<DFRobot_LarkWeatherStation.h>)
#include "Sensor/DFRobotLarkSensor.h"
#endif

#if __has_include(<DFRobot_RainfallSensor.h>)
#include "Sensor/DFRobotGravitySensor.h"
#endif

#if __has_include(<SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>)
#include "Sensor/NAU7802Sensor.h"
#endif

#if __has_include(<Adafruit_BMP3XX.h>)
#include "Sensor/BMP3XXSensor.h"
#endif

#if __has_include(<Adafruit_PCT2075.h>)
#include "Sensor/PCT2075Sensor.h"
#endif

#endif
#ifdef T1000X_SENSOR_EN
#include "Sensor/T1000xSensor.h"
#endif

#ifdef SENSECAP_INDICATOR
#include "Sensor/IndicatorSensor.h"
#endif

#if __has_include(<Adafruit_TSL2561_U.h>)
#include "Sensor/TSL2561Sensor.h"
#endif

#if __has_include(<BH1750_WE.h>)
#include "Sensor/BH1750Sensor.h"
#endif

#define FAILED_STATE_SENSOR_READ_MULTIPLIER 10
#define DISPLAY_RECEIVEID_MEASUREMENTS_ON_SCREEN true

#include "Sensor/AddI2CSensorTemplate.h"
#include "graphics/ScreenFonts.h"
#include <Throttle.h>

static constexpr uint16_t TX_HISTORY_KEY_ENVIRONMENT_TELEMETRY = 0x8002;

///////////////////////////////////////////////
EnvironmentTelemetryModule *EnvironmentTelemetryModule::instance = nullptr;

extern float fanTemp; // "Cerca questa variabile fuori da questo file"
extern float fanHum;

extern boolean onsleep;

static bool isTelemetryBusy = false;

#ifdef RAIN_SENSOR_PIN
// ATTENZIONE: Qui le variabili sono già dichiarate extern altrove, 
    // quindi le usiamo direttamente senza il tipo (float) davanti.
    extern float pioggia_ultima_ora;
    extern float pioggia_totale_24h;
#endif
///////////////////////////////////////////////

void EnvironmentTelemetryModule::i2cScanFinished(ScanI2C *i2cScanner)
{
    if (!moduleConfig.telemetry.environment_measurement_enabled && !ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE
///////////////////////////////////////////////
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)
        && false // Forza l'esecuzione della scansione se la ventola è definita
#endif
    ) {
///////////////////////////////////////////////

        return;
    }
    
    LOG_INFO("Environment Telemetry adding I2C devices...");

    // order by priority of metrics/values (low top, high bottom)

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR
#ifdef T1000X_SENSOR_EN
    // Not a real I2C device
    addSensor<T1000xSensor>(i2cScanner, ScanI2C::DeviceType::NONE);
#else
#ifdef SENSECAP_INDICATOR
    // Not a real I2C device, uses UART
    addSensor<IndicatorSensor>(i2cScanner, ScanI2C::DeviceType::NONE);
#endif
    addSensor<RCWL9620Sensor>(i2cScanner, ScanI2C::DeviceType::RCWL9620);
    addSensor<CGRadSensSensor>(i2cScanner, ScanI2C::DeviceType::CGRADSENS);
#endif
#endif

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL
#if __has_include(<DFRobot_LarkWeatherStation.h>)
    addSensor<DFRobotLarkSensor>(i2cScanner, ScanI2C::DeviceType::DFROBOT_LARK);
#endif
#if __has_include(<DFRobot_RainfallSensor.h>)
    addSensor<DFRobotGravitySensor>(i2cScanner, ScanI2C::DeviceType::DFROBOT_RAIN);
#endif
#if __has_include(<Adafruit_AHTX0.h>)
    addSensor<AHT10Sensor>(i2cScanner, ScanI2C::DeviceType::AHT10);
#endif
#if __has_include(<Adafruit_BMP085.h>)
    addSensor<BMP085Sensor>(i2cScanner, ScanI2C::DeviceType::BMP_085);
#endif
#if __has_include(<Adafruit_BME280.h>)
    addSensor<BME280Sensor>(i2cScanner, ScanI2C::DeviceType::BME_280);
#endif
#if __has_include(<Adafruit_LTR390.h>)
    addSensor<LTR390UVSensor>(i2cScanner, ScanI2C::DeviceType::LTR390UV);
#endif
#if __has_include(<bsec2.h>) || __has_include(<Adafruit_BME680.h>)
    addSensor<BME680Sensor>(i2cScanner, ScanI2C::DeviceType::BME_680);
#endif
#if __has_include(<Adafruit_BMP280.h>)
    addSensor<BMP280Sensor>(i2cScanner, ScanI2C::DeviceType::BMP_280);
#endif
#if __has_include(<Adafruit_DPS310.h>)
    addSensor<DPS310Sensor>(i2cScanner, ScanI2C::DeviceType::DPS310);
#endif
#if __has_include(<Adafruit_MCP9808.h>)
    addSensor<MCP9808Sensor>(i2cScanner, ScanI2C::DeviceType::MCP9808);
#endif
#if __has_include(<Adafruit_SHT31.h>)
    addSensor<SHT31Sensor>(i2cScanner, ScanI2C::DeviceType::SHT31);
#endif
#if __has_include(<Adafruit_LPS2X.h>)
    addSensor<LPS22HBSensor>(i2cScanner, ScanI2C::DeviceType::LPS22HB);
#endif
#if __has_include(<Adafruit_SHTC3.h>)
    addSensor<SHTC3Sensor>(i2cScanner, ScanI2C::DeviceType::SHTC3);
#endif
#if __has_include("RAK12035_SoilMoisture.h") && defined(RAK_4631) && RAK_4631 == 1
    addSensor<RAK12035Sensor>(i2cScanner, ScanI2C::DeviceType::RAK12035);
#endif
#if __has_include(<Adafruit_VEML7700.h>)
    addSensor<VEML7700Sensor>(i2cScanner, ScanI2C::DeviceType::VEML7700);
#endif
#if __has_include(<Adafruit_TSL2591.h>)
    addSensor<TSL2591Sensor>(i2cScanner, ScanI2C::DeviceType::TSL2591);
#endif
#if __has_include(<ClosedCube_OPT3001.h>)
    addSensor<OPT3001Sensor>(i2cScanner, ScanI2C::DeviceType::OPT3001);
#endif
#if __has_include(<Adafruit_SHT4x.h>)
    addSensor<SHT4XSensor>(i2cScanner, ScanI2C::DeviceType::SHT4X);
#endif
#if __has_include(<SparkFun_MLX90632_Arduino_Library.h>)
    addSensor<MLX90632Sensor>(i2cScanner, ScanI2C::DeviceType::MLX90632);
#endif

#if __has_include(<Adafruit_BMP3XX.h>)
    addSensor<BMP3XXSensor>(i2cScanner, ScanI2C::DeviceType::BMP_3XX);
#endif
#if __has_include(<Adafruit_PCT2075.h>)
    addSensor<PCT2075Sensor>(i2cScanner, ScanI2C::DeviceType::PCT2075);
#endif
#if __has_include(<Adafruit_TSL2561_U.h>)
    addSensor<TSL2561Sensor>(i2cScanner, ScanI2C::DeviceType::TSL2561);
#endif
#if __has_include(<SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>)
    addSensor<NAU7802Sensor>(i2cScanner, ScanI2C::DeviceType::NAU7802);
#endif
#if __has_include(<BH1750_WE.h>)
    addSensor<BH1750Sensor>(i2cScanner, ScanI2C::DeviceType::BH1750);
#endif

#endif
}

int32_t EnvironmentTelemetryModule::runOnce()
{
    // 1. Gestione Deep Sleep (Codice originale)
    if (sleepOnNextExecution == true) {
        sleepOnNextExecution = false;
        uint32_t nightyNightMs = Default::getConfiguredOrDefaultMs(moduleConfig.telemetry.environment_update_interval,
                                                                   default_telemetry_broadcast_interval_secs);
        LOG_DEBUG("Sleep for %ims, then awake to send metrics again", nightyNightMs);
        doDeepSleep(nightyNightMs, true, false);
    }

    uint32_t result = UINT32_MAX;
    /*
        Uncomment the preferences below if you want to use the module
        without having to configure it from the PythonAPI or WebUI.
    */
  // 2. CONTROLLO SOPRAVVIVENZA: Impedisce al modulo di chiudersi
    // moduleConfig.telemetry.environment_measurement_enabled = 1;
    // moduleConfig.telemetry.environment_screen_enabled = 1;
    // moduleConfig.telemetry.environment_update_interval = 15;

    if (!(moduleConfig.telemetry.environment_measurement_enabled || moduleConfig.telemetry.environment_screen_enabled ||
          ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE
///////////////////////////////////////////////
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)
          || true // Forza il modulo a restare acceso per la ventola
#endif
        )) {
///////////////////////////////////////////////

        return disable();
    }

    // 3. PRIMA ESECUZIONE (Inizializzazione sensori)
    if (firstTime) {
        firstTime = 0;

        // Entra nell'init se abilitato o se serve alla ventola
        if (moduleConfig.telemetry.environment_measurement_enabled || ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE 
///////////////////////////////////////////////
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)
            || true 
#endif
        ) {
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)
            LOG_INFO("BOX FAN: Telemetria forzata (Istanza attiva)");
#else
///////////////////////////////////////////////

            LOG_INFO("Environment Telemetry: init");
			
///////////////////////////////////////////////
#endif
///////////////////////////////////////////////

            // check if we have at least one sensor
            if (!sensors.empty()) {
                result = DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
            }

#ifdef T1000X_SENSOR_EN
            // (Codice specifico T1000 se presente)
#elif !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL
            if (ina219Sensor.hasSensor())
                result = ina219Sensor.runOnce();
            if (ina260Sensor.hasSensor())
                result = ina260Sensor.runOnce();
            if (ina3221Sensor.hasSensor())
                result = ina3221Sensor.runOnce();
            if (max17048Sensor.hasSensor())
                result = max17048Sensor.runOnce();
                // this only works on the wismesh hub with the solar option. This is not an I2C sensor, so we don't need the
                // sensormap here.
#ifdef HAS_RAKPROT
            if (rak9154Sensor.hasSensor())
                result = rak9154Sensor.runOnce();
#endif
#endif
        }

///////////////////////////////////////////////
///////////////////////////////////////////////
    // 1. Controllo Avvio: Se abbiamo QUALSIASI sensore della box, 
    // non permettiamo MAI il disable() qui.
    ///////////////////////////////////////////////
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)
    
    return setStartDelay();

#else

    return result == UINT32_MAX ? disable() : setStartDelay();

#endif
    ///////////////////////////////////////////////

    } else {
        // 4. CICLO CONTINUO
        // Se la telemetria è disattivata nelle impostazioni ufficiali...
        if (!moduleConfig.telemetry.environment_measurement_enabled && !ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE) {

    ///////////////////////////////////////////////
    // ...ma abbiamo uno dei nostri sensori custom definiti, NON spegnere.
    // Il disable() scatta SOLO SE non abbiamo nessuno di questi sensori.
    ///////////////////////////////////////////////
#if !defined(I2C_FAN_SENSOR_ADDR) && !defined(ONEWIRE_TEMP_PIN) && !defined(DHT_TEMP_PIN) && !defined(ANALOG_TEMP_PIN)
            
            return disable(); 

#else
            // Restiamo vivi per gestire la ventola e l'iniezione dati Ninja
            return setStartDelay();

#endif
    ///////////////////////////////////////////////
        }

        // Lettura sensori
        for (TelemetrySensor *sensor : sensors) {
            uint32_t delay = sensor->runOnce();
            if (delay < result) {
                result = delay;
            }
        }

 
        // 5. INVIO RADIO (Mesh remota - protetta dal filtro leggisolouno)
        uint32_t lastTelemetry =
            transmitHistory ? transmitHistory->getLastSentToMeshMillis(TX_HISTORY_KEY_ENVIRONMENT_TELEMETRY) : 0;
            
        // Correzione: usiamo moduleConfig invece di config
 ///////////////////////////////////////////////
        if (!leggisolouno && moduleConfig.telemetry.environment_measurement_enabled && 
 ///////////////////////////////////////////////
 
            ((lastTelemetry == 0) ||
             !Throttle::isWithinTimespanMs(lastTelemetry, Default::getConfiguredOrDefaultMsScaled(
                                                              moduleConfig.telemetry.environment_update_interval,
                                                              default_telemetry_broadcast_interval_secs, numOnlineNodes))) &&
            airTime->isTxAllowedChannelUtil(config.device.role != meshtastic_Config_DeviceConfig_Role_SENSOR) &&
            airTime->isTxAllowedAirUtil()) {
            
            sendTelemetry(); 
            if (transmitHistory)
                transmitHistory->setLastSentToMesh(TX_HISTORY_KEY_ENVIRONMENT_TELEMETRY);

///////////////////////////////////////////////
        // 6. INVIO LOCALE (Bluetooth al tuo telefono)
        } else if (moduleConfig.telemetry.environment_measurement_enabled && 
                   ((lastSentToPhone == 0) || !Throttle::isWithinTimespanMs(lastSentToPhone, sendToPhoneIntervalMs)) &&
                   (service->isToPhoneQueueEmpty())) {
///////////////////////////////////////////////
            // Only send while queue is empty (phone assumed connected)
            sendTelemetry(NODENUM_BROADCAST, true);
            lastSentToPhone = millis();
        }
    }
    return min(sendToPhoneIntervalMs, result);
}

bool EnvironmentTelemetryModule::wantUIFrame()
{
    return moduleConfig.telemetry.environment_screen_enabled;
}

#if HAS_SCREEN
void EnvironmentTelemetryModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // === Setup display ===
    display->clear();
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    int line = 1;

    // === Set Title
    const char *titleStr = (graphics::currentResolution == graphics::ScreenResolution::High) ? "Environment" : "Env.";

    // === Header ===
    graphics::drawCommonHeader(display, x, y, titleStr);

    // === Row spacing setup ===
    const int rowHeight = FONT_HEIGHT_SMALL - 4;
    int currentY = graphics::getTextPositions(display)[line++];

    // === Show "No Telemetry" if no data available ===
    if (!lastMeasurementPacket) {
        display->drawString(x, currentY, "No Telemetry");
        return;
    }

    // Decode the telemetry message from the latest received packet
    const meshtastic_Data &p = lastMeasurementPacket->decoded;
    meshtastic_Telemetry telemetry;
    if (!pb_decode_from_bytes(p.payload.bytes, p.payload.size, &meshtastic_Telemetry_msg, &telemetry)) {
        display->drawString(x, currentY, "No Telemetry");
        return;
    }

    const auto &m = telemetry.variant.environment_metrics;

    // Check if any telemetry field has valid data
    bool hasAny = m.has_temperature || m.has_relative_humidity || m.barometric_pressure != 0 || m.iaq != 0 || m.voltage != 0 ||
                  m.current != 0 || m.lux != 0 || m.white_lux != 0 || m.weight != 0 || m.distance != 0 || m.radiation != 0;

    if (!hasAny) {
        display->drawString(x, currentY, "No Telemetry");
        return;
    }

    // === First line: Show sender name + time since received (left), and first metric (right) ===
    const char *sender = getSenderShortName(*lastMeasurementPacket);
    uint32_t agoSecs = service->GetTimeSinceMeshPacket(lastMeasurementPacket);
    String agoStr = (agoSecs > 864000) ? "?"
                    : (agoSecs > 3600) ? String(agoSecs / 3600) + "h"
                    : (agoSecs > 60)   ? String(agoSecs / 60) + "m"
                                       : String(agoSecs) + "s";

    String leftStr = String(sender) + " (" + agoStr + ")";
    display->drawString(x, currentY, leftStr); // Left side: who and when

    // === Collect sensor readings as label strings (no icons) ===
    std::vector<String> entries;

    if (m.has_temperature) {
        String tempStr = moduleConfig.telemetry.environment_display_fahrenheit
                             ? "Tmp: " + String(UnitConversions::CelsiusToFahrenheit(m.temperature), 1) + "°F"
                             : "Tmp: " + String(m.temperature, 1) + "°C";
        entries.push_back(tempStr);
    }
    if (m.has_relative_humidity)
        entries.push_back("Hum: " + String(m.relative_humidity, 0) + "%");
    if (m.barometric_pressure != 0)
        entries.push_back("Prss: " + String(m.barometric_pressure, 0) + " hPa");
    if (m.iaq != 0) {
        String aqi = "IAQ: " + String(m.iaq);
        const char *bannerMsg = nullptr; // Default: no banner

        if (m.iaq <= 25)
            aqi += " (Excellent)";
        else if (m.iaq <= 50)
            aqi += " (Good)";
        else if (m.iaq <= 100)
            aqi += " (Moderate)";
        else if (m.iaq <= 150)
            aqi += " (Poor)";
        else if (m.iaq <= 200) {
            aqi += " (Unhealthy)";
            bannerMsg = "Unhealthy IAQ";
        } else if (m.iaq <= 300) {
            aqi += " (Very Unhealthy)";
            bannerMsg = "Very Unhealthy IAQ";
        } else {
            aqi += " (Hazardous)";
            bannerMsg = "Hazardous IAQ";
        }

        entries.push_back(aqi);

        // === IAQ alert logic ===
        static uint32_t lastAlertTime = 0;
        uint32_t now = millis();

        bool isOwnTelemetry = lastMeasurementPacket->from == nodeDB->getNodeNum();
        bool isCooldownOver = (now - lastAlertTime > 60000);

        if (isOwnTelemetry && bannerMsg && isCooldownOver) {
            LOG_INFO("drawFrame: IAQ %d (own) — showing banner: %s", m.iaq, bannerMsg);
            screen->showSimpleBanner(bannerMsg, 3000);

            // Only buzz if IAQ is over 200
            if (m.iaq > 200 && moduleConfig.external_notification.enabled && !externalNotificationModule->getMute()) {
                playLongBeep();
            }

            lastAlertTime = now;
        }
    }
    if (m.voltage != 0 || m.current != 0)
        entries.push_back(String(m.voltage, 1) + "V / " + String(m.current, 0) + "mA");
    if (m.lux != 0)
        entries.push_back("Light: " + String(m.lux, 0) + "lx");
    if (m.white_lux != 0)
        entries.push_back("White: " + String(m.white_lux, 0) + "lx");
    if (m.weight != 0)
        entries.push_back("Weight: " + String(m.weight, 0) + "kg");
    if (m.distance != 0)
        entries.push_back("Level: " + String(m.distance, 0) + "mm");
    if (m.radiation != 0)
        entries.push_back("Rad: " + String(m.radiation, 2) + " µR/h");

    // === Show first available metric on top-right of first line ===
    if (!entries.empty()) {
        String valueStr = entries.front();
        int rightX = SCREEN_WIDTH - display->getStringWidth(valueStr);
        display->drawString(rightX, currentY, valueStr);
        entries.erase(entries.begin()); // Remove from queue
    }

    // === Advance to next line for remaining telemetry entries ===
    currentY += rowHeight;

    // === Draw remaining entries in 2-column format (left and right) ===
    for (size_t i = 0; i < entries.size(); i += 2) {
        // Left column
        display->drawString(x, currentY, entries[i]);

        // Right column if it exists
        if (i + 1 < entries.size()) {
            int rightX = SCREEN_WIDTH / 2;
            display->drawString(rightX, currentY, entries[i + 1]);
        }

        currentY += rowHeight;
    }
    graphics::drawCommonFooter(display, x, y);
}
#endif

bool EnvironmentTelemetryModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_Telemetry *t)
{
    if (t->which_variant == meshtastic_Telemetry_environment_metrics_tag) {
#if defined(DEBUG_PORT) && !defined(DEBUG_MUTE)
        const char *sender = getSenderShortName(mp);

        LOG_INFO("(Received from %s): barometric_pressure=%f, current=%f, gas_resistance=%f, relative_humidity=%f, "
                 "temperature=%f",
                 sender, t->variant.environment_metrics.barometric_pressure, t->variant.environment_metrics.current,
                 t->variant.environment_metrics.gas_resistance, t->variant.environment_metrics.relative_humidity,
                 t->variant.environment_metrics.temperature);
        LOG_INFO("(Received from %s): voltage=%f, IAQ=%d, distance=%f, lux=%f, white_lux=%f", sender,
                 t->variant.environment_metrics.voltage, t->variant.environment_metrics.iaq,
                 t->variant.environment_metrics.distance, t->variant.environment_metrics.lux,
                 t->variant.environment_metrics.white_lux);

        LOG_INFO("(Received from %s): wind speed=%fm/s, direction=%d degrees, weight=%fkg", sender,
                 t->variant.environment_metrics.wind_speed, t->variant.environment_metrics.wind_direction,
                 t->variant.environment_metrics.weight);

        LOG_INFO("(Received from %s): radiation=%fµR/h", sender, t->variant.environment_metrics.radiation);

#endif
        // release previous packet before occupying a new spot
        if (lastMeasurementPacket != nullptr)
            packetPool.release(lastMeasurementPacket);

        lastMeasurementPacket = packetPool.allocCopy(mp);
    }

    return false; // Let others look at this message also if they want
}

 

///////////////////////////////////////////////
void EnvironmentTelemetryModule::aggiornaTemperaturaBox() {
    // 1. Configurazione FLAG per la lettura
    if (onsleep) {
        // Se stiamo per morire, forziamo la lettura di TUTTI i sensori (SHT20, BMP280, OneWire)
        // per avere il codice 8xxx (prefisso + stato relay) reale.
        leggisolouno = false; 
        LOG_INFO("BATTERY: Preparazione 'Ultimo Respiro'. Scansione totale sensori...");
    } else {
        // Lettura rapida solo per controllo ventola (solo BME/BMP)
        leggisolouno = true; 
    }

    // 2. ESECUZIONE LETTURA: Popola fanTemp e prepara l'oggetto Telemetry
    meshtastic_Telemetry m = meshtastic_Telemetry_init_zero;
    getEnvironmentTelemetry(&m); 

    // 3. INVIO DOPPIO: Spara i dati sia sulla Mesh che al Telefono
    if (onsleep) {
        // Invia alla rete LoRa (per gli altri nodi e MQTT)
        sendTelemetry(); 

        // Invia al Bluetooth (per l'App sul tuo telefono se sei vicino)
        // Usiamo NODENUM_BROADCAST e true per forzare il pacchetto verso il BLE
        sendTelemetry(NODENUM_BROADCAST, true); 

        LOG_INFO("BATTERY: Telemetria finale inviata (LoRa + BLE). Addio.");
    }

    // 4. RIPRISTINO: Riporta il flag a false per il prossimo risveglio tra 12 ore
    leggisolouno = false;
}
///////////////////////////////////////////////




 

bool EnvironmentTelemetryModule::getEnvironmentTelemetry(meshtastic_Telemetry *m)
{

    // 2. The Gatekeeper: if busy, exit immediately to prevent I2C collision
    //////// if (isTelemetryBusy) {
    ////////     LOG_WARN("TELEMETRY: Resource busy, skipping to avoid WDT reset");
    ////////     return false; 
    //////// }

///////////////////////////////////////////////
    // 3. Lock the resource
    isTelemetryBusy = true;

    LOG_DEBUG("TELEMETRY: Avvio getEnvironmentTelemetry");
///////////////////////////////////////////////
    bool valid = false;
    bool hasSensor = false;
    // getMetrics() doesn't always get evaluated because of
    // short-circuit evaluation rules in c++
    bool get_metrics;
    bool has_sensors=false;

    m->time = getTime();
    m->which_variant = meshtastic_Telemetry_environment_metrics_tag;
    m->variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;

     
     

 
// --- AGGIUNTO IL CICLO FOR MANCANTE ---
for (TelemetrySensor *sensor : sensors) {
has_sensors=true;


///////////////////////////////////////////////
// Chiamata singola: recupera l'indirizzo già corretto dal .h

uint8_t currentAddr = sensor->getAddr();

 

if (leggisolouno) {
    #ifdef I2C_FAN_SENSOR_ADDR
            LOG_DEBUG("FAN CHECK: Controllo sensore all'indirizzo 0x%02x", currentAddr);

            if (currentAddr == I2C_FAN_SENSOR_ADDR) {
                if (sensor->getMetrics(m)) {
                    fanTemp = m->variant.environment_metrics.temperature;
                    fanHum =  m->variant.environment_metrics.relative_humidity;
                    LOG_INFO("FAN CHECK: Lettura riuscita su 0x%02x! Temp: %.1f", currentAddr, fanTemp);
                    isTelemetryBusy = false;
                    return true; // Missione compiuta, usciamo
                } else {
                    LOG_ERROR("FAN CHECK: Trovato 0x%02x ma la lettura ha fallito!", currentAddr);
                    // Fallimento lettura: impostiamo errore e usciamo comunque
                    fanTemp = -150.0f;
                    fanHum = 0.0f;
                    isTelemetryBusy = false;
                    return false;
                }
            }
            // Se non è l'indirizzo giusto, continua il ciclo per cercarlo
            continue; 
    #else
            // SE NON È DEFINITO: Non possiamo sapere qual è il sensore giusto.
            // Impostiamo il valore di errore, sblocchiamo e chiudiamo la funzione.
            LOG_WARN("FAN CHECK: Indirizzo sensore non definito! Impossibile leggere su i2c.");
            fanTemp = -150.0f;
            fanHum=0.0f;
            isTelemetryBusy = false;
            return false; 
    #endif
}


        
        // ... qui segue il resto della funzione normale ...




        // --- GESTIONE SENSORE BOX (BME280) ---
#ifdef I2C_FAN_SENSOR_ADDR
        if (currentAddr == I2C_FAN_SENSOR_ADDR) {
            LOG_DEBUG("TELEMETRY: Salto sensore Box all'indirizzo 0x%02x (verra' iniettato dopo)", currentAddr);
            continue; 
        }
#endif
///////////////////////////////////////////////
        // --- GESTIONE SENSORE AMBIENTE (GY-21 / SHT) ---
        LOG_DEBUG("TELEMETRY: Lettura sensore ambiente all'indirizzo 0x%02x", currentAddr);
///////////////////////////////////////////////

///////////////////////////////////////////////
        get_metrics = sensor->getMetrics(m); 
        if (get_metrics) {
            LOG_INFO("TELEMETRY: Dati letti correttamente da 0x%02x (T: %.1f H: %.1f)", 
                      currentAddr, m->variant.environment_metrics.temperature, m->variant.environment_metrics.relative_humidity);
            valid = true;
            hasSensor = true;
        } else {
            LOG_WARN("TELEMETRY: Lettura fallita per sensore 0x%02x", currentAddr);
        }
///////////////////////////////////////////////

    }

    // --- SENSORI POWER (INA219, ecc.) ---
#ifndef T1000X_SENSOR_EN
    if (ina219Sensor.hasSensor()) {
        get_metrics = ina219Sensor.getMetrics(m);

///////////////////////////////////////////////
		LOG_DEBUG("TELEMETRY: Lettura INA219: %s", get_metrics ? "OK" : "FAIL");
///////////////////////////////////////////////

        valid = valid || get_metrics;
        hasSensor = true;
    }
    if (ina260Sensor.hasSensor()) {
        get_metrics = ina260Sensor.getMetrics(m);
        valid = valid || get_metrics;
        hasSensor = true;
    }
    if (ina3221Sensor.hasSensor()) {
        get_metrics = ina3221Sensor.getMetrics(m);
        valid = valid || get_metrics;
        hasSensor = true;
    }
    if (max17048Sensor.hasSensor()) {
        get_metrics = max17048Sensor.getMetrics(m);
        valid = valid || get_metrics;
        hasSensor = true;
    }
#endif
#ifdef HAS_RAKPROT
    if (rak9154Sensor.hasSensor()) {
        get_metrics = rak9154Sensor.getMetrics(m);
        valid = valid || get_metrics;
        hasSensor = true;
    }
#endif

///////////////////////////////////////////////
// ... (restano i check per INA219, INA260, ecc. come nel tuo codice) ...
    // --- INIEZIONE FINALE DATI BOX ---
// La condizione ora include tutti i possibili sensori di temperatura
#if defined(I2C_FAN_SENSOR_ADDR) || defined(ONEWIRE_TEMP_PIN) || defined(DHT_TEMP_PIN) || defined(ANALOG_TEMP_PIN)
    LOG_DEBUG("TELEMETRY: Controllo iniezione fanTemp (Attuale: %.1f C)", fanTemp);
///////////////// COMMENTARE DA QUI A....
    if (fanTemp > -50.0f) {
        float finalVal;
        float tempIntera = (float)((int)fanTemp); // Forza 23.8 -> 23.0

#if HAS_HUMIDITY
        // Mescola Umidità nei decimali (es. 23.65)
        finalVal = tempIntera + (std::min(std::max(fanHum, 0.0f), 99.0f) / 100.0f);
#else
        // Solo temperatura intera (es. 23.00)
        fanHum=0.0f;
        finalVal = tempIntera;
#endif

        // Se NON vogliamo vedere i dati nelle Power Metrics, procediamo con l'iniezione Ambientale
#ifndef SHOW_ON_POWER_METRICS

    // Ora iniettiamo la temperatura della box nel campo VOLTAGE di 'm'
    if (!has_sensors) {
        // 1. DICHIARA IL TIPO DI PACCHETTO (Fondamentale per lo storico/grafici)
        m->which_variant = meshtastic_Telemetry_environment_metrics_tag; 

        // 2. INIZIALIZZA (per pulire eventuali residui di altri sensori)
        m->variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;

        // Iniettiamo il valore nel campo TEMPERATURA (Così si attiva lo storico!)
        m->variant.environment_metrics.has_temperature = true;
        m->variant.environment_metrics.temperature = fanTemp;
        
        m->variant.environment_metrics.has_relative_humidity = false;
        m->variant.environment_metrics.relative_humidity = 0.0f;
        
        m->variant.environment_metrics.has_voltage = true;
        m->variant.environment_metrics.voltage = finalVal;
        
        valid = true; // Per essere sicuri che invii se non ci sono sensori

    } else {
        // Se ci sono sensori, scrivi solo nel voltaggio come volevi tu
        m->variant.environment_metrics.has_voltage = true;
        m->variant.environment_metrics.voltage = finalVal;
    }

#else
    // Se ci sono sensori, scrivi solo nel voltaggio come volevi tu
    m->variant.environment_metrics.has_voltage = false;
    m->variant.environment_metrics.voltage = 0.0f;
    LOG_DEBUG("powermetrics Mode: Dati deviati su Power Metrics, salto iniezione ambientale.");
#endif

       
/////////////////// qui per ELIMINARE IL FAN TEMP DA  info FAN TEMP da  metriche normali!!!

        valid = true;
        hasSensor = true;

        LOG_INFO("TELEMETRY: Iniezione finale riuscita! Voltage=%.1f (Temp Box), Current=%.0f", 
                  m->variant.environment_metrics.voltage, m->variant.environment_metrics.current);
    } else {
        LOG_WARN("TELEMETRY: fanTemp non valida (%.1f), iniezione saltata", fanTemp);
    }
 
///////////////////////////////////////////////

// --- NUOVA LOGICA STATUS PANEL (FAN & RELAYS) ---
// Formato: 5XYZ (5=OK, 9=Errore Sensore)
// X=Ventola, Y=Relay 1, Z=Relay 2 
// Legenda: 1=ON, 0=OFF, 2=Non Configurato (Assente)

///////////////////////////////////////////////
#endif



int relayMap = 5000; 


// 1. Cifra delle CENTINAIA: VENTOLA (X)
#ifdef FAN_RELAY_PIN
    relayMap += (digitalRead(FAN_RELAY_PIN) == HIGH) ? 100 : 0;
#else
    relayMap += 200; 
#endif

// 2. Cifra delle DECINE: RELAY 1 (Y)
#ifdef RELAY_1_PIN
    relayMap += (digitalRead(RELAY_1_PIN) == HIGH) ? 10 : 0;
#else
    relayMap += 20;
#endif

// 3. Cifra delle UNITA': RELAY 2 (Z)
#ifdef RELAY_2_PIN
    relayMap += (digitalRead(RELAY_2_PIN) == HIGH) ? 1 : 0;
#else
    relayMap += 2;
#endif

// --- GESTIONE ERRORE SENSORE ---
// Se fanTemp (variabile globale aggiornata dai sensori) è fuori range, il 5 diventa 9
if (fanTemp <= -50.0f || fanTemp >= 150.0f) {
    relayMap += 4000; 
}

if (onsleep) {
    if (relayMap >= 9000) {
        // Eravamo in errore (9), sottraiamo 1000 per portarlo a 8 (Sleep)
        relayMap -= 1000; 
    } else if (relayMap < 6000) {
        // Eravamo regolari (5), sommiamo 3000 per portarlo a 8 (Sleep)
        relayMap += 3000;
    }
    // NOTA: Se per qualche motivo relayMap fosse già 8xxx (es. doppio check), 
    // queste condizioni lo lasciano invariato.
}

 

#if defined(FAN_RELAY_PIN) || defined(RELAY_1_PIN) || defined(RELAY_2_PIN)

// --- INIEZIONE NELLE METRICHE ---
// Se SHOW_ON_POWER_METRICS è definita, saltiamo questa iniezione per non duplicare i dati
#ifndef SHOW_ON_POWER_METRICS
    m->variant.environment_metrics.has_current = true;
    m->variant.environment_metrics.current = (float)relayMap;

    LOG_INFO("TELEMETRY: Relay Status Map: %d (Inviato come %.1f)", relayMap, m->variant.environment_metrics.current);
#else
    // Se invece è definito, ci assicuriamo che il campo corrente sia spento o ignorato qui
    m->variant.environment_metrics.has_current = false;
    m->variant.environment_metrics.current = 0.0f;
    LOG_DEBUG("TELEMETRY: Relay Map ignorata nelle Env Metrics (attesa su Power Metrics)");
#endif

    LOG_DEBUG("TELEMETRY: Fine. Valid=%s, HasSensor=%s", valid ? "YES" : "NO", hasSensor ? "YES" : "NO");

#endif




// =========================================================================
// BLOCCO METEO UNIFICATO ED ELASTICO (DIREZIONE & VELOCITÀ) - AGGIORNATO INTERRUPT
// =========================================================================
#if defined(HAS_WIND_DIRECTION) || defined(WIND_VELOCITY_PIN) // Usiamo WIND_VELOCITY_PIN come controllo
if (!leggisolouno) 
{
    float calcolodir = -1.0f;
    float calcolovel = -1.0f;
    bool ha_dir = false;
    bool ha_vel = false;

    // 1. Chiamata al metodo della Banderuola (AS5600)
#ifdef HAS_WIND_DIRECTION
    calcolodir = getWindDirectionDegrees();
    if (calcolodir >= 0.0f && calcolodir <= 360.0f) {
        ha_dir = true;
    }
#endif

    // 2. NUOVA LETTURA DA VARIABILE GLOBALE (Aggiornata ogni 30s dal thread meteo)
#ifdef WIND_VELOCITY_PIN
    // Diciamo a questo file che la variabile esiste ed è aggiornata altrove
    extern float vento_salvato_globale; 
    extern float wind_gust_globale;
    extern float wind_lull_globale;
     
    calcolovel = vento_salvato_globale; // Legge la fotografia del vento più recente senza toccare gli interrupt
   
    if (calcolovel >= 0.0f) {
        ha_vel = true;
    }
#endif

    // 3. Fusione intelligente dei valori e gestione concorrenza per l'App
    if (ha_dir || ha_vel) 
    {
        // CASO 1: Entrambi i sensori sono vivi e reali
        if (ha_dir && ha_vel) {
            m->variant.environment_metrics.wind_direction = (uint32_t)calcolodir;
            m->variant.environment_metrics.wind_speed = calcolovel;
            m->variant.environment_metrics.has_wind_direction = true; // Abilita Dir
            m->variant.environment_metrics.has_wind_speed = true;     // Abilita Vel
            
            // CASO 1
            m->variant.environment_metrics.wind_gust = wind_gust_globale;
            m->variant.environment_metrics.wind_lull = wind_lull_globale;
            m->variant.environment_metrics.has_wind_gust = true;
            m->variant.environment_metrics.has_wind_lull = true;

            LOG_INFO("METEO COMPLETO: [REALE] Dir = %u° | [REALE] Vel = %.2f km/h", (uint32_t)calcolodir, calcolovel);
        }
        // CASO 2: Solo Banderuola reale -> Velocità fasulla (2.5 km/h) per svegliare l'App
        else if (ha_dir && !ha_vel) {
            m->variant.environment_metrics.wind_direction = (uint32_t)calcolodir;
            m->variant.environment_metrics.wind_speed = 0.0f; 
            m->variant.environment_metrics.has_wind_direction = true; // Abilita Dir
            m->variant.environment_metrics.has_wind_speed = true;     // SVEGLIA l'app anche per la velocità!
            
            m->variant.environment_metrics.wind_gust = 0.0f;
            m->variant.environment_metrics.wind_lull = 0.0f;
            m->variant.environment_metrics.has_wind_gust = false;
            m->variant.environment_metrics.has_wind_lull = false;

            LOG_INFO("METEO PARZIALE: [REALE] Dir = %u° | [FASULLO] Vel = 2.50 km/h", (uint32_t)calcolodir);
        }
        // CASO 3: Solo Anemometro reale -> Direzione fasulla (180° = Sud) per orientare l'App
        else if (!ha_dir && ha_vel) {
            m->variant.environment_metrics.wind_direction = 180; //NORD!!!!!
            m->variant.environment_metrics.wind_speed = calcolovel;
            m->variant.environment_metrics.has_wind_direction = true; // SVEGLIA l'app anche per la direzione!
            m->variant.environment_metrics.has_wind_speed = true;     // Abilita Vel
            // CASO 3
            m->variant.environment_metrics.wind_gust = wind_gust_globale;
            m->variant.environment_metrics.wind_lull = wind_lull_globale;
            m->variant.environment_metrics.has_wind_gust = true;
            m->variant.environment_metrics.has_wind_lull = true;
            LOG_INFO("METEO PARZIALE: [FASULLO] Dir = 180° | [REALE] Vel = %.2f km/h", calcolovel);
        }

        // Diamo il via libera alla trasmissione della telemetria ambientale
        valid = true; 
        hasSensor = true; 


    }
}
#endif


// 3. Fusione dati Pioggia nel pacchetto telemetria
#ifdef RAIN_SENSOR_PIN

if (!leggisolouno) 
{

    // 1h: Inviato sempre, anche se è 0.000, per mantenere l'App attiva
    m->variant.environment_metrics.rainfall_1h = pioggia_ultima_ora;
    m->variant.environment_metrics.has_rainfall_1h = true; 
    
    // 24h: Inviato sempre per mantenere aggiornato il totale giornaliero
    m->variant.environment_metrics.rainfall_24h = pioggia_totale_24h;
    m->variant.environment_metrics.has_rainfall_24h = true;
 
 

    LOG_INFO("METEO PIOGGIA: Invio 1h=%.3f mm, 24h=%.3f mm", pioggia_ultima_ora, pioggia_totale_24h);

      // Diamo il via libera alla trasmissione della telemetria ambientale
        valid = true; 
        hasSensor = true; 
}
#else

// 1h: Inviato sempre, anche se è 0.000, per mantenere l'App attiva
    m->variant.environment_metrics.rainfall_1h = 0.0f;
    m->variant.environment_metrics.has_rainfall_1h = false; 
    
    // 24h: Inviato sempre per mantenere aggiornato il totale giornaliero
    m->variant.environment_metrics.rainfall_24h = 0.0f;
    m->variant.environment_metrics.has_rainfall_24h = false;

#endif

///////////////////////////////////////////////

    isTelemetryBusy = false;
///////////////////////////////////////////////

    return valid && hasSensor;
}

///////////////////////////////////////////////
/**
 * Legge l'angolo grezzo dall'AS5600 e lo converte in gradi (0-360)
 * Ritorna -1.0 in caso di errore di lettura sul bus.
 */
#ifdef HAS_WIND_DIRECTION
float EnvironmentTelemetryModule::getWindDirectionDegrees() {
        // 1. Diciamo al chip che vogliamo leggere a partire dal registro 0x0C (RAW ANGLE)
Wire.beginTransmission(EnvironmentTelemetryModule::AS5600_ADDR);
    Wire.write(0x0C); 
    if (Wire.endTransmission() != 0) {
        return -1.0f; // Il sensore non risponde (es. cavo scollegato)
    }

    // 2. Richiediamo i 2 byte successivi (0x0C e 0x0D)
        // 2. Richiediamo i 2 byte successivi (0x0C e 0x0D)
        Wire.requestFrom((uint8_t)EnvironmentTelemetryModule::AS5600_ADDR, (uint8_t)2);
        if (Wire.available() >= 2) {
            uint8_t msb = Wire.read(); // Byte alto
            uint8_t lsb = Wire.read(); // Byte basso
            // Uniamo i due byte per ottenere il valore a 12-bit (0 - 4095)
            uint16_t rawAngle = ((msb & 0x0F) << 8) | lsb;
            // Convertiamo il valore grezzo magnetico in gradi reali (0.0 - 359.9) prima dell'offset
            float gradi_magnetici = (rawAngle * 360.0f) / 4096.0f;
            
            // --- APPLICAZIONE TARATURA NORD ---
            // e applichiamo l'offset di calibrazione e invertiamo in caso di magnete montato al contrario
float degrees;
if (WIND_DIRECTION_INVERT) {
    degrees = 360.0f - gradi_magnetici - WIND_NORTH_OFFSET;
} else {
    degrees = gradi_magnetici - WIND_NORTH_OFFSET;
}
            
            // Ritorniamo nel range corretto [0.0 - 359.9] se andiamo sottozero o sopra i 360
            if (degrees < 0.0f) {
                degrees += 360.0f;
            }
            if (degrees >= 360.0f) {
                degrees -= 360.0f;
            }
            // ----------------------------------
            // <<< IL FOTTUTO LOG DI CALIBRAZIONE >>>
            LOG_INFO("[AS5600-DEBUG] RAW: %d | Gradi Magnetici: %.1f° | Offset: %.1f° | -> DIREZIONE FINALE: %.1f°", 
                     rawAngle, gradi_magnetici, (float)WIND_NORTH_OFFSET, degrees);
            return degrees;
        }
        return -1.0f; // Errore di lettura
    }
#endif
///////////////////////////////////////////////

 


meshtastic_MeshPacket *EnvironmentTelemetryModule::allocReply()
{
    if (currentRequest) {
        if (isMultiHopBroadcastRequest() && !isSensorOrRouterRole()) {
            ignoreRequest = true;
            return NULL;
        }
        auto req = *currentRequest;
        const auto &p = req.decoded;
        meshtastic_Telemetry scratch;
        meshtastic_Telemetry *decoded = NULL;
        memset(&scratch, 0, sizeof(scratch));
        if (pb_decode_from_bytes(p.payload.bytes, p.payload.size, &meshtastic_Telemetry_msg, &scratch)) {
            decoded = &scratch;
        } else {
            LOG_ERROR("Error decoding EnvironmentTelemetry module!");
            return NULL;
        }
        // Check for a request for environment metrics
        if (decoded->which_variant == meshtastic_Telemetry_environment_metrics_tag) {
            meshtastic_Telemetry m = meshtastic_Telemetry_init_zero;
            if (getEnvironmentTelemetry(&m)) {


///////////////////////////////////////////////
// --- BONIFICA TOTALE E SPEGNIMENTO FLAG ---
        
        // 1. PRESSIONE
        if (m.variant.environment_metrics.barometric_pressure < 0) {
            m.variant.environment_metrics.barometric_pressure = 0;
            m.variant.environment_metrics.has_barometric_pressure = false;
        }

        // 2. UMIDITÀ RELATIVA
        if (m.variant.environment_metrics.relative_humidity < 0) {
            m.variant.environment_metrics.relative_humidity = 0;
            m.variant.environment_metrics.has_relative_humidity = false;
        }

        // 3. IAQ (è unsigned, non può essere < 0, quindi controlliamo se è 0 o quello che vuoi)
        // Se vuoi che sparisca quando è 0:
        if (m.variant.environment_metrics.iaq == 0) {
            m.variant.environment_metrics.has_iaq = false;
        }

        // 4. VOLTAGGIO
        if (m.variant.environment_metrics.voltage < 0) {
            m.variant.environment_metrics.voltage = 0;
            m.variant.environment_metrics.has_voltage = false;
        }

        // 5. LUX (Luce)
        if (m.variant.environment_metrics.lux < 0) {
            m.variant.environment_metrics.lux = 0;
            m.variant.environment_metrics.has_lux = false;
        }

        // 6. WHITE LUX
        if (m.variant.environment_metrics.white_lux < 0) {
            m.variant.environment_metrics.white_lux = 0;
            m.variant.environment_metrics.has_white_lux = false;
        }

        // 7. PESO (Weight)
        if (m.variant.environment_metrics.weight < 0) {
            m.variant.environment_metrics.weight = 0;
            m.variant.environment_metrics.has_weight = false;
        }

        // 8. DISTANZA
        if (m.variant.environment_metrics.distance < 0) {
            m.variant.environment_metrics.distance = 0;
            m.variant.environment_metrics.has_distance = false;
        }

        // 9. RADIAZIONI
        if (m.variant.environment_metrics.radiation < 0) {
            m.variant.environment_metrics.radiation = 0;
            m.variant.environment_metrics.has_radiation = false;
        }

        // 10. RESISTENZA GAS
        if (m.variant.environment_metrics.gas_resistance < 0) {
            m.variant.environment_metrics.gas_resistance = 0;
            m.variant.environment_metrics.has_gas_resistance = false;
        }

        // 11. VELOCITÀ VENTO
        if (m.variant.environment_metrics.wind_speed < 0) {
            m.variant.environment_metrics.wind_speed = 0;
            m.variant.environment_metrics.has_wind_speed = false;
        }

        // 12. UMIDITÀ SUOLO (anche questa è unsigned)
        if (m.variant.environment_metrics.soil_moisture == 0) {
            m.variant.environment_metrics.has_soil_moisture = false;
        }
        // ------------------------------------------
///////////////////////////////////////////////

 

                LOG_INFO("Environment telemetry reply to request");
                return allocDataProtobuf(m);
            } else {
                return NULL;
            }
        }
    }
    return NULL;
}

bool EnvironmentTelemetryModule::sendTelemetry(NodeNum dest, bool phoneOnly)
{
    meshtastic_Telemetry m = meshtastic_Telemetry_init_zero;
    m.which_variant = meshtastic_Telemetry_environment_metrics_tag;
    m.time = getTime();

    if (getEnvironmentTelemetry(&m)) {

///////////////////////////////////////////////
// --- BONIFICA TOTALE E SPEGNIMENTO FLAG ---
        
        // 1. PRESSIONE
        if (m.variant.environment_metrics.barometric_pressure < 0) {
            m.variant.environment_metrics.barometric_pressure = 0;
            m.variant.environment_metrics.has_barometric_pressure = false;
        }

        // 2. UMIDITÀ RELATIVA
        if (m.variant.environment_metrics.relative_humidity < 0) {
            m.variant.environment_metrics.relative_humidity = 0;
            m.variant.environment_metrics.has_relative_humidity = false;
        }

        // 3. IAQ (è unsigned, non può essere < 0, quindi controlliamo se è 0 o quello che vuoi)
        // Se vuoi che sparisca quando è 0:
        if (m.variant.environment_metrics.iaq == 0) {
            m.variant.environment_metrics.has_iaq = false;
        }

        // 4. VOLTAGGIO
        if (m.variant.environment_metrics.voltage < 0) {
            m.variant.environment_metrics.voltage = 0;
            m.variant.environment_metrics.has_voltage = false;
        }

        // 5. LUX (Luce)
        if (m.variant.environment_metrics.lux < 0) {
            m.variant.environment_metrics.lux = 0;
            m.variant.environment_metrics.has_lux = false;
        }

        // 6. WHITE LUX
        if (m.variant.environment_metrics.white_lux < 0) {
            m.variant.environment_metrics.white_lux = 0;
            m.variant.environment_metrics.has_white_lux = false;
        }

        // 7. PESO (Weight)
        if (m.variant.environment_metrics.weight < 0) {
            m.variant.environment_metrics.weight = 0;
            m.variant.environment_metrics.has_weight = false;
        }

        // 8. DISTANZA
        if (m.variant.environment_metrics.distance < 0) {
            m.variant.environment_metrics.distance = 0;
            m.variant.environment_metrics.has_distance = false;
        }

        // 9. RADIAZIONI
        if (m.variant.environment_metrics.radiation < 0) {
            m.variant.environment_metrics.radiation = 0;
            m.variant.environment_metrics.has_radiation = false;
        }

        // 10. RESISTENZA GAS
        if (m.variant.environment_metrics.gas_resistance < 0) {
            m.variant.environment_metrics.gas_resistance = 0;
            m.variant.environment_metrics.has_gas_resistance = false;
        }

        // 11. VELOCITÀ VENTO
        if (m.variant.environment_metrics.wind_speed < 0) {
            m.variant.environment_metrics.wind_speed = 0;
            m.variant.environment_metrics.has_wind_speed = false;
        }

        // 12. UMIDITÀ SUOLO (anche questa è unsigned)
        if (m.variant.environment_metrics.soil_moisture == 0) {
            m.variant.environment_metrics.has_soil_moisture = false;
        }
        // ------------------------------------------
///////////////////////////////////////////////


        LOG_INFO("Send: barometric_pressure=%f, current=%f, gas_resistance=%f, relative_humidity=%f, temperature=%f",
                 m.variant.environment_metrics.barometric_pressure, m.variant.environment_metrics.current,
                 m.variant.environment_metrics.gas_resistance, m.variant.environment_metrics.relative_humidity,
                 m.variant.environment_metrics.temperature);
        LOG_INFO("Send: voltage=%f, IAQ=%d, distance=%f, lux=%f", m.variant.environment_metrics.voltage,
                 m.variant.environment_metrics.iaq, m.variant.environment_metrics.distance, m.variant.environment_metrics.lux);

        LOG_INFO("Send: wind speed=%fm/s, direction=%d degrees, weight=%fkg", m.variant.environment_metrics.wind_speed,
                 m.variant.environment_metrics.wind_direction, m.variant.environment_metrics.weight);

        LOG_INFO("Send: radiation=%fµR/h", m.variant.environment_metrics.radiation);

        LOG_INFO("Send: soil_temperature=%f, soil_moisture=%u", m.variant.environment_metrics.soil_temperature,
                 m.variant.environment_metrics.soil_moisture);

///////////////////////////////////////////////
                LOG_INFO("Send:  Rainfall_1h = %f, has = %d", m.variant.environment_metrics.rainfall_1h, m.variant.environment_metrics.has_rainfall_1h);
                LOG_INFO("Send:  Rainfall_24h = %f, has = %d", m.variant.environment_metrics.rainfall_24h, m.variant.environment_metrics.has_rainfall_24h);
///////////////////////////////////////////////
                
        meshtastic_MeshPacket *p = allocDataProtobuf(m);
        p->to = dest;
        p->decoded.want_response = false;
        if (config.device.role == meshtastic_Config_DeviceConfig_Role_SENSOR)
            p->priority = meshtastic_MeshPacket_Priority_RELIABLE;
        else
            p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
        // release previous packet before occupying a new spot
        if (lastMeasurementPacket != nullptr)
            packetPool.release(lastMeasurementPacket);

        lastMeasurementPacket = packetPool.allocCopy(*p);
        if (phoneOnly) {
            LOG_INFO("Send packet to phone");
            service->sendToPhone(p);
        } else {
            LOG_INFO("Send packet to mesh");
            service->sendToMesh(p, RX_SRC_LOCAL, true);

            if (config.device.role == meshtastic_Config_DeviceConfig_Role_SENSOR && config.power.is_power_saving) {
                meshtastic_ClientNotification *notification = clientNotificationPool.allocZeroed();
                notification->level = meshtastic_LogRecord_Level_INFO;
                notification->time = getValidTime(RTCQualityFromNet);
                sprintf(notification->message, "Sending telemetry and sleeping for %us interval in a moment",
                        Default::getConfiguredOrDefaultMs(moduleConfig.telemetry.environment_update_interval,
                                                          default_telemetry_broadcast_interval_secs) /
                            1000U);
                service->sendClientNotification(notification);
                sleepOnNextExecution = true;
                LOG_DEBUG("Start next execution in 5s, then sleep");
                setIntervalFromNow(FIVE_SECONDS_MS);
            }
        }
        return true;
    }
    return false;
}

AdminMessageHandleResult EnvironmentTelemetryModule::handleAdminMessageForModule(const meshtastic_MeshPacket &mp,
                                                                                 meshtastic_AdminMessage *request,
                                                                                 meshtastic_AdminMessage *response)
{
    AdminMessageHandleResult result = AdminMessageHandleResult::NOT_HANDLED;
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL

    for (TelemetrySensor *sensor : sensors) {
        result = sensor->handleAdminMessage(mp, request, response);
        if (result != AdminMessageHandleResult::NOT_HANDLED)
            return result;
    }

    if (ina219Sensor.hasSensor()) {
        result = ina219Sensor.handleAdminMessage(mp, request, response);
        if (result != AdminMessageHandleResult::NOT_HANDLED)
            return result;
    }
    if (ina260Sensor.hasSensor()) {
        result = ina260Sensor.handleAdminMessage(mp, request, response);
        if (result != AdminMessageHandleResult::NOT_HANDLED)
            return result;
    }
    if (ina3221Sensor.hasSensor()) {
        result = ina3221Sensor.handleAdminMessage(mp, request, response);
        if (result != AdminMessageHandleResult::NOT_HANDLED)
            return result;
    }
    if (max17048Sensor.hasSensor()) {
        result = max17048Sensor.handleAdminMessage(mp, request, response);
        if (result != AdminMessageHandleResult::NOT_HANDLED)
            return result;
    }
#endif
    return result;
}

#endif
