from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

VEC_FILE = Path("results/AdaptiveVideoV2-#0.vec")

TARGETS = {
    ("SatSGP4.groundStation[0].app[0]", "segmentDownloadTime:vector"): "segment_download_time_s",
    ("SatSGP4.groundStation[0].app[0]", "segmentThroughput:vector"): "segment_throughput_bps",
    ("SatSGP4.groundStation[0].app[0]", "requestedBitrate:vector"): "requested_bitrate_bps",
    ("SatSGP4.groundStation[0].app[0]", "qualitySwitchCount:vector"): "quality_switch_count",
    ("SatSGP4.groundStation[0].app[0]", "bufferLevel:vector"): "buffer_level_s",
    ("SatSGP4.groundStation[0].app[0]", "stallDuration:vector"): "stall_duration_s",
    ("SatSGP4.groundStation[0].app[1]", "rtt:vector"): "rtt_s",
}

vector_ids = {}
samples = {name: [] for name in TARGETS.values()}

with VEC_FILE.open("r", encoding="utf-8") as f:
    for line in f:
        parts = line.split()

        if len(parts) >= 4 and parts[0] == "vector":
            vec_id = parts[1]
            module = parts[2]
            name = parts[3]

            key = (module, name)
            if key in TARGETS:
                vector_ids[vec_id] = TARGETS[key]
            continue

        if len(parts) >= 4 and parts[0] in vector_ids:
            metric = vector_ids[parts[0]]
            try:
                time_s = float(parts[-2])
                value = float(parts[-1])
                samples[metric].append((time_s, value))
            except ValueError:
                pass

print("Parsed vector IDs:")
for vec_id, metric in vector_ids.items():
    print(f"  {vec_id}: {metric}")

print("\nSample counts:")
for metric, values in samples.items():
    print(f"  {metric}: {len(values)}")

def make_df(metric, value_name):
    return pd.DataFrame(samples[metric], columns=["time_s", value_name])

download = make_df("segment_download_time_s", "download_time_s")
throughput = make_df("segment_throughput_bps", "throughput_bps")
bitrate = make_df("requested_bitrate_bps", "bitrate_bps")
switches = make_df("quality_switch_count", "quality_switch_count")
buffer = make_df("buffer_level_s", "buffer_s")
stalls = make_df("stall_duration_s", "stall_s")
rtt = make_df("rtt_s", "rtt_s")

summary_rows = []

if not download.empty:
    summary_rows.append(["completed_segments", len(download)])
    summary_rows.append(["avg_segment_download_time_s", download["download_time_s"].mean()])
    summary_rows.append(["max_segment_download_time_s", download["download_time_s"].max()])

if not throughput.empty:
    throughput["throughput_mbps"] = throughput["throughput_bps"] / 1e6
    summary_rows.append(["avg_segment_throughput_mbps", throughput["throughput_mbps"].mean()])

if not bitrate.empty:
    bitrate["bitrate_mbps"] = bitrate["bitrate_bps"] / 1e6
    summary_rows.append(["avg_requested_bitrate_mbps", bitrate["bitrate_mbps"].mean()])
    summary_rows.append(["final_requested_bitrate_mbps", bitrate["bitrate_mbps"].iloc[-1]])

if not switches.empty:
    summary_rows.append(["final_quality_switch_count", switches["quality_switch_count"].iloc[-1]])
else:
    summary_rows.append(["final_quality_switch_count", 0])

if not buffer.empty:
    summary_rows.append(["max_buffer_level_s", buffer["buffer_s"].max()])
    summary_rows.append(["final_buffer_level_s", buffer["buffer_s"].iloc[-1]])

if not stalls.empty:
    summary_rows.append(["stall_count", len(stalls)])
    summary_rows.append(["total_stall_duration_s", stalls["stall_s"].sum()])
else:
    summary_rows.append(["stall_count", 0])
    summary_rows.append(["total_stall_duration_s", 0])

if not rtt.empty:
    rtt["rtt_ms"] = rtt["rtt_s"] * 1000
    summary_rows.append(["avg_rtt_ms", rtt["rtt_ms"].mean()])
    summary_rows.append(["p95_rtt_ms", rtt["rtt_ms"].quantile(0.95)])
    summary_rows.append(["max_rtt_ms", rtt["rtt_ms"].max()])

summary = pd.DataFrame(summary_rows, columns=["metric", "value"])
summary.to_csv("adaptive_video_v2_summary.csv", index=False)

print("\nSummary:")
print(summary.to_string(index=False))

def save_line_plot(df, x, y, title, ylabel, filename, step=False, marker=True):
    if df.empty:
        return

    plt.figure(figsize=(10, 5))

    if step:
        plt.step(df[x], df[y], where="post")
    elif marker:
        plt.plot(df[x], df[y], marker="o")
    else:
        plt.plot(df[x], df[y])

    plt.xlabel("Simulation time (s)")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(filename, dpi=200)
    plt.close()

save_line_plot(download, "time_s", "download_time_s",
               "Segment Download Time", "Download time (s)",
               "finished_segment_download_time.png")

save_line_plot(throughput, "time_s", "throughput_mbps",
               "Measured Segment Throughput", "Throughput (Mbps)",
               "finished_segment_throughput.png")

save_line_plot(bitrate, "time_s", "bitrate_mbps",
               "Requested Bitrate / Quality Level", "Bitrate (Mbps)",
               "finished_requested_bitrate.png", step=True)

save_line_plot(switches, "time_s", "quality_switch_count",
               "Cumulative Quality Switch Count", "Switch count",
               "finished_quality_switch_count.png", step=True)

save_line_plot(buffer, "time_s", "buffer_s",
               "Playback Buffer Level", "Buffer level (s)",
               "finished_buffer_level.png", step=True)

save_line_plot(rtt, "time_s", "rtt_ms",
               "RTT During Adaptive Video Run", "RTT (ms)",
               "finished_rtt_during_video.png", marker=False)

if not stalls.empty:
    plt.figure(figsize=(10, 5))
    plt.bar(stalls["time_s"], stalls["stall_s"], width=0.2)
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Stall duration (s)")
    plt.title("Playback Stall Durations")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("finished_stall_durations.png", dpi=200)
    plt.close()

print("\nSaved plots:")
for path in sorted(Path(".").glob("finished_*.png")):
    print(" ", path)
print(" ", "adaptive_video_v2_summary.csv")
