#!/usr/bin/env bash
# snobol4ever_clone.sh — clone all snobol4ever repos into the current directory
#
# Usage:
#   bash snobol4ever_clone.sh [--token TOKEN] [--ssh]
#
# Options:
#   --token TOKEN   GitHub PAT (or set GH_TOKEN env var)
#   --ssh           Clone via SSH instead of HTTPS (uses git@github.com:snobol4ever/)
#
# Each repo is cloned as a sibling directory of wherever you run this script.
# Safe to re-run — already-cloned repos are updated with git pull --rebase.

set -euo pipefail

ORG="snobol4ever"
REPOS=(
    .github
    corpus
    harness
    one4all
    snobol4artifact
    snobol4csharp
    snobol4dotnet
    snobol4jvm
    snobol4python
    x32
    x64
)

# ── Argument parsing ──────────────────────────────────────────────────────────
USE_SSH=0
TOKEN="${GH_TOKEN:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --token) TOKEN="$2"; shift 2 ;;
        --ssh)   USE_SSH=1;  shift   ;;
        *)       echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── URL builder ───────────────────────────────────────────────────────────────
repo_url() {
    local repo="$1"
    if [[ $USE_SSH -eq 1 ]]; then
        echo "git@github.com:${ORG}/${repo}.git"
    elif [[ -n "$TOKEN" ]]; then
        echo "https://${TOKEN}@github.com/${ORG}/${repo}"
    else
        echo "https://github.com/${ORG}/${repo}"
    fi
}

# ── Clone or update ───────────────────────────────────────────────────────────
DEST="$(pwd)"
GREEN='\033[0;32m'; YELLOW='\033[0;33m'; RESET='\033[0m'

for repo in "${REPOS[@]}"; do
    dir="${DEST}/${repo}"
    url="$(repo_url "$repo")"

    if [[ -d "${dir}/.git" ]]; then
        echo -e "${YELLOW}UPDATE${RESET}  ${repo}"
        git -C "$dir" pull --rebase --quiet 2>&1 | tail -1 || true
    else
        echo -e "${GREEN}CLONE${RESET}   ${repo}"
        git clone --quiet "$url" "$dir"
    fi
done

echo ""
echo "Done. Repos in: $DEST"
echo "  one4all:  ${DEST}/one4all"
echo "  corpus:   ${DEST}/corpus"
echo "  harness:  ${DEST}/harness"
echo "  x64:      ${DEST}/x64  (SPITBOL oracle)"
