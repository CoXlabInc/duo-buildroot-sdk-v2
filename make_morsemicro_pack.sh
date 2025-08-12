#!/bin/bash
set -e

# Define output directory and archive name
PACKAGE_DIR="morsemicro"
ARCHIVE_NAME="${PACKAGE_DIR}.tar.gz"

# Clean up any previous output
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

# Copy all required files into the flat package directory
cp morse_driver/morse.ko "$PACKAGE_DIR"/
cp morse_driver/dot11ah/dot11ah.ko "$PACKAGE_DIR"/

cp hostap/wpa_supplicant/wpa_supplicant_s1g "$PACKAGE_DIR"/
cp hostap/wpa_supplicant/wpa_cli_s1g "$PACKAGE_DIR"/
cp hostap/wpa_supplicant/wpa_passphrase_s1g "$PACKAGE_DIR"/
cp hostap/wpa_supplicant/wpa_supplicant.conf "$PACKAGE_DIR"/

cp runtime-scripts/resize.sh "$PACKAGE_DIR"/

cp hostap/hostapd/hostapd_s1g "$PACKAGE_DIR"/
cp hostap/hostapd/hostapd_cli_s1g "$PACKAGE_DIR"/

cp morsectrl_rel_1_14_1_2024_Dec_05/morsectrl "$PACKAGE_DIR"/
cp morsectrl_rel_1_14_1_2024_Dec_05/morse_cli "$PACKAGE_DIR"/

cp morsemicro_fwbin/*.bin "$PACKAGE_DIR"/

cp install_morsemicro.sh "$PACKAGE_DIR"/

# Create tar.gz archive with the folder included in the structure
tar -czf "$ARCHIVE_NAME" "$PACKAGE_DIR"

echo "✅ Packaging completed: $ARCHIVE_NAME"

