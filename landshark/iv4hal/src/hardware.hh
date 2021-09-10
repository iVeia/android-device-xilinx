#ifndef _IVHAL_HARDWARE_HH
#define _IVHAL_HARDWARE_HH

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "debug.hh"

namespace iv4 {
  // Get and set the value of the pot
  uint8_t GetPot(std::string dev, uint8_t addr);
  bool SetPot(std::string dev, uint8_t addr, uint8_t val);

  uint8_t GetLED(bool pwm_leds);
  bool SetLED(uint8_t lightsVal, bool pwm_leds);

  
  class FDManager {
  public:
    explicit FDManager(const std::string &dev, int flags) {
      fd = open(dev.c_str(), flags);
      debug << "Opened " << dev << "::" << fd << std::endl;
      if(fd < 0) err = errno;
    }

    explicit FDManager(const std::string &dev, int flags, int mode) {
      fd = open(dev.c_str(), flags, mode);
      debug << "Opened " << dev << "::" << fd << std::endl;
      if(fd < 0) err = errno;
    }
    
    ~FDManager() {
      if(fd >= 0) close(fd);
      debug << "Closed " << fd << std::endl;
      fd = -1;
    }

    inline bool Good() const {return fd >= 0;}
    inline int FD() const {return fd;}
    inline int Err() const {if(fd < 0) return err; else return 0;}
    
  protected:
    int fd;
    int err;

  private:
  };
};

#endif
