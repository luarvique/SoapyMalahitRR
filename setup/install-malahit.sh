#!/bin/bash

# Add GPIO access library
sudo apt install gpiod
#sudo apt install libgpiod-dev

# Enable I2S audio
sudo cp asound.conf /etc
sudo cp alsa.conf /usr/share/alsa

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

# Let OpenWebRX access hardware
sudo usermod -a -G audio openwebrx
sudo usermod -a -G gpio openwebrx
sudo usermod -a -G spi openwebrx

# Copy Malahit-specific config file
sudo cp ./settings.json /var/lib/openwebrx

# Add OpenWebRX administrator account
sudo openwebrx admin adduser admin

# Update Linux and RPI firmware
sudo apt update
sudo apt full-upgrade
sudo rpi-update
sudo reboot
