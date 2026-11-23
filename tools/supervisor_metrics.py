#!/usr/bin/env python3
"""Compute supervisor metrics (M-01..M-07) from experiment bags and draw charts.

Usage:
  python3 tools/supervisor_metrics.py BAG [BAG ...] --out docs/metrics

Times are the recorder receive times in each bag.
"""

import argparse
import json
import os

SENSOR_TOPICS = {
    "/scan_raw": 2.0,
    "/ros_robot_controller/imu_raw": 2.0,
    "/odom_raw": 2.0,
}
COMMAND_CHANNELS = {
    "/cmd_vel": "/safety_gate/cmd_vel",
    "/controller/cmd_vel": "/safety_gate/controller/cmd_vel",
}
GATE_OUTPUTS = tuple(COMMAND_CHANNELS.values())
STATUS_TOPIC = "/robot_status"

COMMAND_TIMEOUT = 1.0
STATUS_TIMEOUT = 0.5
SEARCH_WINDOW = 5.0
TEARDOWN_SEC = (
    10.0  # topics that stop for good this close to the end are shutdown, not faults
)
TARGET = {"sensor": 2.2, "command": 1.1, "monitor": 0.6, "gate": 0.6}


# ---------- pure functions (no ROS needed) ----------


def gaps(times, min_gap, end_time):
    """Return (last_before_gap, resume_or_None) for silences longer than min_gap."""
    result = []
    for prev, cur in zip(times, times[1:]):
        if cur - prev > min_gap:
            result.append((prev, cur))
    if times and end_time - times[-1] > min_gap:
        result.append((times[-1], None))
    return result


def first_at_or_after(series, t0, predicate, limit=SEARCH_WINDOW):
    for t, value in series:
        if t < t0:
            continue
        if t > t0 + limit:
            return None
        if predicate(value):
            return t
    return None


def value_before(series, t0):
    last = None
    for t, value in series:
        if t > t0:
            break
        last = value
    return last


def is_zero(x):
    return x is not None and abs(x) < 1e-9


def sensor_events(data, end_time):
    events = []
    status = data.get(STATUS_TOPIC, [])
    outputs = sorted((t, x) for topic in GATE_OUTPUTS for t, x in data.get(topic, []))
    for topic, timeout in SENSOR_TOPICS.items():
        times = [t for t, _ in data.get(topic, [])]
        for last, resume in gaps(times, timeout, end_time):
            fault = first_at_or_after(status, last, lambda s: s[0] == 2)
            stop = first_at_or_after(outputs, last, is_zero)
            ok = (
                first_at_or_after(status, resume, lambda s: s[0] == 0)
                if resume is not None
                else None
            )
            events.append(
                {
                    "kind": "sensor",
                    "topic": topic,
                    "last": last,
                    "resume": resume,
                    "detect": None if fault is None else fault - last,
                    "stop": None if stop is None else stop - last,
                    "recover": None if ok is None else ok - resume,
                }
            )
    return events


def command_events(data, end_time):
    events = []
    for source, output in COMMAND_CHANNELS.items():
        times = [t for t, _ in data.get(source, [])]
        out = data.get(output, [])
        out_times = [t for t, _ in out]
        for last, _ in gaps(times, COMMAND_TIMEOUT, end_time):
            if is_zero(value_before(out, last)):
                continue
            if not any(last - STATUS_TIMEOUT <= t <= last for t in out_times):
                continue  # gate itself was silent: reported by gate_silence_events
            stop = first_at_or_after(out, last + 1e-6, is_zero)
            events.append(
                {
                    "kind": "command",
                    "topic": source,
                    "last": last,
                    "resume": None,
                    "detect": None,
                    "stop": None if stop is None else stop - last,
                    "recover": None,
                }
            )
    return events


def gate_silence_events(data, end_time):
    """Gate output went silent while commands kept arriving (e.g. gate crash)."""
    events = []
    for source, output in COMMAND_CHANNELS.items():
        src_times = [t for t, _ in data.get(source, [])]
        out = data.get(output, [])
        for last, resume in gaps([t for t, _ in out], STATUS_TIMEOUT, end_time):
            until = resume if resume is not None else end_time
            if not any(last < t < until for t in src_times):
                continue
            if is_zero(value_before(out, last)):
                continue
            stop = first_at_or_after(out, last + 1e-6, is_zero)
            events.append(
                {
                    "kind": "gate",
                    "topic": output,
                    "last": last,
                    "resume": resume,
                    "detect": None,
                    "stop": None if stop is None else stop - last,
                    "recover": None,
                }
            )
    return events


