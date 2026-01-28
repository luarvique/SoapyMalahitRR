#include "MalahitSDR.hpp"
#include <SoapySDR/Registry.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>

static const unsigned int sampleRates[] =
{
  650000, 744192, 912000, 0
};

MalahitSDR::MalahitSDR()
{
  // Hard-reset attached hardware
  stmDevice.reset();
  // Check firmware and update as necessary
  stmDevice.updateFirmware("/usr/share/malahit/" CURRENT_FIRMWARE);
  // Start STM receiver
  stmDevice.go();
  // Update hardware with initial settings
  updateRadio();
}

MalahitSDR::~MalahitSDR()
{
  // Close audio device
  alsaDevice.close();
}

bool MalahitSDR::blinkLEDs(size_t samples)
{
  // Do not blink until accumulated enough time (1 second)
  ledCount += samples;
  if(ledCount<sampleRate*10) return(true);
  ledCount = 0;

  // Invert leds for now
  leds ^= LED_1;
  return(stmDevice.leds(leds));
}

bool MalahitSDR::reportBattery(size_t samples)
{
  float voltage;
  float current;
  char charge;
  bool charger;
  FILE *f;

  // Do not report until accumulated enough time (10+ seconds)
  statusCount += samples;
  if(statusCount<sampleRate*10) return(true);
  statusCount = 0;

  // Get STM status
  char id[32], ch;
  unsigned int ver;
  if(!stmDevice.getStatus(&voltage, &current, &charge, &ch, id, &ver)) return(false);

  // Determine charger status
  charger = ch!='\0';

  // Light up a LED when the charge is too low
  leds = (leds ^ ~LED_2) | (!charger && (charge < 15)? LED_2:0);

  // Save STM chip ID and firmware version to a file
  f = fopen(idPipeName, "wb");
  if(f)
  {
    fprintf(f, "%s %.2f\n", id, ver / 100.0f);
    fclose(f);
  }

  // This file will be used to report battery status
  f = fopen(statusPipeName, "wb");
  if(!f) return(false);

  // Report battery status
  bool result = fprintf(f, "%.2fV%s %.2fA %d%%\n", voltage, charger? "!":"", current, charge) > 0;
  fclose(f);

  // Done
  return(result);
}

bool MalahitSDR::updateRadio()
{
  // Apply frequency correction
  unsigned int frequency = curFrequency * (1.0 + curFreqCorrection / 1000000.0);

  fprintf(stderr, "updateRadio(): Rate=%dHz, Freq=%dHz, SW=0x%X, ATT=%d\n",
    sampleRate, frequency, switches, attenuator
  );

  return(stmDevice.update(sampleRate, frequency, switches, attenuator, gain));
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string MalahitSDR::getDriverKey(void) const
{
  return("Malahit");
}

std::string MalahitSDR::getHardwareKey(void) const
{
  return("R1");
}

SoapySDR::Kwargs MalahitSDR::getHardwareInfo(void) const
{
  SoapySDR::Kwargs result;

  // @@@ TODO!

  return(result);
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t MalahitSDR::getNumChannels(const int direction) const
{
  // We only support one RX channel
  return(direction? 1:0);
}

/*******************************************************************
 * Stream API
 ******************************************************************/

std::vector<std::string> MalahitSDR::getStreamFormats(const int direction, const size_t channel) const
{
  std::vector<std::string> result;

  // We only support one channel, with CS16 data
  if(direction!=0 && channel==0) result.push_back("CS16");

  return(result);
}

std::string MalahitSDR::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
  // We only support CS16 data format
  return("CS16");
}

SoapySDR::ArgInfoList MalahitSDR::getStreamArgsInfo(const int direction, const size_t channel) const
{
  SoapySDR::ArgInfoList result;
  return(result);
}

SoapySDR::Stream *MalahitSDR::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args)
{
  // We only have one RX channel
  if((direction==0) || (channels.size()>1) || ((channels.size()>0) && (channels.at(0)>0)))
    throw std::runtime_error("setupStream invalid channel selection");

  // We only support CS16 data format
  if(format!="CS16")
    throw std::runtime_error("setupStream invalid format '" + format + "'");

  // Return our ALSA device (may not be open yet)
  return(reinterpret_cast<SoapySDR::Stream *>(&alsaDevice));
}

