#include "GPIO.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define GPIOD_RST_LINE     25
#define GPIOD_BUSY_LINE     5

void GPIO::uninitialize()
{
  if(chip)
  {
    gpiod_chip_close(chip);
    chip = 0;
  }
}

bool GPIO::waitForSTM() const
{
  // Must have GPIO lines
  if(!chip) return(false);

  // Wait for STM chip to become ready
  for(int j = 0 ; j < 10000 ; j++)
    if(gpiod_line_get_value(busy_line)) usleep(1000); else return(true);

  // Timeout
  return(false);
}

bool GPIO::initialize(const char *chipName)
{
  // Uninitialize first
  uninitialize();

  fprintf(stderr, "GPIO::initialize(): Opening '%s'...\n", chipName);

  chip = gpiod_chip_open_by_name(chipName);
  if(!chip)
  {
    fprintf(stderr, "GPIO::initialize(): Failed opening GPIO chip\n");
    return(false);
  }

  fprintf(stderr, "GPIO::initialize(): Requesting GPIO lines...\n");

  rst_line = gpiod_chip_get_line(chip, GPIOD_RST_LINE);
  if(!rst_line)
  {
    fprintf(stderr, "GPIO::initialize(): Failed obtaining RST GPIO line\n");
    uninitialize();
    return(false);
  }

  if(gpiod_line_request_output(rst_line, "master", 0) < 0)
  {
    fprintf(stderr, "GPIO::initialize(): Failed setting RST GPIO line as output\n");
    uninitialize();
    return(false);
  }

  busy_line = gpiod_chip_get_line(chip, GPIOD_BUSY_LINE);
  if(!busy_line)
  {
    fprintf(stderr, "GPIO::initialize(): Failed obtaining BUSY GPIO line\n");
    uninitialize();
    return(false);
  }

  if(gpiod_line_request_input(busy_line, "master") < 0)
  {
    fprintf(stderr, "GPIO::initialize(): Failed setting BUSY GPIO line as input\n");
    uninitialize();
    return(false);
  }

  // Success
  fprintf(stderr, "GPIO::initialize(): DONE!\n");
  return(true);
}

bool GPIO::reset() const
{
  // Must have GPIO lines
  if(!chip)
  {
    fprintf(stderr, "GPIO::reset(): GPIO has not been initialized!\n");
    return(false);
  }

  fprintf(stderr, "GPIO::reset(): Resetting radio ...\n");

  // Keep RST high for 50msec
  gpiod_line_set_value(rst_line, 1);
  usleep(50000);

  // Keep RST low for 50msec
  gpiod_line_set_value(rst_line, 0);
  usleep(50000);

  // Leave RST line high
  gpiod_line_set_value(rst_line, 1);
  usleep(15000);

  return(true);
}
