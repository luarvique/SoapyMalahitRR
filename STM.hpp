#ifndef STM_HPP
#define STM_HPP

#include "SPI.hpp"
#include "GPIO.hpp"
#include <mutex>

class STM: public SPI
{
  public:
    static const unsigned int FIRMWARE_SIZE = 0x200000;
    static const unsigned int FIRMWARE_STEP = 0x800;

#ifdef ALLWINNER
    static const char *DEFAULT_SPI = "/dev/spidev1.0";
#else
    static const char *DEFAULT_SPI = "/dev/spidev0.0";
#endif

    STM(const char *deviceName = DEFAULT_SPI, unsigned int speed = 10000000)
    { open(deviceName, speed); }

    ~STM() { close(); }

    bool reset() const;
    bool go() const;

    bool update(unsigned int rate, unsigned int frequency, unsigned int switches, unsigned char attenuator = 0, unsigned char gain = 255);
    bool leds(unsigned char state) const;

    bool getStatus(float *voltage = 0, float *current = 0, char *charge = 0, char *charger = 0, char *id = 0, unsigned int *version = 0) const;
    float getVbat() const;
    bool isCharging() const;
    const char *getId();
    unsigned int getVersion() const;

    bool updateFirmware(const char *firmwareFile, bool force = false) const;
    bool getFirmware(const char *firmwareFile) const;

  private:
    typedef struct
    {
      unsigned char magic[6];   // 0
      unsigned char voltage[2]; // 6
      unsigned char current[2]; // 8
      unsigned char charging;   // 10
      unsigned char charge;     // 11
      unsigned char uid[12];    // 12
      unsigned char version[2]; // 24
      unsigned char batch[2];   // 26
      unsigned char crcok;      // 28
      unsigned char loader;     // 29
      unsigned char pad1[2];    // 30
    } STMState;

    typedef struct
    {
      unsigned char command;    // 0
      unsigned char freq[4];    // 1
      unsigned char switches;   // 5
      unsigned char attenuator; // 6
      unsigned char gain;       // 7
      unsigned char rate[2];    // 8
      unsigned char pad1[22];   // 10
    } STMControl;

    GPIO gpio;
    char id[32];

    mutable std::mutex mutex;

    bool send(unsigned char *data, unsigned int length) const;
    bool recv(unsigned char *data, unsigned int length) const;
    bool sendrecv(unsigned char *dataTx, unsigned char *dataRx, unsigned int length) const;
    bool sendrecv(unsigned char *dataTx, unsigned int lenTx, unsigned char *dataRx, unsigned int lenRx) const;
    unsigned short crc16(const unsigned char *data, unsigned int length) const;
    void printData(const char *label, const unsigned char *data, unsigned int length) const;
    bool fwWrite(const unsigned char *data, unsigned int addr, unsigned int length) const;
    bool fwRead(unsigned char *data, unsigned int addr, unsigned int length) const;
};

#endif // STM_HPP
