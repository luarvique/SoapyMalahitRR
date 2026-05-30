#include "ALSA.hpp"
#include <stdio.h>

void ALSA::close()
{
  if(handle) { snd_pcm_close(handle);handle=0; }
}

bool ALSA::open(const char *deviceName, unsigned int rate, unsigned int bufferSize, unsigned int periodSize)
{
  snd_pcm_hw_params_t *hwParams;
  snd_pcm_sw_params_t *swParams;
  snd_output_t *log;
  snd_pcm_t *handle;
  int res;

  // Close currently open device
  close();

  // Do sanity check
  if((periodSize<=0) || (periodSize>=bufferSize)) periodSize = bufferSize/2;

  // Allocate ALSA data structures on stack
  snd_pcm_hw_params_alloca(&hwParams);
  snd_pcm_sw_params_alloca(&swParams);

  if((res = snd_output_stdio_attach(&log, stderr, 0)) < 0)
  {
    fprintf(stderr, "ALSA::open(): snd_output_stdio_attach() error: %s\n", snd_strerror(res));
    return(false);
  }

  // Open the device
  fprintf(stderr, "ALSA::open(): Opening ALSA device '%s'...", deviceName);
  if((res = snd_pcm_open(&handle, deviceName, SND_PCM_STREAM_CAPTURE, 0)) < 0)
  {
    fprintf(stderr, "ALSA::open(): snd_pcm_open() error: %s\n", snd_strerror(res));
    return(false);
  }

//  snd_pcm_info_t *info;
//  snd_pcm_info_alloca(&info);
//  if((res = snd_pcm_info(handle, info)) < 0)
//  {
//    fprintf(stderr, "ALSA::open(): snd_pcm_info() error: %s\n", snd_strerror(res));
//    snd_pcm_close(handle);
//    return(false);
//  }

  // Making device non-blocking (necessary ???)
//  if((res = snd_pcm_nonblock(handle, 1)) < 0)
//  {
//    fprintf(stderr, "snd_pcm_nonblock() error: %s\n", snd_strerror(res));
//    snd_pcm_close(handle);
//    return(false);
//  }

  //
  // Setting Hardware Parameters
  //

  if((res = snd_pcm_hw_params_any(handle, hwParams)) < 0)
  {
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_any() error: %s\n", snd_strerror(res));
    snd_pcm_close(handle);
    return(false);
  }

  // Using interleaved access
  res = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
  if(res<0) fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_access() error: %s\n", snd_strerror(res));

  // Using S16 little-endian samples
  res = snd_pcm_hw_params_set_format(handle, hwParams, SND_PCM_FORMAT_S16_LE);
  if(res<0) fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_format() error: %s\n", snd_strerror(res));

  // Using two channels for I/Q
  res = snd_pcm_hw_params_set_channels(handle, hwParams, 2);
  if(res<0) fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_channels() error: %s\n", snd_strerror(res));

  // Try using requested rate
  this->rate = rate;
  res = snd_pcm_hw_params_set_rate_near(handle, hwParams, &this->rate, 0);
  if(res<0)
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_rate_near() error: %s\n", snd_strerror(res));
  else if(this->rate!=rate)
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_rate_near() returned %u <> %u\n", this->rate, rate);

  this->periodSize = periodSize;
  res = snd_pcm_hw_params_set_period_size_near(handle, hwParams,  &this->periodSize, 0);
  if(res<0)
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_period_size_near() error: %s\n", snd_strerror(res));
  else if(this->periodSize!=periodSize)
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_period_size_near() returned %lu <> %u\n", this->periodSize, periodSize);

  this->bufferSize = bufferSize;
  res = snd_pcm_hw_params_set_buffer_size_near(handle, hwParams,  &this->bufferSize);
  if(res<0)
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_buffer_size_near() error: %s\n", snd_strerror(res));
  else if(this->bufferSize!=bufferSize)
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params_set_buffer_size_near() returned %lu <> %u\n", this->bufferSize, bufferSize);

  // Put hardware parameters into effect
  if((res = snd_pcm_hw_params(handle, hwParams)) < 0)
  {
    fprintf(stderr, "ALSA::open(): snd_pcm_hw_params() error: %s\n", snd_strerror(res));
    snd_pcm_hw_params_dump(hwParams, log);
    snd_pcm_close(handle);
    return(false);
  }

  // Check period and buffer sizes for sanity
  snd_pcm_hw_params_get_period_size(hwParams, &this->periodSize, 0);
  snd_pcm_hw_params_get_buffer_size(hwParams, &this->bufferSize);
  if(this->periodSize>=this->bufferSize)
    fprintf(stderr, "ALSA::open(): Bad period size %lu >= buffer size %lu\n", this->periodSize, this->bufferSize);

  //
  // Setting Software Parameters
  //

  if((res = snd_pcm_sw_params_current(handle, swParams)) < 0)
  {
    fprintf(stderr, "ALSA::open(): snd_pcm_sw_params_current() error: %s\n", snd_strerror(res));
    snd_pcm_close(handle);
    return(false);
  }

  res = snd_pcm_sw_params_set_avail_min(handle, swParams, this->periodSize);
  if(res<0) fprintf(stderr, "ALSA::open(): snd_pcm_sw_params_set_avail_min() error: %s\n", snd_strerror(res));

  res = snd_pcm_sw_params_set_start_threshold(handle, swParams, this->bufferSize);
  if(res<0) fprintf(stderr, "ALSA::open(): snd_pcm_sw_params_set_start_threshold() error: %s\n", snd_strerror(res));

  res = snd_pcm_sw_params_set_stop_threshold(handle, swParams, this->bufferSize);
  if(res<0) fprintf(stderr, "ALSA::open(): snd_pcm_sw_params_set_stop_threshold() error: %s\n", snd_strerror(res));

  if((res = snd_pcm_sw_params(handle, swParams)) < 0)
  {
    fprintf(stderr, "ALSA::open(): snd_pcm_sw_params() error: %s\n", snd_strerror(res));
    snd_pcm_sw_params_dump(swParams, log);
    snd_pcm_close(handle);
    return(false);
  }

  // Done initializing audio device
  snd_pcm_dump(handle, log);
  this->handle = handle;
  return(true);
}

unsigned int ALSA::read(void *data, unsigned int samples)
{
  unsigned int count;
  int res;

  // Stream must be open, number of samples must be valid
  if(!handle || (samples<periodSize)) return(0);

  // Truncate to period size
  samples -= samples % periodSize;

  // Read data
  for(res=0, count=0 ; (res>=0) && (count<samples) ; count+=res>0? res:0)
  {
    res = snd_pcm_readi(handle, data, samples);
    if((res==-EAGAIN) || (res==0))
    {
      snd_pcm_wait(handle, 1000);
      res = 0;
    }
    else if(res==-EPIPE)
    {
      res = snd_pcm_prepare(handle);
    }
    else if(res==-ESTRPIPE)
    {
      // Wait until suspend flag is released
      while((res=snd_pcm_resume(handle))==-EAGAIN) sleep(1);
      res = snd_pcm_prepare(handle);
    }

    if(res<0) fprintf(stderr, "ALSA::read(): %s (%d)\n", snd_strerror(res), res);
  }

//  fprintf(stderr, "@@@ Reading %d (/%lu) frames to %p => got %d\n", samples, periodSize, data, count);

  // Done
  return(count);
}
