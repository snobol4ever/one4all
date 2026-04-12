#!/bin/bash
# build_scrip.sh — build the scrip compiler from one4all/src
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
apt-get install -y build-essential libgmp-dev m4 nasm libgc-dev -qq 2>/dev/null || true
cd "$ROOT/src" && make
echo "Built: $ROOT/scrip"
