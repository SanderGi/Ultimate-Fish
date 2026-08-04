#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <Chess-Ultimate.xapk> <empty-output-directory>" >&2
    exit 2
fi

xapk=$1
output=$2

if [ ! -f "$xapk" ]; then
    echo "input does not exist: $xapk" >&2
    exit 2
fi

if [ -e "$output" ] && [ "$(find "$output" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]; then
    echo "refusing to overwrite non-empty output directory: $output" >&2
    exit 2
fi

mkdir -p "$output/splits" "$output/recovered"
unzip -tq "$xapk"
unzip -q "$xapk" '*.apk' 'manifest.json' -d "$output/splits"

echo "XAPK SHA-256:"
shasum -a 256 "$xapk"
echo "Split SHA-256 values:"
find "$output/splits" -type f -name '*.apk' -exec shasum -a 256 {} \;

apksigner_path=${APKSIGNER:-}
if [ -z "$apksigner_path" ] && [ -n "${ANDROID_HOME:-}" ]; then
    apksigner_path=$(find "$ANDROID_HOME/build-tools" -type f -name apksigner -print 2>/dev/null | sort -V | tail -n 1)
fi

if [ -n "$apksigner_path" ] && [ -x "$apksigner_path" ]; then
    for apk in "$output"/splits/*.apk; do
        "$apksigner_path" verify --verbose --print-certs "$apk"
    done
else
    echo "APKSIGNER or ANDROID_HOME is not configured; signature verification skipped." >&2
fi

for apk in "$output"/splits/*.apk; do
    if unzip -Z1 "$apk" | grep -q '^assets/bin/Data/Managed/Metadata/global-metadata.dat$'; then
        unzip -p "$apk" assets/bin/Data/Managed/Metadata/global-metadata.dat > "$output/recovered/global-metadata.dat"
    fi
    if unzip -Z1 "$apk" | grep -q '^lib/armeabi-v7a/libil2cpp.so$'; then
        unzip -p "$apk" lib/armeabi-v7a/libil2cpp.so > "$output/recovered/libil2cpp.so"
    fi
done

if [ ! -s "$output/recovered/global-metadata.dat" ] || [ ! -s "$output/recovered/libil2cpp.so" ]; then
    echo "could not recover both IL2CPP inputs" >&2
    exit 1
fi

echo "Recovered IL2CPP SHA-256 values:"
shasum -a 256 "$output/recovered/global-metadata.dat" "$output/recovered/libil2cpp.so"
