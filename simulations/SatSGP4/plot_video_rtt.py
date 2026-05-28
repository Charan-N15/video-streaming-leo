from pathlib import Path
import matplotlib.pyplot as plt
import pandas as pd

VEC_FILE = Path("results/VideoUDP-#0.vec")
RTT_VECTOR_ID = "4968"

samples = []

with VEC_FILE.open("r", encoding="utf-8") as f:
    for line in f:
        parts = line.split()
        if len(parts) >= 4 and parts[0] == RTT_VECTOR_ID:
            try:
                time_s = float(parts[-2])
                rtt_s = float(parts[-1])
                samples.append((time_s, rtt_s))
            except ValueError:
                pass

df = pd.DataFrame(samples, columns=["time_s", "rtt_s"])
df["rtt_ms"] = df["rtt_s"] * 1000

print("RTT samples:", len(df))
print(df["rtt_ms"].describe())

plt.figure(figsize=(11, 6))
plt.plot(df["time_s"], df["rtt_ms"], linewidth=1)
plt.xlabel("Simulation time (s)")
plt.ylabel("RTT (ms)")
plt.title("Ping RTT During UDP Video Stream")
plt.grid(True)
plt.tight_layout()
plt.savefig("video_udp_rtt.png", dpi=200)

print("Saved video_udp_rtt.png")
