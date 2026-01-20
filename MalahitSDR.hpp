#ifndef MALAHITSDR_HPP
#define MALAHITSDR_HPP

#include <SoapySDR/Device.hpp>

#include "ALSA.hpp"
#include "GPIO.hpp"
#include "STM.hpp"

class MalahitSDR : public SoapySDR::Device
{
  public:
    static const unsigned int SW_HIGHZ  = 0x0004;
    static const unsigned int SW_PREAMP = 0x0008;
    static const unsigned int SW_BIAST  = 0x0001;
    static const unsigned int SW_LOOP   = 0x0002;

    static const unsigned int LED_1     = 0x0001;
    static const unsigned int LED_2     = 0x0002;

    MalahitSDR();
    ~MalahitSDR();

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    size_t getNumChannels(const int direction) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;

    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const;

    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const;

    SoapySDR::Stream *setupStream(const int direction,
                                  const std::string &format,
                                  const std::vector<size_t> &channels = std::vector<size_t>(),
                                  const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(SoapySDR::Stream *stream,
                       const int flags = 0,
                       const long long timeNs = 0,
                       const size_t numElems = 0);

    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0);

    int readStream(SoapySDR::Stream *stream,
                   void * const *buffs,
                   const size_t numElems,
                   int &flags,
                   long long &timeNs,
                   const long timeoutUs = 200000);

    /*******************************************************************
     * Direct buffer access API
     ******************************************************************/

    size_t getNumDirectAccessBuffers(SoapySDR::Stream *stream);

    int getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs);

    int acquireReadBuffer(SoapySDR::Stream *stream,
                          size_t &handle,
                          const void **buffs,
                          int &flags,
                          long long &timeNs,
                          const long timeoutUs = 100000);

    void releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    bool hasDCOffsetMode(const int direction, const size_t channel) const;

    bool hasFrequencyCorrection(const int direction, const size_t channel) const;

    void setFrequencyCorrection(const int direction, const size_t channel, const double value);

    double getFrequencyCorrection(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    bool hasGainMode(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const double value);

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel) const;

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction,
                      const size_t channel,
                      const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void setFrequency(const int direction,
                      const size_t channel,
                      const std::string &name,
                      const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    double getFrequency(const int direction, const size_t channel) const;

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;


    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    /*******************************************************************
    * Bandwidth API
    ******************************************************************/

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;

    void setDCOffsetMode(const int direction, const size_t channel, const bool automatic);

    bool getDCOffsetMode(const int direction, const size_t channel) const;

    bool hasDCOffset(const int direction, const size_t channel) const;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    SoapySDR::ArgInfoList getSettingInfo(void) const;

    void writeSetting(const std::string &key, const std::string &value);

    std::string readSetting(const std::string &key) const;

  private:
    const char *statusPipeName = "/tmp/battery";
    const char *idPipeName = "/tmp/stm-id";
    const char *alsaDeviceName = "default";
    const unsigned int minFrequency = 150000;
    const unsigned int maxFrequency = 1766000000;

    const unsigned int chunkSize  = 1024;
    const unsigned int chunkCount = 4;

    ALSA alsaDevice;
      // I2S devices are accessed via ALSA API, encapsulated by this object.
    STM stmDevice;
      // Interface to the STM SoC.
    size_t statusCount = SIZE_MAX/2;
      // Count for battery status reporting.
    size_t ledCount = 0;
      // Count for LED blinking.
    unsigned int sampleRate = 650000;
      // Current sample rate in Hz.
    double curFrequency = 1000000.0;
      // Current frequency in Hz.
    double curFreqCorrection = 0.0;
      // Current frequency correction in PPM.
    unsigned int gain = 63;
      // Current gain level.
    unsigned int attenuator = 0;
      // Current attenuation level.
    unsigned int switches = 0;
      // Current GPIO switch states.
    unsigned int leds = LED_1;
      // Current LED states.

    bool updateRadio();
      // Send configuration to the radio chips

    bool reportBattery(size_t samples);
      // Report SW6106 status.
    bool blinkLEDs(size_t samples);
      // Blink LEDs.
};

#endif // MALAHITSDR_HPP
