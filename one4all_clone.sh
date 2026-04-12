#!/usr/bin/env bash
# one4all_clone.sh — clone all snobol4ever repos as siblings of one4all
#
# Usage (from anywhere, even inside an existing one4all checkout):
#   bash one4all_clone.sh --token ghp_YOUR_TOKEN
#
# Clones into the parent directory of wherever one4all lives.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT="$(dirname "$SCRIPT_DIR")"
cd "$PARENT"
exec bash "$SCRIPT_DIR/snobol4ever_clone.sh" "$@"
