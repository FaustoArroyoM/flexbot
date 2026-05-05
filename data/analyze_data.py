import csv, statistics, math
from pathlib import Path

files = [
    "/home/fausto/Desktop/flexbot/data/output_2026-05-05_18-21-49.csv",
    "/home/fausto/Desktop/flexbot/data/output_2026-05-05_18-23-15.csv",
]
for fp in files:
    rows = []
    with open(fp, newline="") as f:
        lines = [ln for ln in f if not ln.startswith("#")]
    r = csv.DictReader(lines)
    for row in r:
        for k in ["pc_proc_us", "pc_wait_us", "pc_loop_us", "esp_comp_us", "esp_comm_us"]:
            row[k] = float(row[k])
        rows.append(row)
    nz = [x for x in rows if x["esp_comp_us"] != 0 or x["esp_comm_us"] != 0]
    print("\n", Path(fp).name)
    print("rows", len(rows), "esp_nonzero", len(nz))

    def stat(key, subset=None):
        arr = [x[key] for x in (subset or rows)]
        return (min(arr), statistics.mean(arr), max(arr))

    for key in ["pc_wait_us", "pc_proc_us", "pc_loop_us"]:
        mn, av, mx = stat(key)
        print(f"{key}: min={mn:.1f} mean={av:.1f} max={mx:.1f}")
    if nz:
        for key in ["esp_comp_us", "esp_comm_us"]:
            mn, av, mx = stat(key, nz)
            print(f"{key}(nonzero): min={mn:.1f} mean={av:.1f} max={mx:.1f}")
