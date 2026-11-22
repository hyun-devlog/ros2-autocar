#!/usr/bin/env bash
set -euo pipefail

name="${1:?usage: record_experiment.sh <experiment-id>}"
root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ -f "${root}/install/setup.bash" ]]; then
  set +u
  source "${root}/install/setup.bash"
  set -u
fi

out="${root}/experiments_raw/${name}_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${out}"

topics=(
  /robot_status
  /cmd_vel
  /controller/cmd_vel
  /safety_gate/cmd_vel
  /safety_gate/controller/cmd_vel
  /scan_raw
  /ros_robot_controller/imu_raw
  /odom_raw
)

echo "recording ${#topics[@]} topics to ${out}/bag (Ctrl+C to stop)"
ros2 bag record -o "${out}/bag" "${topics[@]}"