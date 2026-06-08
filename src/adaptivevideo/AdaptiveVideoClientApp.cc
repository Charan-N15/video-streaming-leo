#include "AdaptiveVideoClientApp.h"

#include <algorithm>
#include <cmath>

#include "omnetpp/cstringtokenizer.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include <cctype>
#include <cstdlib>
#include <string>


namespace inet {

#define MSGKIND_CONNECT  0
#define MSGKIND_SEND     1

Define_Module(AdaptiveVideoClientApp);

AdaptiveVideoClientApp::~AdaptiveVideoClientApp()
{
    cancelAndDelete(timeoutMsg);
}

// Init video session state, ABR, playback buffer, and values we want to record
void AdaptiveVideoClientApp::initialize(int stage)
{
    TcpAppBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        numSegmentsToRequest = 0;
        nextSegmentIndex = 0;
        currentSegmentIndex = -1;

        currentSegmentSizeBytes = 0;
        currentSegmentBytesReceived = 0;
        segmentRequestInFlight = false;

        earlySend = false;

        startTime = par("startTime");
        stopTime = par("stopTime");

        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");

        timeoutMsg = new cMessage("timer");
        // Segment duration time
        segmentDurationSeconds = par("segmentDuration").doubleValue();
        if (segmentDurationSeconds <= 0.0)
            throw cRuntimeError("segmentDuration must be greater than 0");
        // read the adaptive bitrate settings
        adaptBitrate = par("adaptBitrate").boolValue();
        minSegmentBitrateBps = par("minSegmentBitrate").doubleValue();
        maxSegmentBitrateBps = par("maxSegmentBitrate").doubleValue();
        adaptationSafetyFactor = par("adaptationSafetyFactor").doubleValue();
        // parse bitrate ladder if ABR is enabled
        useBitrateLadder = par("useBitrateLadder").boolValue();
        parseBitrateLadder();

        if (minSegmentBitrateBps <= 0.0)
            throw cRuntimeError("minSegmentBitrate must be greater than 0");

        if (maxSegmentBitrateBps < minSegmentBitrateBps)
            throw cRuntimeError("maxSegmentBitrate must be greater than or equal to minSegmentBitrate");

        if (adaptationSafetyFactor <= 0.0)
            throw cRuntimeError("adaptationSafetyFactor must be greater than 0");

        currentSegmentBitrateBps = clampSegmentBitrate(par("segmentBitrate").doubleValue());
        lastMeasuredThroughputBps = 0.0;

        if(useBitrateLadder)
            currentSegmentBitrateBps = chooseBitrateFromLadder(currentSegmentBitrateBps);

        previousRequestedBitrateBps = currentSegmentBitrateBps;
        qualitySwitchCount = 0;
        

            

        enablePlaybackBuffer = par("enablePlaybackBuffer").boolValue();
        startupBufferTargetSeconds = par("startupBufferTarget").doubleValue();
        maxBufferTargetSeconds = par("maxBufferTarget").doubleValue();

        if (startupBufferTargetSeconds < 0.0)
            throw cRuntimeError("startupBufferTarget cannot be negative");

        if (maxBufferTargetSeconds < 0.0)
            throw cRuntimeError("maxBufferTarget cannot be negative");

        if (enablePlaybackBuffer && maxBufferTargetSeconds > 0.0 && maxBufferTargetSeconds < startupBufferTargetSeconds) {
            throw cRuntimeError("maxBufferTarget must be greater than or equal to startupBufferTarget, or 0 to disable it");
        }
        // we start with empty buffer an no stall history, resetting everything
        bufferLevelSeconds = 0.0;
        playbackStarted = false;
        playbackStalled = false;
        firstSegmentRequestSent = false;
        stallCount = 0;
        totalStallDurationSeconds = 0.0;

        // Result reecording
        segmentIndexSignal = registerSignal("segmentIndex");
        segmentSizeBytesSignal = registerSignal("segmentSizeBytes");
        segmentDownloadTimeSignal = registerSignal("segmentDownloadTime");
        segmentThroughputSignal = registerSignal("segmentThroughput");

        requestedBitrateSignal = registerSignal("requestedBitrate");
        qualitySwitchCountSignal = registerSignal("qualitySwitchCount");

        bufferLevelSignal = registerSignal("bufferLevel");
        startupDelaySignal = registerSignal("startupDelay");
        stallStartedSignal = registerSignal("stallStarted");
        stallDurationSignal = registerSignal("stallDuration");
        totalStallDurationSignal = registerSignal("totalStallDuration");

        WATCH(numSegmentsToRequest);
        WATCH(nextSegmentIndex);
        WATCH(currentSegmentIndex);
        WATCH(currentSegmentSizeBytes);
        WATCH(currentSegmentBytesReceived);
        WATCH(segmentRequestInFlight);

        WATCH(adaptBitrate);
        WATCH(currentSegmentBitrateBps);
        WATCH(lastMeasuredThroughputBps);

        WATCH(useBitrateLadder);
        WATCH(qualitySwitchCount);

        WATCH(enablePlaybackBuffer);
        WATCH(startupBufferTargetSeconds);
        WATCH(maxBufferTargetSeconds);
        WATCH(bufferLevelSeconds);
        WATCH(playbackStarted);
        WATCH(playbackStalled);
        WATCH(stallCount);
        WATCH(totalStallDurationSeconds);
    }
}