void MalahitSDR::closeStream(SoapySDR::Stream *stream)
{
  std::lock_guard <std::mutex> lock(mutex);

  // Close ALSA device
  (reinterpret_cast<ALSA *>(stream))->close();
}

size_t MalahitSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
  // Assuming that MTU is essentially a chunk
  return(chunkSize);
}

int MalahitSDR::activateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs, const size_t numElems)
{
  std::lock_guard <std::mutex> lock(mutex);

  // Open ALSA device
  ALSA *device = reinterpret_cast<ALSA *>(stream);
  return(device->open(alsaDeviceName, sampleRate, chunkCount * chunkSize, chunkSize)? 0 : -1);
}

int MalahitSDR::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
  std::lock_guard <std::mutex> lock(mutex);

  // Close ALSA device
  ALSA *device = reinterpret_cast<ALSA *>(stream);
  device->close();
  return(0);
}

int MalahitSDR::readStream(SoapySDR::Stream *stream, void * const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs)
{
  std::lock_guard <std::mutex> lock(mutex);

  // Report SW6106 status
  reportBattery(numElems);

  // Blink LEDs
  blinkLEDs(numElems);

  // Read data from the ALSA device
  ALSA *device = reinterpret_cast<ALSA *>(stream);
  return(device->read(buffs[0], numElems/16));
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t MalahitSDR::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
  // No direct access
  return(0);
}

int MalahitSDR::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
  // No direct access
  return(-1);
}

int MalahitSDR::acquireReadBuffer(SoapySDR::Stream *stream, size_t &handle, const void **buffs, int &flags, long long &timeNs, const long timeoutUs)
{
  // No direct access
  return(-1);
}

void MalahitSDR::releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle)
{
  // No direct access
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> MalahitSDR::listAntennas(const int direction, const size_t channel) const
{
  std::vector<std::string> result;
  result.push_back("Regular");
  result.push_back("Loop");
  return(result);
}

void MalahitSDR::setAntenna(const int direction, const size_t channel, const std::string &name)
{
  bool loop = name == "Loop";

  if(loop != !!(switches & SW_LOOP))
  {
    switches = (switches & ~SW_LOOP) | (loop? SW_LOOP : 0);
    updateRadio();
  }
}

std::string MalahitSDR::getAntenna(const int direction, const size_t channel) const
{
  return(!!(switches & SW_LOOP)? "Loop" : "Regular");
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool MalahitSDR::hasDCOffsetMode(const int direction, const size_t channel) const
{
  // No DC offset yet
  return(false);
}

bool MalahitSDR::hasFrequencyCorrection(const int direction, const size_t channel) const
{
  return(true);
}

void MalahitSDR::setFrequencyCorrection(const int direction, const size_t channel, const double value)
{
  if(value != curFreqCorrection)
  {
    curFreqCorrection = value;
    updateRadio();
  }
}

double MalahitSDR::getFrequencyCorrection(const int direction, const size_t channel) const
{
  return(curFreqCorrection);
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> MalahitSDR::listGains(const int direction, const size_t channel) const
{
  std::vector<std::string> results;
  results.push_back("MAIN");
  return(results);
}

bool MalahitSDR::hasGainMode(const int direction, const size_t channel) const
{
  // No gain mode
  return(false);
}

void MalahitSDR::setGainMode(const int direction, const size_t channel, const bool automatic)
{
  // No gain control
}

bool MalahitSDR::getGainMode(const int direction, const size_t channel) const
{
  // No gain control
  return(false);
}

void MalahitSDR::setGain(const int direction, const size_t channel, const double value)
{
  setGain(direction, channel, "MAIN", value);
}

void MalahitSDR::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
   SoapySDR::Range range = getGainRange(direction, channel, name);
   unsigned int v = round(
     value<range.minimum()? range.minimum()
   : value>range.maximum()? range.maximum()
   : value);

  if((name=="MAIN") && (v!=gain))
  {
    gain = v;
    updateRadio();
  }
}

double MalahitSDR::getGain(const int direction, const size_t channel) const
{
  return(getGain(direction, channel, "MAIN"));
}

double MalahitSDR::getGain(const int direction, const size_t channel, const std::string &name) const
{
  return(name=="MAIN"? (double)gain : 0.0);
}

SoapySDR::Range MalahitSDR::getGainRange(const int direction, const size_t channel) const
{
  return(getGainRange(direction, channel, "MAIN"));
}

SoapySDR::Range MalahitSDR::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
  return(name=="MAIN"? SoapySDR::Range(0.0, 63.0) : SoapySDR::Range(0.0, 0.0));
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void MalahitSDR::setFrequency(const int direction, const size_t channel, const double frequency, const SoapySDR::Kwargs &args)
{
  setFrequency(direction, channel, "MAIN", frequency, args);
}

void MalahitSDR::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
  // If frequency changes...
  if(frequency != curFrequency)
  {
    // New frequency now in effect
    curFrequency = frequency;
    updateRadio();
  }
}

double MalahitSDR::getFrequency(const int direction, const size_t channel) const
{
  return(getFrequency(direction, channel, "MAIN"));
}

double MalahitSDR::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
  return(curFrequency);
}

