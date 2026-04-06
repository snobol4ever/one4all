#!/usr/bin/env bash
# one4all_clone.sh — clone all snobol4ever repos as siblings of one4all
#
# Usage (from anywhere):
#   bash /path/to/one4all/one4all_clone.sh [--token TOKEN] [--ssh]
#
# Clones everything into the same parent directory that contains one4all.
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$(dirname "$SCRIPT_DIR")"   # parent of one4all

USE_SSH=0
TOKEN="${GH_TOKEN:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --token) TOKEN="$2"; shift 2 ;;
        --ssh)   USE_SSH=1;  shift   ;;
        *)       echo "Unknown option: $1"; exit 1 ;;
    esac
done

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

GREEN='\033[0;32m'; YELLOW='\033[0;33m'; RESET='\033[0m'

echo "Cloning/updating snobol4ever repos into: $DEST"
echo ""

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
echo "Done. Layout:"
echo "  ${DEST}/one4all   (interpreter + emitter)"
echo "  ${DEST}/corpus    (test corpus)"
echo "  ${DEST}/harness   (test harness)"
echo "  ${DEST}/x64       (SPITBOL oracle)"
echo "  ${DEST}/.github   (HQ docs + session state)"