// Schedule the first tcp connection when app starts
void AdaptiveVideoClientApp::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t now = simTime();
    simtime_t start = std::max(startTime, now);

    if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(start, timeoutMsg);
    }
}

// stop the tcp connection when shutdown
void AdaptiveVideoClientApp::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);

    if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED) {
        close();
    }
}

void AdaptiveVideoClientApp::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);

    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy();
}

// keep bitrate inside min/max range specified by user params
double AdaptiveVideoClientApp::clampSegmentBitrate(double bitrateBps) const
{
    if (bitrateBps < minSegmentBitrateBps)
        return minSegmentBitrateBps;

    if (bitrateBps > maxSegmentBitrateBps)
        return maxSegmentBitrateBps;

    return bitrateBps;
}

// Parse the bitrate ladder that the user wanted
void AdaptiveVideoClientApp::parseBitrateLadder()
{
    bitrateLadderBps.clear();
    
    const char *ladder = par("bitrateLadder").stringValue();
    cStringTokenizer tokenizer(ladder);

    while (tokenizer.hasMoreTokens()) {
        std::string token = tokenizer.nextToken();
        // convert to lowercase
        for (char& c : token)
            c = std::tolower(c);

        char *endPtr = nullptr;
        double value = std::strtod(token.c_str(), &endPtr);

        if (value <= 0.0)
            continue;

        std::string unit = endPtr ? std::string(endPtr) : "";

        double multiplier = 1.0;  // default: plain number means bps
        // account for multiple different types of units. 
        if (unit == "bps" || unit == "")
            multiplier = 1.0;
        else if (unit == "kbps")
            multiplier = 1e3;
        else if (unit == "mbps")
            multiplier = 1e6;
        else if (unit == "gbps")
            multiplier = 1e9;
        else
            throw cRuntimeError("Unknown bitrate unit '%s' in bitrateLadder token '%s'",
                                unit.c_str(), token.c_str());

        double bitrateBps = value * multiplier;
        bitrateLadderBps.push_back(clampSegmentBitrate(bitrateBps));
    }

    std::sort(bitrateLadderBps.begin(), bitrateLadderBps.end());
    bitrateLadderBps.erase(std::unique(bitrateLadderBps.begin(), bitrateLadderBps.end()), bitrateLadderBps.end());

    if (useBitrateLadder && bitrateLadderBps.empty())
        throw cRuntimeError("useBitrateLadder=true, but bitrateLadder is empty or invalid");
}