SoapySDR::RangeList MalahitSDR::getBandwidthRange(const int direction, const size_t channel) const
{
  SoapySDR::RangeList result;

  // Call into the older deprecated listBandwidths() call
  for(auto &bw: this->listBandwidths(direction, channel))
    result.push_back(SoapySDR::Range(bw, bw));

  return(result);
}

std::vector<std::string> MalahitSDR::listFrequencies(const int direction, const size_t channel) const
{
  std::vector<std::string> result;
  result.push_back("MAIN");
  return(result);
}

SoapySDR::RangeList MalahitSDR::getFrequencyRange(const int direction, const size_t channel) const
{
  return(getFrequencyRange(direction, channel, "MAIN"));
}

SoapySDR::RangeList MalahitSDR::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
  SoapySDR::RangeList result;

  if(name=="MAIN") result.push_back(SoapySDR::Range(minFrequency, maxFrequency));

  return(result);
}

SoapySDR::ArgInfoList MalahitSDR::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
  // No frequency args yet
  SoapySDR::ArgInfoList result;
  return(result);
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void MalahitSDR::setSampleRate(const int direction, const size_t channel, const double rate)
{
  unsigned int newRate = (unsigned int)rate;
  int j;

  for(j = 0 ; sampleRates[j] ; ++j)
    if(newRate == sampleRates[j]) break;

  // If given rate valid...
  if(sampleRates[j] && (newRate!=sampleRate))
  {
    std::lock_guard <std::mutex> lock(mutex);

    fprintf(stderr, "setSampleRate(%d): Setting new rate...\n", newRate);

    // Close ALSA device while changing sample rate
    bool needReopen = alsaDevice.isOpen();
    if(needReopen) alsaDevice.close();

    // Change sample rate
    sampleRate = newRate;
    updateRadio();

    // Reopen ALSA device
    if(needReopen)
      alsaDevice.open(alsaDeviceName, sampleRate, chunkCount * chunkSize, chunkSize);

    fprintf(stderr, "setSampleRate(%d): DONE!\n", newRate);
  }
}

double MalahitSDR::getSampleRate(const int direction, const size_t channel) const
{
  return(sampleRate);
}

std::vector<double> MalahitSDR::listSampleRates(const int direction, const size_t channel) const
{
  std::vector<double> result;
  for(int j = 0 ; sampleRates[j] ; ++j)
    result.push_back(sampleRates[j]);
  return(result);
}

/*******************************************************************
 * Bandwidth API
 ******************************************************************/

void MalahitSDR::setBandwidth(const int direction, const size_t channel, const double bw)
{
  // Same as sample rates
  setSampleRate(direction, channel, bw);
}

double MalahitSDR::getBandwidth(const int direction, const size_t channel) const
{
  // Same as sample rates
  return(getSampleRate(direction, channel));
}

std::vector<double> MalahitSDR::listBandwidths(const int direction, const size_t channel) const
{
  // Same as sample rates
  return(listSampleRates(direction, channel));
}

void MalahitSDR::setDCOffsetMode(const int direction, const size_t channel, const bool automatic)
{
  // No DC offset yet
}