def monitor_events(data, end_time):
    events = []
    times = [t for t, _ in data.get(STATUS_TOPIC, [])]
    outputs = sorted((t, x) for topic in GATE_OUTPUTS for t, x in data.get(topic, []))
    for last, resume in gaps(times, STATUS_TIMEOUT, end_time):
        if resume is None:
            continue
        stop = first_at_or_after(outputs, last + 1e-6, is_zero)
        events.append(
            {
                "kind": "monitor",
                "topic": STATUS_TOPIC,
                "last": last,
                "resume": resume,
                "detect": None,
                "stop": None if stop is None else stop - last,
                "recover": None,
            }
        )
    return events


def status_summary(status):
    if len(status) < 2:
        return {"time_ok_pct": None, "dtc_counts": {}}
    total = status[-1][0] - status[0][0]
    ok = sum(t1 - t0 for (t0, s0), (t1, _) in zip(status, status[1:]) if s0[0] == 0)
    counts = {}
    previous = set()
    for _, (_, _, codes) in status:
        current = set(codes)
        for code in current - previous:
            counts[code] = counts.get(code, 0) + 1
        previous = current
    return {
        "time_ok_pct": 100.0 * ok / total if total > 0 else None,
        "dtc_counts": counts,
    }


def scan_rate_per_second(times):
    if not times:
        return [], []
    start = int(times[0])
    bins = {}
    for t in times:
        bins[int(t) - start] = bins.get(int(t) - start, 0) + 1
    xs = list(range(0, max(bins) + 1))
    return [start + x for x in xs], [bins.get(x, 0) for x in xs]


def judge(event):
    if event["stop"] is None:
        return "NO STOP"
    return "PASS" if event["stop"] <= TARGET[event["kind"]] else "SLOW"


# ---------- bag reading (needs ROS) ----------


def read_bag(path):
    from rclpy.serialization import deserialize_message
    import rosbag2_py
    from rosidl_runtime_py.utilities import get_message

    reader = rosbag2_py.SequentialReader()
    reader.open(
        rosbag2_py.StorageOptions(uri=path, storage_id="sqlite3"),
        rosbag2_py.ConverterOptions("cdr", "cdr"),
    )
    types = {}
    for topic in reader.get_all_topics_and_types():
        try:
            types[topic.name] = get_message(topic.type)
        except (ModuleNotFoundError, AttributeError, ValueError):
            print(f"warning: cannot decode {topic.name}; source the workspace first")

    data = {}
    start = end = None
    while reader.has_next():
        topic, raw, stamp_ns = reader.read_next()
        t = stamp_ns * 1e-9
        start = t if start is None else start
        end = t
        value = None
        if topic == STATUS_TOPIC and topic in types:
            msg = deserialize_message(raw, types[topic])
            value = (
                msg.level,
                msg.raw_level,
                [f.message.split(" ")[0] for f in msg.faults],
            )
        elif (topic in COMMAND_CHANNELS or topic in GATE_OUTPUTS) and topic in types:
            value = deserialize_message(raw, types[topic]).linear.x
        data.setdefault(topic, []).append((t, value))
    return data, start, end


# ---------- charts ----------


def plot_timeline(name, data, events, start, out_dir):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(
        3, 1, figsize=(10, 6.5), sharex=True, gridspec_kw={"height_ratios": [1, 1.2, 1]}
    )
    status = data.get(STATUS_TOPIC, [])
    ax = axes[0]
    if status:
        ax.step(
            [t - start for t, _ in status],
            [s[0] for _, s in status],
            where="post",
            color="#d03b3b",
            lw=2,
            label="level",
        )
        ax.step(
            [t - start for t, _ in status],
            [s[1] for _, s in status],
            where="post",
            color="#898781",
            lw=1,
            ls="--",
            label="raw_level",
        )
    ax.set_yticks([0, 1, 2])
    ax.set_yticklabels(["OK", "DEGRADED", "FAULT"])
    ax.set_ylim(-0.3, 2.3)
    ax.legend(loc="upper right", fontsize=8, frameon=False)
    ax.set_title(
        f"{name}: /robot_status and safety_gate output", fontsize=10, loc="left"
    )

    ax = axes[1]
    styles = {GATE_OUTPUTS[0]: ("#2a78d6", "-"), GATE_OUTPUTS[1]: ("#eb6834", "--")}
    for topic in GATE_OUTPUTS:
        series = [(t, x) for t, x in data.get(topic, []) if x is not None]
        if series:
            color, ls = styles[topic]
            ax.step(
                [t - start for t, _ in series],
                [x for _, x in series],
                where="post",
                color=color,
                ls=ls,
                lw=1.5,
                label=topic,
            )
    ax.set_ylabel("linear.x (m/s)", fontsize=8)
    ax.legend(loc="upper right", fontsize=8, frameon=False)

    ax = axes[2]
    xs, ys = scan_rate_per_second([t for t, _ in data.get("/scan_raw", [])])
    if xs:
        ax.step(
            [x - start for x in xs],
            ys,
            where="post",
            color="#1baf7a",
            lw=1.5,
            label="/scan_raw msgs per s",
        )
        ax.axhline(9, color="#898781", ls=":", lw=1, label="min 9 Hz")
    ax.set_ylabel("Hz", fontsize=8)
    ax.set_xlabel("seconds since recording start", fontsize=8)
    ax.legend(loc="upper right", fontsize=8, frameon=False)

    for event in events:
        for a in axes:
            a.axvline(event["last"] - start, color="#6250d6", lw=0.8, alpha=0.6)
    for a in axes:
        a.grid(axis="y", color="#e1e0d9", lw=0.8)
        a.spines[["top", "right"]].set_visible(False)
    fig.tight_layout()
    path = os.path.join(out_dir, f"{name}_timeline.png")
    fig.savefig(path, dpi=110)
    plt.close(fig)
    return path


