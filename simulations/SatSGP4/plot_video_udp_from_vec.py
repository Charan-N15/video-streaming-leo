import shlex
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

VEC_FILE = Path("results/VideoUDP-#0.vec")

TARGETS = {
    ("SatSGP4.groundStation[0].app[0]", "endToEndDelay:vector"): "delay",
    ("SatSGP4.groundStation[0].app[0]", "throughput:vector"): "throughput",
}

vector_id_to_kind = {}
samples = {
    "delay": [],
    "throughput": [],
}

with VEC_FILE.open("r", encoding="utf-8") as f:
    for line in f:
        line = line.strip()

        if not line:
            continue

        # Vector declaration line, e.g.:
        # vector 667 "module.path" "endToEndDelay:vector" TV
        if line.startswith("vector "):
            parts = shlex.split(line)
            if len(parts) >= 4:
                vector_id = parts[1]
                module = parts[2]
                name = parts[3]

                kind = TARGETS.get((module, name))
                if kind:
                    vector_id_to_kind[vector_id] = kind

            continue

        # Actual vector sample rows begin with numeric vector ID.
        # Depending on OMNeT++ vector format, rows may be:
        #   id time value
        # or:
        #   id eventnum time value
        # Using the last two columns reliably gives time and value.
        first = line.split(maxsplit=1)[0]
        if first in vector_id_to_kind:
            parts = line.split()
            if len(parts) < 3:
                continue

            try:
                time_s = float(parts[-2])
                value = float(parts[-1])
            except ValueError:
                continue

            kind = vector_id_to_kind[first]
            samples[kind].append((time_s, value))

delay = pd.DataFrame(samples["delay"], columns=["time_s", "delay_s"])
throughput = pd.DataFrame(samples["throughput"], columns=["time_s", "throughput_bps"])

print("Parsed samples")
print("--------------")
print(f"Delay samples:      {len(delay)}")
print(f"Throughput samples: {len(throughput)}")

if not delay.empty:
    delay["delay_ms"] = delay["delay_s"] * 1000

    print("\nEnd-to-end delay summary, ms:")
    print(delay["delay_ms"].describe())

    plt.figure(figsize=(11, 6))
    plt.plot(delay["time_s"], delay["delay_ms"], linewidth=1)
    plt.xlabel("Simulation time (s)")
    plt.ylabel("End-to-end delay (ms)")
    plt.title("UDP Video Stream End-to-End Delay")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("video_udp_end_to_end_delay.png", dpi=200)
    print("\nSaved video_udp_end_to_end_delay.png")

if not throughput.empty:
    throughput["throughput_mbps"] = throughput["throughput_bps"] / 1e6

    print("\nThroughput summary, Mbps:")
    print(throughput["throughput_mbps"].describe())

    plt.figure(figsize=(11, 6))
    plt.plot(throughput["time_s"], throughput["throughput_mbps"], linewidth=1)
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Throughput (Mbps)")
    plt.title("UDP Video Stream Throughput")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("video_udp_throughput.png", dpi=200)
    print("Saved video_udp_throughput.png")
