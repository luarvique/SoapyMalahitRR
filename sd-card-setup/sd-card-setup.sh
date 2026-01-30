#!/bin/bash

# Add GPIO access library
#sudo apt install gpiod
#sudo apt install libgpiod-dev

echo "Shutting down OpenWebRX..."
sudo systemctl stop openwebrx

echo "Getting latest package lists..."
sudo apt update

echo "Installing Soapy Malahit driver..."
sudo apt install soapysdr-module-malahit-rr

echo "Configuring ALSA for I2S audio..."
sudo install -o root -g root -m 644 asound.conf /etc
sudo install -o root -g root -m 644 alsa.conf /usr/share/alsa

echo "Installing I2S audio device tree..."
sudo kdtc \
    ./generic_audio_out_i2s_slave.dts \
    /boot/firmware/overlays/generic_audio_out_i2s_slave.dtbo

echo "Enabling I2S and SPI hardware..."
CONFIG=/boot/firmware/config.txt
sudo touch $CONFIG

if ! grep -qxF "dtoverlay=generic_audio_out_i2s_slave" $CONFIG; then
    echo "dtoverlay=generic_audio_out_i2s_slave" | sudo tee -a $CONFIG
fi

if ! grep -qxF "dtparam=i2s=on" $CONFIG; then
    echo "dtparam=i2s=on" | sudo tee -a $CONFIG
fi

if ! grep -qxF "dtparam=spi=on" $CONFIG; then
    echo "dtparam=spi=on" | sudo tee -a $CONFIG
fi

echo "Letting OpenWebRX access networking, audio, GPIO, SPI..."
sudo usermod -aG netdev openwebrx
sudo usermod -aG audio openwebrx
sudo usermod -aG gpio openwebrx
sudo usermod -aG spi openwebrx
echo "Groups for" `groups openwebrx`

echo "Letting netdev group control Network Manager..."
PKLA=/etc/polkit-1/localauthority/50-local.d/org.freedesktop.NetworkManager.pkla
if [ ! -e $PKLA ]; then
    sudo install -o root -g root -m 644 netdev-nmcli.pkla $PKLA
fi

echo "# Installing Malahit-specific OpenWebRX config files..."
sudo install -o openwebrx -g openwebrx -m 644 ./settings.json /var/lib/openwebrx
sudo install -o root -g root -m 644 ./openwebrx.conf /etc/openwebrx
echo "Adding OpenWebRX administrator account..."
sudo openwebrx admin adduser admin

echo "Upgrading Linux and RPI firmware..."
sudo apt full-upgrade
sudo rpi-update

echo "Rebooting..."
sudo reboot
