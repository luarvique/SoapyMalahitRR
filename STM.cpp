#include "STM.hpp"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

bool STM::reset() const
{
  // Hard-reset STM chip
  gpio.reset();

  // Wait for STM to become ready
  return(gpio.waitForSTM());
}

bool STM::go() const
{
  unsigned char buf[32] = "GO!";

  // Send request
  bool result = send(buf, sizeof(buf));
  if(!result)
    fprintf(stderr, "STM::go(): Failed starting up the STM!\n");

  // Let STM start up
  usleep(1000000);

  // Get STM status for the first time
  result = getStatus();
  if(!result)
    fprintf(stderr, "STM::go(): Failed to get STM status!\n");

  return(result);
}

void STM::printData(const char *label, const unsigned char *data, unsigned int length) const
{
  fprintf(stderr, "=== %s ===\n", label);
  for(unsigned int j=0 ; j<length ; j+=16)
  {
    for(unsigned int i=0 ; (i<16) && (j+i<length) ; i++)
      fprintf(stderr, "%02X ", data[j+i]);
    for(unsigned int i=0 ; (i<16) && (j+i<length) ; i++)
    {
      unsigned char c = data[j+i];
      fprintf(stderr, "%c", (c>=' ') && (c<0x7F)? c:'.');
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "================================================================\n");
}

bool STM::getStatus(float *voltage, float *current, char *charge, char *charger, char *id, unsigned int *version) const
{
  std::lock_guard <std::mutex> lock(mutex);
  STMState st;

  // Receive status from the STM
  bool result = recv((unsigned char *)&st, sizeof(st));

//  printData("STATUS", (const unsigned char *)&st, sizeof(st));

  // Check if STM returned valid status
  if(!result || memcmp(st.magic, "Status", 6)) return(false);

  // Get current battery voltage / current / charge / charger status
  if(voltage) *voltage = (st.voltage[0] * 256 + st.voltage[1]) / 1000.0f;
  if(current) *current = (st.current[0] * 256 + st.current[1]) / 1000.0f;
  if(charge)  *charge  = st.charge;
  if(charger) *charger = st.charging=='C'? 'C' : '\0';

  // Get firmware version (0xFFFF = no firmware)
  if(version)
  {
    *version = st.version[0] * 256 + st.version[1];
    if(*version == 0xFFFF) *version = 0x0000;
  }

  // Get STM chip ID
  if(id)
  {
    sprintf(id, "%04X-%04X-%04X-%04X-%04X-%04X",
      st.uid[0] * 256 + st.uid[1],
      st.uid[2] * 256 + st.uid[3],
      st.uid[4] * 256 + st.uid[5],
      st.uid[6] * 256 + st.uid[7],
      st.uid[8] * 256 + st.uid[9],
      st.uid[10] * 256 + st.uid[11]
    );
  }

  // Status OK
  return(true);
}

float STM::getVbat() const
{
  float result;
  return(getStatus(&result)? result : 0.0);
}

bool STM::isCharging() const
{
  char result;
  return(getStatus(0, 0, 0, &result) && result);
}

const char *STM::getId()
{
  return(getStatus(0, 0, 0, 0, id)? id : 0);
}

unsigned int STM::getVersion() const
{
  unsigned int version;
  return(getStatus(0, 0, 0, 0, 0, &version)? version : 0);
}

bool STM::send(unsigned char *data, unsigned int length) const
{
  return(sendrecv(data, 0, length));
}

bool STM::recv(unsigned char *data, unsigned int length) const
{
  return(sendrecv(0, data, length));
}

bool STM::sendrecv(unsigned char *dataTx, unsigned char *dataRx, unsigned int length) const
{
  return(sendrecv(dataTx, length, dataRx, length));
}

bool STM::sendrecv(unsigned char *dataTx, unsigned int lenTx, unsigned char *dataRx, unsigned int lenRx) const
{
  // Wait for the STM
  if(!gpio.waitForSTM()) return(false);

  // Add CRC
  if(dataTx)
  {
    unsigned short crc = crc16(dataTx, lenTx-2);
    dataTx[lenTx - 2] = crc >> 8;
    dataTx[lenTx - 1] = crc & 0xFF;
  }

  // Send and receive data
  if(!SPI::sendrecv(dataTx, lenTx, dataRx, lenRx)) return(false);

#if 1
  // Check CRC
  if(dataRx)
  {
    unsigned short crc = ((unsigned short)dataRx[lenRx - 2] << 8) + dataRx[lenRx - 1];
    if(crc != crc16(dataRx, lenRx-2)) {
fprintf(stderr, "CRC FOUND 0x%04X, COMPUTED 0x%04X, LENGTH %d\n", crc, crc16(dataRx, lenRx-2), lenRx);
//return(false);
}
  }
#endif

  // Done
  return(true);
}

bool STM::leds(unsigned char state) const
{
  std::lock_guard <std::mutex> lock(mutex);
  unsigned char buf[32];

  buf[0] = 'L';
  buf[1] = state;

  bool result = send(buf, sizeof(buf));
  if(!result)
    fprintf(stderr, "STM::leds(): Failed communicating with STM!\n");

  return(result);
}

bool STM::update(unsigned int rate, unsigned int frequency, unsigned int switches, unsigned char attenuator, unsigned char gain)
{
  std::lock_guard <std::mutex> lock(mutex);
  STMControl cmd;

  // Rate in kHz
  rate /= 1000;
  if((rate!=650) && (rate!=744) && (rate!=912))
  {
    fprintf(stderr, "STM::update(): Invalid sampling rate of %dkHz!\n", rate);
    return(false);
  }

  // Compose request
  cmd.command  = 'S';
  cmd.freq[0]  = (frequency >> 24) & 0xFF;
  cmd.freq[1]  = (frequency >> 16) & 0xFF;
  cmd.freq[2]  = (frequency >> 8) & 0xFF;
  cmd.freq[3]  = frequency & 0xFF;
  cmd.switches = switches;
  cmd.attenuator = attenuator;
  cmd.gain     = gain;
  cmd.rate[0]  = (rate >> 8) & 0xFF;
  cmd.rate[1]  = rate & 0xFF;

  // Send request
  bool result = send((unsigned char *)&cmd, sizeof(cmd));
  if(!result)
    fprintf(stderr, "STM::update(): Failed communicating with STM!\n");

  // Let STM handle all the changes
  usleep(1000);
  return(result);
}

unsigned short STM::crc16(const unsigned char *data, unsigned int length) const
{
  unsigned short crc;
  unsigned int i, j;

  for(i = 0, crc = 0xFFFF; i < length; i++)
    for (j = 0, crc ^= data[i]; j < 8; j++)
      crc = (crc >> 1) ^ (crc&1? 0xA001 : 0);

  return crc;
}

bool STM::fwWrite(const unsigned char *data, unsigned int addr, unsigned int length) const
{
  // Check address and length
  if((length>0x4000) || (addr+length>FIRMWARE_SIZE)) return(false);

  // Compose command
  unsigned char buf[2 + 4 + 2 + length + 2];
  buf[0] = 'F';
  buf[1] = 'W';
  buf[2] = (addr >> 24) & 0xFF;
  buf[3] = (addr >> 16) & 0xFF;
  buf[4] = (addr >> 8) & 0xFF;
  buf[5] = addr & 0xFF;
  buf[6] = (length >> 8) & 0xFF;
  buf[7] = length & 0xFF;

  // Add data
  memcpy(buf + 2 + 4 + 2, data, length);

  // Send command and data
  return(send(buf, sizeof(buf)));
}

bool STM::fwRead(unsigned char *data, unsigned int addr, unsigned int length) const
{
  // Check address and length
  if((length>0x4000) || (addr+length>FIRMWARE_SIZE)) return(false);

  // Compose command
  unsigned char buf[32];
  unsigned char rsp[2 + 4 + 2 + length + 2];
  buf[0] = 'F';
  buf[1] = 'R';
  buf[2] = (addr >> 24) & 0xFF;
  buf[3] = (addr >> 16) & 0xFF;
  buf[4] = (addr >> 8) & 0xFF;
  buf[5] = addr & 0xFF;
  buf[6] = (length >> 8) & 0xFF;
  buf[7] = length & 0xFF;

  // Send command
  if(!send(buf, sizeof(buf))) return(false);

  // Receive response
  if(!recv(rsp, sizeof(rsp))) return(false);

  // Copy read firmware data
  memcpy(data, rsp + 2 + 4 + 2, length);
  return(true);
}

bool STM::getFirmware(const char *firmwareFile) const
{
  unsigned char buf[FIRMWARE_STEP];
  unsigned int j;

  FILE *F = fopen(firmwareFile, "wb");
  if(!F) return(false);

  for(j=0 ; j<FIRMWARE_SIZE ; j+=sizeof(buf))
  {
    fprintf(stderr, "STM::getFirmware('%s'): Reading %ldkB from 0x%X...\n", firmwareFile, sizeof(buf)>>10, j);
    if(!fwRead(buf, j, sizeof(buf))) break;
    if(fwrite(buf, 1, sizeof(buf), F) != sizeof(buf)) break;
  }

  fclose(F);
  return(j==FIRMWARE_SIZE);
}

bool STM::updateFirmware(const char *firmwareFile, bool force) const
{
  // Assume no firmware for now
  unsigned int oldVersion = 0;
  unsigned int newVersion = 0;
  unsigned int j;
  const char *p;

  // Get current firmware version
  oldVersion = getVersion();
  if(!oldVersion)
    fprintf(stderr, "STM::updateFirmware('%s'): Failed obtaining current version!\n", firmwareFile);

  // Obtain new firmware version from the filename
  p = strrchr(firmwareFile, '/');
  p = p? p+1 : firmwareFile;
  if(p && (sscanf(p, "malahit-r1-fw-%u.bin", &newVersion)!=1)) newVersion = 0;

  // If updating firmware...
  if((newVersion > oldVersion) || force)
  {
    unsigned char buf[FIRMWARE_STEP];

    fprintf(stderr, "STM::updateFirmware('%s'): Updating firmware %03u => %03u...\n", firmwareFile, oldVersion, newVersion);

    FILE *F = fopen(firmwareFile, "rb");
    if(!F)
    {
      fprintf(stderr, "STM::updateFirmware('%s'): Failed opening file!\n", firmwareFile);
      return(false);
    }

    // Write data from file into STM
    for(j=0 ; j<FIRMWARE_SIZE ; j+=sizeof(buf))
    {
      fprintf(stderr, "STM::updateFirmware('%s'): Writing %ldkB to 0x%X...\n", firmwareFile, sizeof(buf)>>10, j);
      if(fread(buf, 1, sizeof(buf), F) != sizeof(buf)) break;
      if(!fwWrite(buf, j, sizeof(buf))) break;
    }

    // Done with the file
    fclose(F);

    // Check how much data has been written
    if(j!=FIRMWARE_SIZE)
      fprintf(stderr, "STM::updateFirmware('%s'): Failed writing firmware (%dkB/%dkB)...\n", firmwareFile, j>>10, FIRMWARE_SIZE>>10);

    if(!gpio.waitForSTM())
      fprintf(stderr, "STM::updateFirmware('%s'): Not ready after update!\n", firmwareFile);

    // Hard-reset STM device
    if(!reset())
      fprintf(stderr, "STM::updateFirmware('%s'): Failed to reset after update!\n", firmwareFile);

    // Check the updated firmware version
    oldVersion = getVersion();
    if(oldVersion != newVersion)
    {
      fprintf(stderr, "STM::updateFirmware('%s'): Failed updating firmware, still at version %03u!\n", firmwareFile, oldVersion);
      return(false);
    }

    fprintf(stderr, "STM::updateFirmware('%s'): Updated %dkB, version is %03u.\n", firmwareFile, j>>10, oldVersion);
  }

  // We are ok
  return(true);
}