def plot_latency_summary(rows, out_dir):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    measured = [r for r in rows if r["stop"] is not None]
    if not measured:
        return None
    labels = [f"{r['bag']} {r['kind']} {r['topic']}" for r in measured]
    fig, ax = plt.subplots(figsize=(9, 0.38 * len(measured) + 1.2))
    ys = range(len(measured))
    ax.barh(ys, [r["stop"] for r in measured], color="#2a78d6", height=0.55)
    for y, r in zip(ys, measured):
        ax.plot([TARGET[r["kind"]]] * 2, [y - 0.35, y + 0.35], color="#d03b3b", lw=1.5)
        ax.text(r["stop"] + 0.03, y, f"{r['stop']:.2f} s", va="center", fontsize=8)
    ax.set_yticks(list(ys))
    ax.set_yticklabels(labels, fontsize=8)
    ax.invert_yaxis()
    ax.set_xlabel(
        "time from last good message to zero command (s); red line = target", fontsize=8
    )
    ax.spines[["top", "right"]].set_visible(False)
    ax.grid(axis="x", color="#e1e0d9", lw=0.8)
    fig.tight_layout()
    path = os.path.join(out_dir, "stop_latency.png")
    fig.savefig(path, dpi=110)
    plt.close(fig)
    return path


# ---------- report ----------


def fmt(x):
    return "—" if x is None else f"{x:.3f}"


def write_report(rows, summaries, out_dir):
    lines = [
        "# Supervisor metrics (generated)",
        "",
        "Generated by `tools/supervisor_metrics.py`. Times are recorder receive times.",
        "",
        "| Bag | Kind | Topic | Last good msg (Unix s) | M-01 detect (s) | M-02 stop (s) | M-07 recover (s) | Target (s) | Result |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for r in rows:
        lines.append(
            f"| {r['bag']} | {r['kind']} | `{r['topic']}` | {r['last']:.3f} | {fmt(r['detect'])} | "
            f"{fmt(r['stop'])} | {fmt(r['recover'])} | ≤ {TARGET[r['kind']]} | {judge(r)} |"
        )
    lines += ["", "| Bag | M-06 time in OK (%) | M-05 DTC onsets |", "|---|---|---|"]
    for name, s in summaries.items():
        dtc = ", ".join(f"{k} ×{v}" for k, v in sorted(s["dtc_counts"].items())) or "—"
        pct = "—" if s["time_ok_pct"] is None else f"{s['time_ok_pct']:.1f}"
        lines.append(f"| {name} | {pct} | {dtc} |")
    path = os.path.join(out_dir, "summary.md")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out_dir, "summary.json"), "w") as f:
        json.dump({"events": rows, "status": summaries}, f, indent=2)
    return path


def bag_name(path):
    parent = os.path.basename(os.path.dirname(os.path.abspath(path)))
    return parent.rsplit("_", 2)[0] if parent.count("_") >= 2 else parent


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("bags", nargs="+", help="bag directories (.../bag)")
    parser.add_argument("--out", default="docs/metrics")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)

    rows = []
    summaries = {}
    for bag in args.bags:
        name = bag_name(bag)
        data, start, end = read_bag(bag)
        events = (
            sensor_events(data, end)
            + command_events(data, end)
            + monitor_events(data, end)
            + gate_silence_events(data, end)
        )
        status_times = [t for t, _ in data.get(STATUS_TOPIC, [])]
        status_end = status_times[-1] if status_times else end
        events = [
            e
            for e in events
            if not (e["resume"] is None and status_end - e["last"] < TEARDOWN_SEC)
        ]
        events.sort(key=lambda e: e["last"])
        for e in events:
            e["bag"] = name
        rows += events
        summaries[name] = status_summary(data.get(STATUS_TOPIC, []))
        print(
            f"{name}: {len(events)} events -> {plot_timeline(name, data, events, start, args.out)}"
        )

    print("summary ->", write_report(rows, summaries, args.out))
    chart = plot_latency_summary(rows, args.out)
    if chart:
        print("chart ->", chart)


if __name__ == "__main__":
    main()