bool MalahitSDR::getDCOffsetMode(const int direction, const size_t channel) const
{
  // No DC offset yet
  return(false);
}

bool MalahitSDR::hasDCOffset(const int direction, const size_t channel) const
{
  // No DC offset yet
  return(false);
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList MalahitSDR::getSettingInfo(void) const
{
  SoapySDR::ArgInfoList result;

  {
    SoapySDR::ArgInfo info;
    info.key = "biasT";
    info.value = "false";
    info.name = "BiasT enable";
    info.description = "BiasT control.";
    info.type = SoapySDR::ArgInfo::BOOL;
    result.push_back(info);
  }

  {
    SoapySDR::ArgInfo info;
    info.key = "lna";
    info.value = "false";
    info.name = "LNA enable";
    info.description = "LNA control.";
    info.type = SoapySDR::ArgInfo::BOOL;
    result.push_back(info);
  }

  {
    SoapySDR::ArgInfo info;
    info.key = "highZ";
    info.value = "false";
    info.name = "HighZ enable";
    info.description = "HighZ input control.";
    info.type = SoapySDR::ArgInfo::BOOL;
    result.push_back(info);
  }

  {
    SoapySDR::ArgInfo info;
    info.key = "attenuator";
    info.value = "false";
    info.name = "Attenuation level";
    info.description = "Attenuator control.";
    info.type = SoapySDR::ArgInfo::INT;
    result.push_back(info);
  }

  {
    SoapySDR::ArgInfo info;
    info.key = "charger";
    info.value = "false";
    info.name = "Charger status";
    info.description = "Battery charger status.";
    info.type = SoapySDR::ArgInfo::BOOL;
    result.push_back(info);
  }

  {
    SoapySDR::ArgInfo info;
    info.key = "voltage";
    info.value = "0.0";
    info.name = "Battery voltage";
    info.description = "Battery voltage in volts.";
    info.type = SoapySDR::ArgInfo::FLOAT;
    result.push_back(info);
  }

  return(result);
}

void MalahitSDR::writeSetting(const std::string &key, const std::string &value)
{
  fprintf(stderr, "writeSetting('%s', '%s')\n", key.c_str(), value.c_str());

  if(key=="biasT" && !!(switches & SW_BIAST)!=(value=="true"))
  {
    switches = (switches & ~SW_BIAST) | (value=="true"? SW_BIAST : 0);
    updateRadio();
  }

  if(key=="highZ" && !!(switches & SW_HIGHZ)!=(value=="true"))
  {
    switches = (switches & ~SW_HIGHZ) | (value=="true"? SW_HIGHZ : 0);
    updateRadio();
  }

  if(key=="lna" && !!(switches & SW_PREAMP)!=(value=="true"))
  {
    switches = (switches & ~SW_PREAMP) | (value=="true"? SW_PREAMP : 0);
    updateRadio();
  }

  if(key=="attenuator" && (unsigned int)stoi(value)!=attenuator)
  {
    attenuator = std::max(0, std::min(30, stoi(value)));
    updateRadio();
  }
}

std::string MalahitSDR::readSetting(const std::string &key) const
{
  if(key=="biasT")       return std::to_string(!!(switches & SW_BIAST));
  if(key=="highZ")       return std::to_string(!!(switches & SW_HIGHZ));
  if(key=="lna")         return std::to_string(!!(switches & SW_PREAMP));
  if(key=="attenuator")  return std::to_string(attenuator);
  if(key=="voltage")     return std::to_string(stmDevice.getVbat());
  if(key=="charger")     return std::to_string(stmDevice.isCharging());

  return "";
}

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findMalahitSDR(const SoapySDR::Kwargs &args)
{
    (void)args;
    //locate the device on the system...
    //return a list of 0, 1, or more argument maps that each identify a device

    return(SoapySDR::KwargsList());
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeMalahitSDR(const SoapySDR::Kwargs &args)
{
    (void)args;
    //create an instance of the device object given the args
    //here we will translate args into something used in the constructor
    return(new MalahitSDR());
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerMalahitSDR("malahitrr", &findMalahitSDR, &makeMalahitSDR, SOAPY_SDR_ABI_VERSION);
