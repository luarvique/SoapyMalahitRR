#include "STM.hpp"
#include "GPIO.hpp"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

STM stmDevice;

int main(int argc, char *argv[])
{
  // Hard-reset STM chip
  if(!stmDevice.reset())
  {
    fprintf(stderr, "%s: Failed to reset STM device\n", argv[0]);
    printf("INIT-ERROR\n");
    return(1);
  }

  // If parameters given...
  if((argc>=3) && (strlen(argv[1])==2) && (argv[1][0]=='-'))
  {
    switch(argv[1][1])
    {
      case 'r': // Read firmware
        if(!stmDevice.getFirmware(argv[2]))
        {
          fprintf(stderr, "%s: Failed to download STM firmware\n", argv[0]);
          printf("FW-ERROR\n");
          return(3);
        }
        break;

      case 'w': // Update firmware
      case 'f': // Force firmware update
        if(!stmDevice.updateFirmware(argv[2], argv[1][1]=='f'))
        {
          fprintf(stderr, "%s: Failed to update STM firmware\n", argv[0]);
          printf("FW-ERROR\n");
          return(3);
        }
        break;
    }
  }

// @@@ Not starting the radio here, since we are not using it!
#if 0
  if(!stmDevice.go())
  {
    fprintf(stderr, "%s: Failed to start STM device\n", argv[0]);
    printf("INIT-ERROR\n");
    return(1);
  }
#endif

  const char *id = stmDevice.getId();
  if(!id)
  {
    fprintf(stderr, "%s: Failed to read STM device ID\n", argv[0]);
    printf("STAT-ERROR\n");
    return(2);
  }

  printf("STM-ID: %s\n", id);

  return(0);
}
