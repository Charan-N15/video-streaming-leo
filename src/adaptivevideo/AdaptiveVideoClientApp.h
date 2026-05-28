#ifndef __LEOSATELLITES_ADAPTIVEVIDEOCLIENTAPP_H
#define __LEOSATELLITES_ADAPTIVEVIDEOCLIENTAPP_H

#include "inet/applications/tcpapp/TcpAppBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"

namespace inet {

class AdaptiveVideoClientApp : public TcpAppBase
{
  protected:
    cMessage *timeoutMsg = nullptr;
    bool earlySend = false;

    int numSegmentsToRequest = 0;
    int nextSegmentIndex = 0;
    int currentSegmentIndex = -1;

    long currentSegmentSizeBytes = 0;
    long currentSegmentBytesReceived = 0;

    simtime_t currentSegmentRequestTime;
    bool segmentRequestInFlight = false;

    simtime_t startTime;
    simtime_t stopTime;

    // Adaptive bitrate state
    bool adaptBitrate = false;
    double currentSegmentBitrateBps = 0.0;
    double lastMeasuredThroughputBps = 0.0;
    double minSegmentBitrateBps = 0.0;
    double maxSegmentBitrateBps = 0.0;
    double adaptationSafetyFactor = 1.0;

    // Playback buffer state
    bool enablePlaybackBuffer = false;
    double segmentDurationSeconds = 0.0;
    double startupBufferTargetSeconds = 0.0;
    double bufferLevelSeconds = 0.0;

    bool playbackStarted = false;
    bool playbackStalled = false;
    bool firstSegmentRequestSent = false;

    simtime_t firstSegmentRequestTime;
    simtime_t lastBufferUpdateTime;
    simtime_t stallStartTime;

    int stallCount = 0;
    double totalStallDurationSeconds = 0.0;

    // Existing segment signals
    simsignal_t segmentIndexSignal;
    simsignal_t segmentSizeBytesSignal;
    simsignal_t segmentDownloadTimeSignal;
    simsignal_t segmentThroughputSignal;

    // New adaptation / playback signals
    simsignal_t requestedBitrateSignal;
    simsignal_t bufferLevelSignal;
    simsignal_t startupDelaySignal;
    simsignal_t stallStartedSignal;
    simsignal_t stallDurationSignal;
    simsignal_t totalStallDurationSignal;

  protected:
    virtual long computeSegmentSizeBytes(double bitrateBps) const;
    virtual double clampSegmentBitrate(double bitrateBps) const;
    virtual void updateSegmentBitrate(double measuredThroughputBps);

    virtual void sendSegmentRequest();
    virtual void completeCurrentSegment();

    virtual void updatePlaybackBuffer(simtime_t now);
    virtual void addCompletedSegmentToBuffer();

    virtual void rescheduleAfterOrDeleteTimer(simtime_t delay, short int msgKind);

    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleTimer(cMessage *msg) override;

    virtual void socketEstablished(TcpSocket *socket) override;
    virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
    virtual void socketClosed(TcpSocket *socket) override;
    virtual void socketFailure(TcpSocket *socket, int code) override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual void close() override;

  public:
    AdaptiveVideoClientApp() {}
    virtual ~AdaptiveVideoClientApp();
};

} // namespace inet

#endif