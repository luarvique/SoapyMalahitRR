#!/bin/bash

FWCONFIG=/boot/firmware/config.txt
UDEVRULES=/etc/udev/rules.d/60-malahit.rules

echo "### Getting latest package lists..."
sudo apt update

echo "### Installing OpenWebRX+..."
sudo apt install openwebrx

echo "### Installing Soapy Malahit driver..."
sudo apt install soapysdr-module-malahit-rr

echo "### Shutting down OpenWebRX..."
sudo systemctl stop openwebrx

echo "### Configuring ALSA for I2S audio..."
sudo install -o root -g root -m 644 asound.conf /etc
sudo install -o root -g root -m 644 alsa.conf /usr/share/alsa

echo "### Installing I2S audio device tree..."
sudo kdtc \
    ./generic_audio_out_i2s_slave.dts \
    /boot/firmware/overlays/generic_audio_out_i2s_slave.dtbo

echo "### Enabling I2S and SPI hardware..."
sudo touch $FWCONFIG

if ! grep -qxF "dtoverlay=generic_audio_out_i2s_slave" $FWCONFIG; then
    echo "dtoverlay=generic_audio_out_i2s_slave" | sudo tee -a $FWCONFIG
fi

if ! grep -qxF "dtparam=i2s=on" $FWCONFIG; then
    echo "dtparam=i2s=on" | sudo tee -a $FWCONFIG
fi

if ! grep -qxF "dtparam=spi=on" $FWCONFIG; then
    echo "dtparam=spi=on" | sudo tee -a $FWCONFIG
fi

if [ ! $(getent group gpio) ]; then
    echo "### Adding missing GPIO system group..."
    sudo addgroup --system gpio
    sudo chgrp gpio /dev/gpiochip*
    echo 'KERNEL=="gpiochip*", GROUP="gpio", MODE="0660"' | sudo tee -a $UDEVRULES
fi

if [ ! $(getent group spi) ]; then
    echo "### Adding missing SPI system group..."
    sudo addgroup --system spi
    sudo chgrp spi /dev/spidev*
    echo 'KERNEL=="spidev*", GROUP="spi", MODE="0660"' | sudo tee -a $UDEVRULES
fi

echo "### Letting OpenWebRX access networking, audio, GPIO, SPI..."
sudo usermod -aG netdev openwebrx
sudo usermod -aG audio openwebrx
sudo usermod -aG gpio openwebrx
sudo usermod -aG spi openwebrx
echo "### Groups for" `groups openwebrx`

echo "### Letting netdev group control Network Manager..."
PKLA=/etc/polkit-1/localauthority/50-local.d/org.freedesktop.NetworkManager.pkla
if [ ! -e $PKLA ]; then
    sudo install -o root -g root -m 644 netdev-nmcli.pkla $PKLA
fi

echo "### Installing Malahit-specific OpenWebRX config files..."
sudo install -o openwebrx -g openwebrx -m 644 ./settings.json /var/lib/openwebrx
sudo install -o openwebrx -g openwebrx -m 644 ./receiver_avatar.jpg /var/lib/openwebrx
sudo install -o openwebrx -g openwebrx -m 644 ./receiver_top_photo.jpg /var/lib/openwebrx
sudo install -o root -g root -m 644 ./openwebrx.conf /etc/openwebrx

echo "### Adding OpenWebRX administrator account..."
sudo openwebrx admin adduser admin

echo "### Upgrading Linux and RPI firmware..."
sudo apt full-upgrade
sudo rpi-update

echo "### Rebooting..."
sudo reboot
