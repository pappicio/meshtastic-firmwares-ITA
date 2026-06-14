#pragma once

#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "BaseTelemetryModule.h"

#ifndef ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE
#define ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE 0
#endif

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "NodeDB.h"
#include "ProtobufModule.h"
#include "detect/ScanI2CConsumer.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

class EnvironmentTelemetryModule : private concurrency::OSThread,
                                   public ScanI2CConsumer,
                                   public BaseTelemetryModule,
                                   public ProtobufModule<meshtastic_Telemetry>
{
    CallbackObserver<EnvironmentTelemetryModule, const meshtastic::Status *> nodeStatusObserver =
        CallbackObserver<EnvironmentTelemetryModule, const meshtastic::Status *>(this,
                                                                                 &EnvironmentTelemetryModule::handleStatusUpdate);

  public:

    ///////////////////////////////////////////////
    static EnvironmentTelemetryModule *instance;
    void aggiornaTemperaturaBox();
    bool leggisolouno = false;
    void forcePublishToMqtt();
    static volatile bool pendingMqttPublish;
    ///////////////////////////////////////////////

    EnvironmentTelemetryModule()
        : concurrency::OSThread("EnvironmentTelemetry"), ScanI2CConsumer(),
          ProtobufModule("EnvironmentTelemetry", meshtastic_PortNum_TELEMETRY_APP, &meshtastic_Telemetry_msg)
    {
        instance = this;
        lastMeasurementPacket = nullptr;
        nodeStatusObserver.observe(&nodeStatus->onNewStatus);
        setIntervalFromNow(10 * 1000);
    }

    virtual bool wantUIFrame() override;
#if !HAS_SCREEN
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
#else
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
#endif

  protected:
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_Telemetry *p) override;
    virtual int32_t runOnce() override;
    bool getEnvironmentTelemetry(meshtastic_Telemetry *m);
    virtual meshtastic_MeshPacket *allocReply() override;
    bool sendTelemetry(NodeNum dest = NODENUM_BROADCAST, bool wantReplies = false);
    virtual AdminMessageHandleResult handleAdminMessageForModule(const meshtastic_MeshPacket &mp,
                                                                 meshtastic_AdminMessage *request,
                                                                 meshtastic_AdminMessage *response) override;
    void i2cScanFinished(ScanI2C *i2cScanner);

  private:
    bool firstTime = 1;
    meshtastic_MeshPacket *lastMeasurementPacket;
    uint32_t sendToPhoneIntervalMs = SECONDS_IN_MINUTE * 1000;
    uint32_t lastSentToPhone = 0;
    ///////////////////////////////////////////////
#ifdef HAS_WIND_DIRECTION
    float getWindDirectionDegrees();
#endif
    static const uint8_t AS5600_ADDR = 0x36;
    ///////////////////////////////////////////////
};

#endif