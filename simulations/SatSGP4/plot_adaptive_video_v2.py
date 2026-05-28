from pathlib import Path
import matplotlib.pyplot as plt
import pandas as pd
import re

VEC_FILE = Path("results/AdaptiveVideoV2-#0.vec")

TARGETS = {
    "segmentDownloadTime:vector": "segment_download_time_s",
    "segmentThroughput:vector": "segment_throughput_bps",
    "requestedBitrate:vector": "requested_bitrate_bps",
    "bufferLevel:vector": "buffer_level_s",
    "stallDuration:vector": "stall_duration_s",
}

vector_ids = {}
samples = {key: [] for key in TARGETS.values()}

with VEC_FILE.open("r", encoding="utf-8") as f:
    for line in f:
        parts = line.split()
        if len(parts) >= 4 and parts[0] == "vector":
            vec_id = parts[1]
            module = parts[2]
            name = parts[3]
            if module == "SatSGP4.groundStation[0].app[0]" and name in TARGETS:
                vector_ids[vec_id] = TARGETS[name]
            continue

        if len(parts) >= 4 and parts[0] in vector_ids:
            kind = vector_ids[parts[0]]
            try:
                time_s = float(parts[-2])
                value = float(parts[-1])
                samples[kind].append((time_s, value))
            except ValueError:
                pass

print("Vector IDs:", vector_ids)
for key, values in samples.items():
    print(f"{key}: {len(values)} samples")

def to_df(key, value_col):
    return pd.DataFrame(samples[key], columns=["time_s", value_col])

download = to_df("segment_download_time_s", "download_time_s")
throughput = to_df("segment_throughput_bps", "throughput_bps")
bitrate = to_df("requested_bitrate_bps", "bitrate_bps")
buffer = to_df("buffer_level_s", "buffer_s")
stalls = to_df("stall_duration_s", "stall_s")

if not download.empty:
    plt.figure(figsize=(10, 5))
    plt.plot(download["time_s"], download["download_time_s"], marker="o")
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Segment download time (s)")
    plt.title("Adaptive Video: Segment Download Time")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("adaptive_v2_segment_download_time.png", dpi=200)

if not throughput.empty:
    throughput["throughput_mbps"] = throughput["throughput_bps"] / 1e6
    plt.figure(figsize=(10, 5))
    plt.plot(throughput["time_s"], throughput["throughput_mbps"], marker="o")
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Segment throughput (Mbps)")
    plt.title("Adaptive Video: Measured Segment Throughput")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("adaptive_v2_segment_throughput.png", dpi=200)

if not bitrate.empty:
    bitrate["bitrate_mbps"] = bitrate["bitrate_bps"] / 1e6
    plt.figure(figsize=(10, 5))
    plt.step(bitrate["time_s"], bitrate["bitrate_mbps"], where="post")
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Requested bitrate (Mbps)")
    plt.title("Adaptive Video: Requested Bitrate")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("adaptive_v2_requested_bitrate.png", dpi=200)

if not buffer.empty:
    plt.figure(figsize=(10, 5))
    plt.step(buffer["time_s"], buffer["buffer_s"], where="post")
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Buffer level (s)")
    plt.title("Adaptive Video: Playback Buffer Level")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("adaptive_v2_buffer_level.png", dpi=200)

if not stalls.empty:
    plt.figure(figsize=(10, 5))
    plt.bar(stalls["time_s"], stalls["stall_s"], width=0.2)
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Stall duration (s)")
    plt.title("Adaptive Video: Stall Durations")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("adaptive_v2_stall_durations.png", dpi=200)

print("\nSaved plots:")
for name in [
    "adaptive_v2_segment_download_time.png",
    "adaptive_v2_segment_throughput.png",
    "adaptive_v2_requested_bitrate.png",
    "adaptive_v2_buffer_level.png",
    "adaptive_v2_stall_durations.png",
]:
    if Path(name).exists():
        print(" ", name)
