"""Sample IME process CPU without changing input, configuration or host state."""
import argparse
import json
from pathlib import Path
import time

import psutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--seconds", type=int, default=12)
parser.add_argument("--label", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = args.output.resolve()
if not output.is_relative_to(root):
    parser.error("--output must stay inside this repository")
if not 1 <= args.seconds <= 120:
    parser.error("--seconds must be between 1 and 120")
output.parent.mkdir(parents=True, exist_ok=True)
processors = psutil.cpu_count() or 1
samples = []
previous = {}
for index in range(args.seconds + 1):
    started = time.monotonic()
    rows = []
    for process in psutil.process_iter(["pid", "name", "exe", "create_time"]):
        if not (process.info["name"] or "").startswith("ArongStudyIme"):
            continue
        try:
            cpu = process.cpu_times()
            total = cpu.user + cpu.system
            key = (process.pid, process.info["create_time"])
            old = previous.get(key)
            if old:
                elapsed = started - old[0]
                rows.append({
                    "pid": process.pid,
                    "name": process.info["name"],
                    "exe": process.info["exe"],
                    "cpu_seconds": round(total - old[1], 6),
                    "wall_seconds": round(elapsed, 6),
                    "machine_cpu_percent": round(100 * (total - old[1]) / elapsed / processors, 4),
                    "rss_bytes": process.memory_info().rss,
                })
            previous[key] = (started, total)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue
    if index:
        samples.append({"sample": index - 1, "processes": rows})
    report = {
        "label": args.label,
        "logical_processors": processors,
        "measurement": "process CPU seconds / elapsed seconds / logical processors",
        "samples": samples,
    }
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if index != args.seconds:
        time.sleep(max(0, 1 - (time.monotonic() - started)))
print(f"Recorded {len(samples)} samples in {output}")
