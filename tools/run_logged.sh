#!/usr/bin/env bash
set -euo pipefail

log="${1:?usage: run_logged.sh <log-file> <command...>}"
shift
root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ -f "${root}/install/setup.bash" ]]; then
  set +u
  source "${root}/install/setup.bash"
  set -u
fi

mkdir -p "$(dirname "${log}")"
RCUTILS_LOGGING_BUFFERED_STREAM=1 PYTHONUNBUFFERED=1 "$@" 2>&1 | tee -i "${log}"