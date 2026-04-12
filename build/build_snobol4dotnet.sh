#!/bin/bash
# build_snobol4dotnet.sh — build snobol4dotnet from snobol4ever/snobol4dotnet
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPO="$ROOT/snobol4dotnet"
[ -d "$REPO" ] || { echo "Clone snobol4ever/snobol4dotnet first"; exit 1; }
cd "$REPO"
dotnet build -c Release
echo "Built: snobol4dotnet"
