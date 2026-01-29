#!/bin/bash

# Add GPIO access library
#sudo apt install gpiod
#sudo apt install libgpiod-dev

echo "Installing Soapy Malahit driver..."
sudo apt install soapysdr-module-malahit-rr

echo "Configuring ALSA for I2S audio..."
sudo install -o root asound.conf /etc
sudo install -o root alsa.conf /usr/share/alsa

echo "Installing I2S audio device tree..."
sudo kdtc \
    ./generic_audio_out_i2s_slave.dts \
    /boot/firmware/overlays/generic_audio_out_i2s_slave.dtbo

CONFIG=/boot/firmware/config.txt
sudo touch $CONFIG

if ! grep -q -F "dtoverlay=generic_audio_out_i2s_slave" $CONFIG; then
    sudo echo "dtoverlay=generic_audio_out_i2s_slave" >> $CONFIG
fi

if ! grep -q -F "dtparam=i2s=on" $CONFIG; then
    sudo echo "dtparam=i2s=on" >> $CONFIG
fi

echo "Letting OpenWebRX access networking, audio, GPIO, SPI..."
sudo usermod -a -G nedev openwebrx
sudo usermod -a -G audio openwebrx
sudo usermod -a -G gpio openwebrx
sudo usermod -a -G spi openwebrx
echo "Groups for" `groups openwebrx`

echo "# Installing Malahit-specific OpenWebRX config file..."
sudo install -o openwebrx ./settings.json /var/lib/openwebrx

echo "Adding OpenWebRX administrator account..."
sudo openwebrx admin adduser admin

echo "Updating Linux and RPI firmware..."
sudo apt update
sudo apt full-upgrade
sudo rpi-update

echo "Rebooting..."
sudo reboot
