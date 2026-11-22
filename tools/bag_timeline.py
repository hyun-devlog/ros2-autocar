#!/usr/bin/env python3
"""Print a timeline of topic starts, stops and value changes from an experiment bag."""

import argparse

from rclpy.serialization import deserialize_message
import rosbag2_py
from rosidl_runtime_py.utilities import get_message

GAP_SEC = 0.5
VALUE_TOPICS = {
    '/cmd_vel',
    '/controller/cmd_vel',
    '/safety_gate/cmd_vel',
    '/safety_gate/controller/cmd_vel',
    '/robot_status',
}


def value_of(msg):
    if hasattr(msg, 'level'):
        faults = ', '.join(f.message for f in msg.faults)
        return f'level {msg.level} raw {msg.raw_level} [{faults}]'
    return f'linear.x {round(msg.linear.x, 3)}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bag', help='bag directory, e.g. experiments_raw/GATE-02_.../bag')
    parser.add_argument('--gap', type=float, default=GAP_SEC, help='seconds of silence = stop')
    args = parser.parse_args()

    reader = rosbag2_py.SequentialReader()
    reader.open(
        rosbag2_py.StorageOptions(uri=args.bag, storage_id='sqlite3'),
        rosbag2_py.ConverterOptions('cdr', 'cdr'))
    types = {t.name: get_message(t.type) for t in reader.get_all_topics_and_types()}

    events = []
    last_seen = {}
    last_value = {}
    start = None

    while reader.has_next():
        topic, data, stamp_ns = reader.read_next()
        t = stamp_ns * 1e-9
        if start is None:
            start = t

        prev = last_seen.get(topic)
        if prev is None:
            events.append((t, topic, 'first message'))
        elif t - prev > args.gap:
            events.append((prev, topic, f'stopped (silent {t - prev:.2f} s)'))
            events.append((t, topic, 'resumed'))
        last_seen[topic] = t

        if topic in VALUE_TOPICS:
            value = value_of(deserialize_message(data, types[topic]))
            if last_value.get(topic) != value:
                events.append((t, topic, value))
                last_value[topic] = value

    for topic, t in last_seen.items():
        events.append((t, topic, 'last message'))

    for t, topic, text in sorted(events):
        print(f'{t:.3f}  +{t - start:8.3f}  {topic:34s} {text}')


if __name__ == '__main__':
    main()
