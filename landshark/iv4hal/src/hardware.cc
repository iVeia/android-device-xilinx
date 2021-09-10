#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

#include "hardware.hh"
#include "debug.hh"
#include "support.hh"

namespace iv4 {
  //TODO: Refactor I2C into a supporting library / object with RAII/auto-closing destructor
  inline bool i2c_read_reg(std::string dev,
                           uint8_t addr,
                           uint8_t reg,
                           uint8_t *val) {
    int i2cfd = open(dev.c_str(), O_RDWR);
    if(i2cfd < 0) return false;

    uint8_t obuf[] = {reg};

    struct i2c_msg msgs[] = {
      {addr, 0, 1, obuf},
      {addr, I2C_M_RD | I2C_M_NOSTART, 1, val},
    };

    int ret = -2;
    struct i2c_rdwr_ioctl_data ioctl_data = { &msgs[0], 2 };
    ret = ioctl(i2cfd, I2C_RDWR, &ioctl_data);
    
    close(i2cfd);
   
    if(ret < 0) {
      return false;
    }
   
    return true;
  }

  static inline bool i2c_write(std::string dev,
                               uint8_t addr,
                               uint8_t reg,
                               uint8_t val) {

    int i2cfd = open(dev.c_str(), O_RDWR);
    if(i2cfd < 0) return false;

    int ret;

    uint8_t dat[] = {reg, val};

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
    bool ret = i2c_read_reg(dev, addr, 0x18, &val);

    if(ret) return val;
    else {
      debug << Debug::Mode::Err << "Failed to read pot value" << std::endl;
      return 0;
    }
  }
  
  bool SetPot(std::string dev, uint8_t addr, uint8_t val) {
    debug << "Setting pot to " << std::hex << (int)val << std::dec << std::endl;
    return i2c_write(dev, addr, 0x18, val);
  }

  bool SetLED(uint8_t lightsVal, bool pwm_leds) {
    debug << "Setting leds to " << lightsVal << std::endl;
    if(pwm_leds) {
      // We need to map 0 - 255 to the range 45500 - 0 (yes inverted)
      float tval = (255.0 - lightsVal) / 255.0; // Invert and range 0 .. 1
      if(tval < 0) tval = 0;
      if(tval > 1) tval = 1;
      int pwm_val = 45500 * tval;

      debug << "pwm: " << tval << ":" << pwm_val << std::endl;

      // I have no idea why but sometimes the sysfs write to the PWM doesn't take the first time
      //  and gets delayed by one write (i.e what gets set on a write is what you wrote last time)
      // But writing the same things twice doesn't work.  So write two values offset by 1
      //  to make sure it takes
      char command_string[64]; command_string[63] = '\0';
      snprintf(command_string, 63, "echo %d > /sys/class/pwm/pwmchip0/pwm0/duty_cycle", pwm_val);
      RunCommand(std::string(command_string));
      usleep(1000);
      snprintf(command_string, 63, "echo %d > /sys/class/pwm/pwmchip0/pwm0/duty_cycle", pwm_val+1);
      RunCommand(std::string(command_string));
    } else {
      if(lightsVal < 0x60) lightsVal = 0;
      if(lightsVal > 0xE0) lightsVal = 0xE0;
      debug << "Setting pot to " << std::hex << lightsVal << std::dec << std::endl;
      SetPot("/dev/i2c-0", 0x2C, lightsVal);
    }

    return true;
  }

  uint8_t GetLED(bool pwm_leds) {
    if(pwm_leds) {
      std::ifstream dsin;
      dsin.open("/sys/class/pwm/pwmchip0/pwm0/duty_cycle");
      int pwm_val;
      dsin >> pwm_val;
      dsin.close();

      double tval = pwm_val / 45500.0;
      if(tval < 0) tval = 0;
      if(tval > 1) tval = 1;
      uint8_t ret_val = 255 - (tval * 255);
      debug << "Read " << pwm_val << ":" << tval << ":" << ret_val <<std::endl;
      
      return ret_val;
    } else {
      uint8_t val = GetPot("/dev/i2c-0", 0x2C);
      debug << "Got pot value of " << std::hex << (int)val << std::dec << std::endl;
      return val;
    }
  }
  
}; // end namespace
