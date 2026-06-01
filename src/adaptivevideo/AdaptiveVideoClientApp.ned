package leosatellites.adaptivevideo;

@namespace(inet);

import inet.applications.contract.IApp;

//
// Adaptive segmented video client over TCP.
//
// The client requests video in segments from TcpGenericServerApp.
// Each request tells the server how many bytes to return.
//
// Version 2 adds:
//  - Neubot-DASH-style throughput-based bitrate adaptation
//  - playback buffer tracking
//  - startup delay tracking
//  - playback stall detection
//
simple AdaptiveVideoClientApp like IApp
{
    parameters:
        string localAddress = default("");
        int localPort = default(-1);

        string connectAddress = default("");
        int connectPort = default(1000);

        double startTime @unit(s) = default(1s);
        double stopTime @unit(s) = default(-1s);

        int numSegmentsPerSession = default(15);
        int requestLength @unit(B) = default(200B);

        // Segment timing
        double segmentDuration @unit(s) = default(2s);

        // Initial bitrate. If adaptBitrate=false, this stays fixed.
        // If adaptBitrate=true, it is only the bitrate for the first segment.
        double segmentBitrate @unit(bps) = default(3Mbps);

        // Neubot-style adaptation:
        // next bitrate = measured throughput × adaptationSafetyFactor
        bool adaptBitrate = default(false);
        double minSegmentBitrate @unit(bps) = default(100kbps);
        double maxSegmentBitrate @unit(bps) = default(30Mbps);
        double adaptationSafetyFactor = default(1.0);

        // if this is enabled, chooses from a fixed "ladder" instead of any bitrate value
        bool useBitrateLadder = default(true);
        string bitrateLadder = default("300kbps 750kbps 1200kbps 2500kbps 4000kbps");

        // Playback model
        bool enablePlaybackBuffer = default(false);
        double startupBufferTarget @unit(s) = default(4s);

        // If > 0, the client pauses before requesting the next segment
        // when the playback buffer is above this target.
        double maxBufferTarget @unit(s) = default(0s);

        // Delay before requesting the next segment after the previous one completes
        volatile double interSegmentDelay @unit(s) = default(0s);

        // Session handling
        volatile double idleInterval @unit(s) = default(1000s);
        volatile double reconnectInterval @unit(s) = default(30s);

        int timeToLive = default(-1);
        int dscp = default(-1);
        int tos = default(-1);

        @display("i=block/app");
        @lifecycleSupport;

        double stopOperationExtraTime @unit(s) = default(-1s);
        double stopOperationTimeout @unit(s) = default(2s);

        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @signal[connect](type=long);

        @signal[segmentIndex](type=long);
        @signal[segmentSizeBytes](type=long);
        @signal[segmentDownloadTime](type=double);
        @signal[segmentThroughput](type=double);

        @signal[requestedBitrate](type=double);
        @signal[qualitySwitchCount](type=long);

        @signal[bufferLevel](type=double);
        @signal[startupDelay](type=double);
        @signal[stallStarted](type=long);
        @signal[stallDuration](type=double);
        @signal[totalStallDuration](type=double);

        @statistic[packetReceived](
            title="packets received";
            source=packetReceived;
            record=count,"sum(packetBytes)","vector(packetBytes)";
            interpolationmode=none
        );

        @statistic[packetSent](
            title="packets sent";
            source=packetSent;
            record=count,"sum(packetBytes)","vector(packetBytes)";
            interpolationmode=none
        );

        @statistic[endToEndDelay](
            title="end-to-end delay";
            source="dataAge(packetReceived)";
            unit=s;
            record=histogram,weightedHistogram,vector;
            interpolationmode=none
        );

        @statistic[numActiveSessions](
            title="number of active sessions";
            source=warmup(sum(connect));
            record=max,timeavg,vector;
            interpolationmode=sample-hold;
            autoWarmupFilter=false
        );

        @statistic[numSessions](
            title="total number of sessions";
            source="sum(connect+1)/2";
            record=last
        );

        @statistic[segmentIndex](
            title="segment index";
            source=segmentIndex;
            record=vector;
            interpolationmode=none
        );

        @statistic[segmentSizeBytes](
            title="segment size";
            source=segmentSizeBytes;
            unit=B;
            record=vector;
            interpolationmode=none
        );

        @statistic[segmentDownloadTime](
            title="segment download time";
            source=segmentDownloadTime;
            unit=s;
            record=vector,mean,max;
            interpolationmode=none
        );

        @statistic[segmentThroughput](
            title="segment throughput";
            source=segmentThroughput;
            unit=bps;
            record=vector,mean;
            interpolationmode=none
        );

        @statistic[requestedBitrate](
            title="requested segment bitrate";
            source=requestedBitrate;
            unit=bps;
            record=vector,mean;
            interpolationmode=none
        );

        @statistic[qualitySwitchCount](
            title="quality switch count";
            source=qualitySwitchCount;
            record=last,vector;
            interpolationmode=sample-hold
        );

        @statistic[bufferLevel](
            title="playback buffer level";
            source=bufferLevel;
            unit=s;
            record=vector,last,min,max;
            interpolationmode=sample-hold
        );

        @statistic[startupDelay](
            title="startup delay before playback begins";
            source=startupDelay;
            unit=s;
            record=last
        );

        @statistic[stallStarted](
            title="playback stall count";
            source=stallStarted;
            record=count
        );

        @statistic[stallDuration](
            title="playback stall duration";
            source=stallDuration;
            unit=s;
            record=vector,sum,mean,max;
            interpolationmode=none
        );

        @statistic[totalStallDuration](
            title="total playback stall duration";
            source=totalStallDuration;
            unit=s;
            record=last
        );

    gates:
        input socketIn @labels(TcpCommand/up);
        output socketOut @labels(TcpCommand/down);
}