// choose the highest ladder bitrate that does not exceed the target bitrate
double AdaptiveVideoClientApp::chooseBitrateFromLadder(double targetBitrateBps) const
{
    if (!useBitrateLadder || bitrateLadderBps.empty())
        return clampSegmentBitrate(targetBitrateBps);
    // start with lowest avialable bitrate then move upward if possible

    double chosenBitrateBps = bitrateLadderBps.front();
    for (double bitrateBps : bitrateLadderBps) {
        if (bitrateBps <= targetBitrateBps)
            chosenBitrateBps = bitrateBps;
        else
            break;
    }

    return chosenBitrateBps;
}


long AdaptiveVideoClientApp::computeSegmentSizeBytes(double bitrateBps) const
{
    double segmentBytes = (segmentDurationSeconds * bitrateBps) / 8.0;
    long roundedBytes = static_cast<long>(std::llround(segmentBytes));

    return std::max<long>(1, roundedBytes);
}

// new bitrate based on throughput computation
void AdaptiveVideoClientApp::updateSegmentBitrate(double measuredThroughputBps)
{
    lastMeasuredThroughputBps = measuredThroughputBps;

    if (!adaptBitrate)
        return;

    if (measuredThroughputBps <= 0.0)
        return;
    // apply the Safety Factor so bitrate is below measured throughput (conservative estimate)
    double targetBitrateBps = measuredThroughputBps * adaptationSafetyFactor;
    // either choose a ladder level if provided or just choose the targetbitrate
    double nextBitrateBps = useBitrateLadder ? chooseBitrateFromLadder(targetBitrateBps) : clampSegmentBitrate(targetBitrateBps);
    // record quality swtich if changed
    if (nextBitrateBps != currentSegmentBitrateBps) {
        qualitySwitchCount++;
        emit(qualitySwitchCountSignal, qualitySwitchCount);
    }

    currentSegmentBitrateBps = nextBitrateBps;
}

// also records metrics
void AdaptiveVideoClientApp::sendSegmentRequest()
{
    // drain buffer to current time before requesting another segment
    if (enablePlaybackBuffer && firstSegmentRequestSent)
        updatePlaybackBuffer(simTime());
    long requestLength = par("requestLength");
    if (requestLength < 1)
        requestLength = 1;

    long segmentSizeBytes = computeSegmentSizeBytes(currentSegmentBitrateBps);
    // advance segment counter
    currentSegmentIndex = nextSegmentIndex;
    nextSegmentIndex++;

    currentSegmentSizeBytes = segmentSizeBytes;
    currentSegmentBytesReceived = 0;
    currentSegmentRequestTime = simTime();
    segmentRequestInFlight = true;

    if (!firstSegmentRequestSent) {
        firstSegmentRequestSent = true;
        firstSegmentRequestTime = simTime();
        lastBufferUpdateTime = simTime();
    }
    
    emit(segmentIndexSignal, currentSegmentIndex);
    emit(segmentSizeBytesSignal, currentSegmentSizeBytes);
    emit(requestedBitrateSignal, currentSegmentBitrateBps);

    const auto& payload = makeShared<GenericAppMsg>();
    Packet *packet = new Packet("segmentRequest");
    // TCP GenericServerApp handled this
    payload->setChunkLength(B(requestLength));
    payload->setExpectedReplyLength(B(segmentSizeBytes));
    payload->setServerClose(false);
    payload->addTag<CreationTimeTag>()->setCreationTime(simTime());

    packet->insertAtBack(payload);

    EV_INFO << "Requesting video segment " << currentSegmentIndex
            << ", request size " << requestLength << " bytes"
            << ", bitrate " << currentSegmentBitrateBps << " bps"
            << ", expected segment size " << segmentSizeBytes << " bytes\n";

    sendPacket(packet);
}

