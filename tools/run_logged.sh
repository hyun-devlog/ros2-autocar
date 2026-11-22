#!/usr/bin/env bash
set -euo pipefail

log="${1:?usage: run_logged.sh <log-file> <command...>}"
shift
mkdir -p "$(dirname "${log}")"

RCUTILS_LOGGING_BUFFERED_STREAM=1 PYTHONUNBUFFERED=1 "$@" 2>&1 | tee -i "${log}"