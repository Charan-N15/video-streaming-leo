# **Atlas**: OMNeT++ and INET framework for simulation Low-Earth Orbit Satellite Constellations.

# LEO Adaptive Video Streaming Model

This project extends the `leosatellites` OMNeT++/INET simulation framework with a simplified DASH-like adaptive video streaming model and a configurable RTT delay variation model.

## Code Structure

### `src/adaptivevideo/`

This folder contains the custom adaptive video streaming application.

`AdaptiveVideoClientApp.ned` defines the application parameters, recorded signals, and statistics for the video streaming model. These include segment size, segment download time, measured throughput, requested bitrate, playback buffer level, startup delay, stalls, and quality switches.

`AdaptiveVideoClientApp.cc` implements the main DASH-like video streaming logic. The client requests fixed-duration video segments over TCP, computes segment size from the selected bitrate, measures segment download time, calculates throughput, updates the adaptive bitrate decision, and tracks playback buffer behavior.

`AdaptiveVideoClientApp.h` declares the state variables and helper functions used by the adaptive video client.

The video client is based on INET's TCP client/server application structure, so INET handles the lower-level TCP socket behavior while this project adds the video-specific logic. https://github.com/inet-framework/inet/blob/master/src/inet/applications/tcpapp/TcpBasicClientApp.cc

## Adaptive Video Model

The video is represented as fixed-duration segments. 

After each segment finishes downloading, the client records:

- segment download time
- segment throughput
- requested bitrate
- playback buffer level
- quality switch count
- stall behavior

The adaptive bitrate logic uses the measured throughput from the previous segment, applies a safety factor, and selects a bitrate from the configured bitrate ladder.

## Jitter / Delay Variation Model

The delay variation model is implemented in:

`src/common` 

The modified files are:

- `LeoChannelConstructor.cc`
- `LeoChannelConstructor.h`
- `LeoChannelConstructor.ned`

The jitter model adds extra delay on top of the simulator's existing geometric propagation delay. The added helper functions are:

- `computeNormalJitterSeconds`
- `computeReconfigurationSpikeSeconds`
- `applyLeoDelayVariation`

These functions add normal RTT noise and periodic reconfiguration-style delay spikes before the final channel delay is applied.

## Simulation Configuration

The main simulation settings are configured in:

- `omnetpp.ini`
- `omnetpp_saveRouting.ini`

`omnetpp_saveRouting.ini` is used to generate saved routing files for the selected satellite topology.

`omnetpp.ini` defines the experiment setup, including the ground stations, video segment settings, bitrate ladder, playback buffer settings, and jitter parameters.

The main adaptive video configuration is:

`AdaptiveVideoV2`

This configuration runs the TCP video client, TCP server, and RTT probe using the selected LEO satellite topology.

## Main Output Metrics

The model records both video-level and network-level behavior.

Video client metrics include:

- requested bitrate
- segment download time
- segment throughput
- playback buffer level
- startup delay
- stall duration
- quality switch count

The RTT probe records round-trip time during the simulation.



@inproceedings{omnetpp-leosatellites-model,
  author = {Valentine, Aiden and Parisis, George},
  title = {{Developing and experimenting with LEO satellite constellations in OMNeT++}},
  booktitle = {Proceedings of the 8th OMNeT++ Community Summit Conference},
  address = {Hamburg, Germany},
  Year = {2021}
}
```