void AdaptiveVideoClientApp::handleTimer(cMessage *msg)
{
    switch (msg->getKind()) {
        case MSGKIND_CONNECT:
            connect();

            if (earlySend)
                sendSegmentRequest();

            break;

        case MSGKIND_SEND:
            sendSegmentRequest();
            numSegmentsToRequest--;
            break;

        default:
            throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
    }
}

void AdaptiveVideoClientApp::socketEstablished(TcpSocket *socket)
{
    TcpAppBase::socketEstablished(socket);

    numSegmentsToRequest = par("numSegmentsPerSession");
    if (numSegmentsToRequest < 1)
        numSegmentsToRequest = 1;

    nextSegmentIndex = 0;

    if (!earlySend)
        sendSegmentRequest();

    numSegmentsToRequest--;
}

// drain buffer
void AdaptiveVideoClientApp::updatePlaybackBuffer(simtime_t now)
{
    if (!enablePlaybackBuffer || !playbackStarted)
        return;

    if (now <= lastBufferUpdateTime)
        return;

    double elapsedSeconds = (now - lastBufferUpdateTime).dbl();
    // If stalled pause buffer
    if (playbackStalled) {
        lastBufferUpdateTime = now;
        return;
    }
    // if there's enough buffered video, then continue to drain
    if (elapsedSeconds < bufferLevelSeconds) {
        bufferLevelSeconds -= elapsedSeconds;
    }
    else {
        // Buffer reached zero so playback enters stalled state
        double timeUntilEmpty = bufferLevelSeconds;

        bufferLevelSeconds = 0.0;
        playbackStalled = true;
        stallCount++;

        stallStartTime = lastBufferUpdateTime + SimTime(timeUntilEmpty);

        emit(stallStartedSignal, 1L);
    }

    lastBufferUpdateTime = now;
    emit(bufferLevelSignal, bufferLevelSeconds);
}

// add to buffer, also handle startup/stall recovery
void AdaptiveVideoClientApp::addCompletedSegmentToBuffer()
{
    if (!enablePlaybackBuffer)
        return;

    simtime_t now = simTime();
    // First drain buffer then add new segment
    updatePlaybackBuffer(now);

    bufferLevelSeconds += segmentDurationSeconds;
    // Enough video has to be buffered before.
    if (!playbackStarted) {
        if (bufferLevelSeconds >= startupBufferTargetSeconds) {
            playbackStarted = true;
            playbackStalled = false;
            lastBufferUpdateTime = now;

            double startupDelaySeconds = 0.0;
            if (firstSegmentRequestSent)
                startupDelaySeconds = (now - firstSegmentRequestTime).dbl();

            emit(startupDelaySignal, startupDelaySeconds);

            EV_INFO << "Playback started after startup delay "
                    << startupDelaySeconds << " s\n";
        }
    }
    else if (playbackStalled && bufferLevelSeconds > 0.0) {
        // Fix stall if segment has arrived
        double stallDurationSeconds = (now - stallStartTime).dbl();

        if (stallDurationSeconds < 0.0)
            stallDurationSeconds = 0.0;

        totalStallDurationSeconds += stallDurationSeconds;

        emit(stallDurationSignal, stallDurationSeconds);
        emit(totalStallDurationSignal, totalStallDurationSeconds);

        playbackStalled = false;
        lastBufferUpdateTime = now;

        EV_INFO << "Playback resumed after stall of "
                << stallDurationSeconds << " s\n";
    }

    emit(bufferLevelSignal, bufferLevelSeconds);
}

