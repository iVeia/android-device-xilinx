#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "hardware.hh"
#include "debug.hh"

namespace iv4 {
  //TODO: Refactor I2C into a supporting library / object with RAII/auto-closing destructor
  inline bool i2c_read_reg(std::string dev,
                           uint8_t addr,
                           uint8_t *val) {
    int i2cfd = open(dev.c_str(), O_RDWR);
    if(i2cfd < 0) return false;

    struct i2c_msg msgs[] = {
      {addr, I2C_M_RD, 1, val},
    };

    int ret = -2;
    struct i2c_rdwr_ioctl_data ioctl_data = { &msgs[0], 1 };
    ret = ioctl(i2cfd, I2C_RDWR, &ioctl_data);
    
    close(i2cfd);
   
    if(ret < 0) {
      return false;
    }
   
    return true;
  }

  static inline bool i2c_write(std::string dev,
                               uint8_t addr,
                               uint8_t val) {

    int i2cfd = open(dev.c_str(), O_RDWR);
    if(i2cfd < 0) return false;

    int ret;

    uint8_t dat[] = {0, val};

    struct i2c_msg msgs[] = {
      {addr, 0, 2, dat},
    };
    
    struct i2c_rdwr_ioctl_data ioctl_data = { &msgs[0], 1 };
    ret = ioctl(i2cfd, I2C_RDWR, &ioctl_data);

    close(i2cfd);
    
    if(ret < 0) {
      return false;
    }
    
    return true;
  }

  uint8_t GetPot(std::string dev, uint8_t addr) {
    uint8_t val = 43;
    bool ret = i2c_read_reg(dev, addr, &val);

    if(ret) return val;
    else {
      debug << Debug::Mode::Err << "Failed to read pot value" << std::endl;
      return 0;
    }
  }
  
  bool SetPot(std::string dev, uint8_t addr, uint8_t val) {
    debug << "Setting pot to " << std::hex << (int)val << std::dec << std::endl;
    return i2c_write(dev, addr, val);
  }

};
