#!/usr/bin/env bash
# Download official 3-4-5 piece Syzygy WDL + DTZ endgame tablebases (~939 MB total)
# into data/syzygy/ for Apex-C 3000 + Syzygy (5-Man TB). Resumable & auto-fallback!
set -euo pipefail

DEST="${1:-data/syzygy}"
mkdir -p "$DEST"
UA="Mozilla/5.0 (Chess-in-C Syzygy Fetcher)"

download_dir() {
    local base_url="$1"
    local html
    if ! html=$(curl -fsSL -A "$UA" "$base_url/" 2>/dev/null); then
        return 1
    fi
    local links
    links=$(printf "%s" "$html" | grep -oE 'href="[^"]+\.rtb[wz]"' | sed 's/href="//;s/"$//' | sort -u)
    local total
    total=$(printf "%s\n" "$links" | grep -c '\.rtb[wz]$' || true)
    if [ "$total" -lt 10 ]; then
        return 1
    fi
    echo "Source: $base_url/ ($total files)"
    local count=0
    for fn in $links; do
        count=$((count + 1))
        local base_fn
        base_fn=$(basename "$fn")
        if [ ! -f "$DEST/$base_fn" ]; then
            printf "[%3d/%3d] %s\n" "$count" "$total" "$base_fn"
            curl -fsSL -A "$UA" -C - "$base_url/$base_fn" -o "$DEST/$base_fn.tmp" \
                && mv -f "$DEST/$base_fn.tmp" "$DEST/$base_fn"
        fi
    done
    return 0
}

echo "Fetching Syzygy 3-4-5 piece endgame tablebases into $DEST ..."

# Mirror 1: Lichess split 3-4-5-wdl + 3-4-5-dtz
if download_dir "https://tablebase.lichess.ovh/tables/standard/3-4-5-wdl" \
   && download_dir "https://tablebase.lichess.ovh/tables/standard/3-4-5-dtz"; then
    echo "Done! Installed $(ls -1 "$DEST"/*.rtb* 2>/dev/null | wc -l) Syzygy files in $DEST."
    exit 0
fi

# Mirror 2: Steinar Gunderson's official Sesse Syzygy mirror
echo "Lichess mirror unavailable — trying Sesse official Syzygy mirror..."
if download_dir "https://tablebase.sesse.net/syzygy/3-4-5" \
   || download_dir "http://tablebase.sesse.net/syzygy/3-4-5"; then
    echo "Done! Installed $(ls -1 "$DEST"/*.rtb* 2>/dev/null | wc -l) Syzygy files in $DEST."
    exit 0
fi

echo "Error: could not reach Lichess or Sesse Syzygy mirrors." >&2
exit 1