void AdaptiveVideoClientApp::completeCurrentSegment()
{
    simtime_t downloadTime = simTime() - currentSegmentRequestTime;
    double downloadSeconds = downloadTime.dbl();
    // segment size / download time = throughput
    double throughputBps = 0.0;
    if (downloadSeconds > 0.0)
        throughputBps = (currentSegmentSizeBytes * 8.0) / downloadSeconds;

    emit(segmentDownloadTimeSignal, downloadSeconds);
    emit(segmentThroughputSignal, throughputBps);
    // completed video adds to buffer, then throughput updates bitrate
    addCompletedSegmentToBuffer();
    updateSegmentBitrate(throughputBps);

    EV_INFO << "Completed video segment " << currentSegmentIndex
            << ", size " << currentSegmentSizeBytes << " bytes"
            << ", download time " << downloadSeconds << " s"
            << ", throughput " << throughputBps << " bps"
            << ", next bitrate " << currentSegmentBitrateBps << " bps\n";

    segmentRequestInFlight = false;
}

// delay next segment if playback buffer is full (above max target buffer)
simtime_t AdaptiveVideoClientApp::computeNextSegmentRequestDelay() const
{
    simtime_t delay = par("interSegmentDelay");

    if (!enablePlaybackBuffer)
        return delay;

    if (maxBufferTargetSeconds <= 0.0)
        return delay;

    if (!playbackStarted)
        return delay;

    if (playbackStalled)
        return delay;

    if (bufferLevelSeconds <= maxBufferTargetSeconds)
        return delay;
    // wait long enough for playback to drain the buffer back to the target
    double bufferWaitSeconds = bufferLevelSeconds - maxBufferTargetSeconds;
    simtime_t bufferWait = SimTime(bufferWaitSeconds);

    if (bufferWait > delay)
        return bufferWait;

    return delay;
}

void AdaptiveVideoClientApp::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{
    long bytesArrived = msg->getByteLength();

    TcpAppBase::socketDataArrived(socket, msg, urgent);

    if (!segmentRequestInFlight)
        return;

    currentSegmentBytesReceived += bytesArrived;

    if (currentSegmentBytesReceived < currentSegmentSizeBytes)
        return;

    completeCurrentSegment();

    if (numSegmentsToRequest > 0) {
        if (timeoutMsg) {
            simtime_t delay = computeNextSegmentRequestDelay();

            EV_INFO << "Scheduling next segment request after "
                    << delay
                    << " because buffer level is "
                    << bufferLevelSeconds
                    << " s and maxBufferTarget is "
                    << maxBufferTargetSeconds
                    << " s\n";

            rescheduleAfterOrDeleteTimer(delay, MSGKIND_SEND);
        }
    }
    else if (socket->getState() != TcpSocket::LOCALLY_CLOSED) {
        EV_INFO << "Final segment completed, closing TCP session\n";
        close();
    }

}

void AdaptiveVideoClientApp::rescheduleAfterOrDeleteTimer(simtime_t delay, short int msgKind)
{
    if (stopTime < SIMTIME_ZERO || simTime() + delay < stopTime) {
        timeoutMsg->setKind(msgKind);
        rescheduleAfter(delay, timeoutMsg);
    }
    else {
        cancelAndDelete(timeoutMsg);
        timeoutMsg = nullptr;
    }
}

void AdaptiveVideoClientApp::close()
{
    TcpAppBase::close();
    cancelEvent(timeoutMsg);
}

void AdaptiveVideoClientApp::socketClosed(TcpSocket *socket)
{
    TcpAppBase::socketClosed(socket);

    if (timeoutMsg) {
        simtime_t delay = par("idleInterval");
        rescheduleAfterOrDeleteTimer(delay, MSGKIND_CONNECT);
    }
}

void AdaptiveVideoClientApp::socketFailure(TcpSocket *socket, int code)
{
    TcpAppBase::socketFailure(socket, code);

    segmentRequestInFlight = false;
    currentSegmentBytesReceived = 0;

    if (timeoutMsg) {
        simtime_t delay = par("reconnectInterval");
        rescheduleAfterOrDeleteTimer(delay, MSGKIND_CONNECT);
    }
}

} // namespace inet
