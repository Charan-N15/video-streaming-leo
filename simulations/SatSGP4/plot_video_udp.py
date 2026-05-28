import pandas as pd
import matplotlib.pyplot as plt

CSV_FILE = "video_udp_vectors.csv"

df = pd.read_csv(CSV_FILE)
df["type"] = df["type"].astype(str).str.strip()
df["module"] = df["module"].fillna("")
df["name"] = df["name"].fillna("")

vectors = df[df["type"] == "vector"].copy()

print("Vector rows in CSV:", len(vectors))
print("\nMatching vector rows:")
matches = vectors[
    vectors["name"].str.contains("endToEndDelay|throughput|packetReceived|packetSent", case=False, regex=True)
]
print(matches[["module", "name"]].drop_duplicates().to_string(index=False))

def expand_vector_rows(rows):
    """Expand OMNeT++ CSV-R vector rows into one row per sample."""
    expanded = []

    for _, row in rows.iterrows():
        times_raw = str(row.get("vectime", "")).strip()
        values_raw = str(row.get("vecvalue", "")).strip()

        if not times_raw or not values_raw or times_raw == "nan" or values_raw == "nan":
            continue

        times = times_raw.split()
        values = values_raw.split()

        if len(times) != len(values):
            continue

        for t, v in zip(times, values):
            try:
                expanded.append({
                    "module": row["module"],
                    "name": row["name"],
                    "time_s": float(t),
                    "value": float(v),
                })
            except ValueError:
                pass

    return pd.DataFrame(expanded)

# Client-side video delay
delay_rows = vectors[
    vectors["module"].str.contains(r"groundStation\[0\]\.app\[0\]", regex=True)
    & vectors["name"].str.contains("endToEndDelay", case=False)
]

# Client-side video throughput
throughput_rows = vectors[
    vectors["module"].str.contains(r"groundStation\[0\]\.app\[0\]", regex=True)
    & vectors["name"].str.contains("throughput", case=False)
]

delay = expand_vector_rows(delay_rows)
throughput = expand_vector_rows(throughput_rows)

print("\nExpanded delay samples:", len(delay))
print("Expanded throughput samples:", len(throughput))

if not delay.empty:
    delay["delay_ms"] = delay["value"] * 1000
    print("\nEnd-to-end delay summary, ms:")
    print(delay["delay_ms"].describe())

    plt.figure(figsize=(10, 5))
    plt.plot(delay["time_s"], delay["delay_ms"], linewidth=1)
    plt.xlabel("Simulation time (s)")
    plt.ylabel("End-to-end delay (ms)")
    plt.title("UDP Video Stream End-to-End Delay")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("video_udp_end_to_end_delay.png", dpi=200)
    print("Saved video_udp_end_to_end_delay.png")

if not throughput.empty:
    throughput["throughput_mbps"] = throughput["value"] / 1e6
    print("\nThroughput summary, Mbps:")
    print(throughput["throughput_mbps"].describe())

    plt.figure(figsize=(10, 5))
    plt.plot(throughput["time_s"], throughput["throughput_mbps"], linewidth=1)
    plt.xlabel("Simulation time (s)")
    plt.ylabel("Throughput (Mbps)")
    plt.title("UDP Video Stream Throughput")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("video_udp_throughput.png", dpi=200)
    print("Saved video_udp_throughput.png")

if delay.empty and throughput.empty:
    print("\nNo app-level delay/throughput samples found.")
    print("Run this to inspect actual vector names:")
    print("python3 inspect_video_vectors.py")
