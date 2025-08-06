#!/bin/sh
set -e

# Step 1: Check if running on Milk-V Duo
is_milkv_duo() {
  if [ -f /proc/device-tree/model ]; then
    model=$(tr -d '\0' < /proc/device-tree/model)
    echo "$model" | grep -q "Milk-V Duo256M"
    return $?
  else
    return 1
  fi
}

if ! is_milkv_duo; then
  echo "❌ This system is not a Milk-V Duo 256M. Aborting installation."
  exit 1
fi

echo "🔧 Installing kernel modules..."
mkdir -p /mnt/system/ko
cp -v morse.ko /mnt/system/ko/
cp -v dot11ah.ko /mnt/system/ko/

echo "🛠️ Installing binaries..."
cp -v wpa_supplicant_s1g /usr/sbin/
cp -v wpa_cli_s1g /usr/sbin/
cp -v wpa_passphrase_s1g /usr/sbin/
cp -v hostapd_s1g /usr/sbin/
cp -v hostapd_cli_s1g /usr/sbin/
cp -v morsectrl /usr/bin/
cp -v morse_cli /usr/bin/

echo "📄 Installing configuration files..."
mkdir -p /etc/wpa_supplicant
cp -v wpa_supplicant.conf /etc/wpa_supplicant/

echo "📜 Installing runtime scripts..."
cp -v resize.sh /root/
cp -v start-halow.sh /root/

echo "📡 Installing firmware..."
mkdir -p /lib/firmware/
cp -v *.bin /lib/firmware/
ln -sf /lib/firmware/bcf_mf08651_us.bin /lib/firmware/bcf_boardtype_0801.bin
ln -sf /lib/firmware/bcf_mf10220.bin /lib/firmware/bcf_default.bin

echo "✅ Installation completed successfully."

