#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "chillups.hh"
#include "debug.hh"
#include "rs485.hh"


namespace iv4 {
#define SET_READ_WAIT_TIME (1000 * 100) // 100ms
  ChillUPSInterface::ChillUPSInterface(RS485Interface *serial, const std::string &dev)
    : _dev(dev), serial(serial) {
    i2cfd = -1;
    tLastFastUpdate = 0;
    tLastSlowUpdate = 0;

    lastMainStatusRead = false;

    chillupsFastUpdateFreq = (FAST_UPDATE_FREQ_S);
    chillupsSlowUpdateFreq = (SLOW_UPDATE_FREQ_S);
  }

  ChillUPSInterface::~ChillUPSInterface() {
    if(opened()) close();
  }

  bool ChillUPSInterface::opened() const {
    return false;    
    //return (i2cfd >= 0 || serial != nullptr);
  }

  bool ChillUPSInterface::open() {
    if(serial != nullptr) return true;
    
    i2cfd = ::open(_dev.c_str(), O_RDWR);    
    if(opened()) {
      debug << Debug::Mode::Debug << "CUPS opened " << _dev << std::endl;
      return true;
    } else {
      debug << Debug::Mode::Failure << "Failed to open i2c dev: " << _dev << ":" <<
        strerror(errno) << std::endl;
      return false;
    }
  }

  bool ChillUPSInterface::close() {
    if(!opened()) return false;

    if(i2cfd >= 0) {
      ::close(i2cfd);
      i2cfd = -1;
    }

    return true;
  }

  inline bool i2c_write(int fd,
                        uint8_t addr,
                        std::vector<uint8_t> &buf) {
    int ret;
    if(fd < 0) {
      debug << Debug::Mode::Warn << "Tried to write register " <<
        std::hex << (int)addr << std::dec <<
        " when i2c is not open" << std::endl;
      return false;
    }

    struct i2c_msg msgs[] = {
      {addr, 0,        static_cast<uint16_t>(buf.size()),      buf.data()},
    };
    
    struct i2c_rdwr_ioctl_data ioctl_data = { &msgs[0], 1 };
    ret = ioctl(fd, I2C_RDWR, &ioctl_data);

    if(ret < 0) {
      debug << Debug::Mode::Failure << "Failed to transmit I2C write message: " <<
        fd << " " << 
        std::hex << (unsigned int)addr << std::dec <<
        strerror(errno) << std::endl;
      return false;
    }
    
    return true;
  }
  
  inline bool i2c_read_reg(int fd,
                           uint8_t addr,
                           uint8_t reg,
                           uint8_t *buf, int buflen,
                           bool write_reg = true) {
    if(fd < 0) {
      debug << Debug::Mode::Warn << "Tried to read register " <<
        std::hex << reg << std::dec << " when i2c is not open" << std::endl;
      return false;
    }
   
    uint8_t cmd[] = {reg};
   
    struct i2c_msg msgs[] = {
      {addr, 0,        1,      cmd}, // Write the register address
      {addr, I2C_M_RD, static_cast<uint16_t>(buflen), buf}, // Then read the result
    };

    int ret = -2;
    if(write_reg) {
      struct i2c_rdwr_ioctl_data ioctl_data = { &msgs[0], 2 };
      ret = ioctl(fd, I2C_RDWR, &ioctl_data);
    } else {
      struct i2c_rdwr_ioctl_data ioctl_data = { &msgs[1], 1 };
      ret = ioctl(fd, I2C_RDWR, &ioctl_data);
    }
   
    if(ret < 0) {
      debug << Debug::Mode::Failure << "Failed to transmit I2C write/read message: " << strerror(errno) << std::endl;
      return false;
    }
   
    return true;
  }

  inline i2c_u8 i2c_read_reg_u8(int fd, uint_t addr, uint_t reg) {
    uint8_t ival;
    bool ret = i2c_read_reg(fd, addr, reg, &ival, 1);
    return std::make_tuple(ret, ival);
  }

  inline i2c_u16 i2c_read_reg_u16(int fd, uint8_t addr, uint8_t reg) {
    uint8_t ival[2];
    uint16_t rval;

    bool ret = i2c_read_reg(fd, addr, reg, ival, 2);
    if(ret) {
      rval = (ival[1] << 8) | ival[0];
      debug << "Read u16: " << std::hex << (short)ival[0] << ":" << (short)ival[1] << " -- " <<
        rval << std::dec << " = " << rval << std::endl;
    } else {
      rval = 0.0;
    }

    return std::make_tuple(ret, rval);
  }

  inline i2c_i16 i2c_read_reg_i16(int fd, uint8_t addr, uint8_t reg) {
    uint8_t ival[2];
    int16_t rval;

    bool ret = i2c_read_reg(fd, addr, reg, ival, 2);
    if(ret) {
      rval = (ival[1] << 8) | ival[0];
      debug << "Read i16: " << std::hex << (short)ival[0] << ":" << (short)ival[1] << " -- " <<
        rval << std::dec << " = " << rval << std::endl;
    } else {
      rval = 0.0;
    }

    return std::make_tuple(ret, rval);
  }


  // -----------------------------------------------------------------
  // -----------------------------------------------------------------
  //  Start functions for reading chillups status
  // -----------------------------------------------------------------
  // -----------------------------------------------------------------
  
  bool ChillUPSInterface::getMainStatus(uint8_t &status_reg) {
    bool success = false;
    
    if(serial != nullptr) {
      uint8_t raddr = RS485Interface::CUPS_ADDRESS;
      uint8_t rtype = RS485Interface::CUPS_GET_STATUS;
      std::vector<uint8_t> msg {0x00};
      RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
      if(ret != RS485Interface::RS485Return::Success || msg.size() < 1) {
        return false;
      } else {
        status_reg = msg[0];
        success = true;
      }
    } else {
      uint8_t regval = 0;
      success = i2c_read_reg(i2cfd, 0x60, 0x00, &regval, 1, false);
      if(success) {
        status_reg = regval;
        success = true;
      }
    }
    
    return success;
  }

  bool ChillUPSInterface::readTemperatures() {
    if(serial == nullptr) return false;

    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_TEMPERATURE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read temperatures failed for address " << raddr << std::endl;
      return false;
    }

    if(rtype != RS485Interface::CUPS_GET_TEMPERATURE_RETURN) {
      debug << Debug::Mode::Err << "read temperature returned wrong type: " << (int)rtype << std::endl;
      return false;
    }

    uint16_t lastTemp = (msg[0] << 8) | (msg[1]);
    lastThermistorTemp = lastTemp * 0.01f;

    uint16_t lastCCTemp = (msg[2] << 8) | (msg[3]);
    lastCalibratedColdCubeTemp = lastCCTemp * 0.01f;

    uint16_t lastCATemp = (msg[4] << 8) | (msg[5]);
    lastCalibratedAmbientTemp = lastCATemp * 0.01f;

    return true;
  }

  bool ChillUPSInterface::readVoltages() {
    if(serial == nullptr) return false;

    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_VOLTAGE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read voltages failed for address " << raddr << std::endl;
      return false;
    }

    if(rtype != RS485Interface::CUPS_GET_VOLTAGE_RETURN) {
      debug << Debug::Mode::Err << "read voltage returned wrong type: " << (int)rtype << std::endl;
      return false;
    }

    lastChargePercent = msg[0] * 0.1f;
    lastSupplyVoltage = msg[1] * 0.1f;

    //lastSupply2Voltage = msg[2] * 0.1;
    lastBackplaneVoltage = msg[2] * 0.1f;
    
    lastSupply3Voltage = msg[3] * 0.1f;
    lastBatteryVoltage = msg[4] * 0.1f;

    //lastBackplaneVoltage = msg[5] * 0.1;

    lastOtherVoltage = msg[6] * 0.1f;
    
    
    return true;
  }

  bool ChillUPSInterface::readPersistentParams() {
    if(serial == nullptr) return false;

    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_PSETTINGS;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read psettings failed for address " << raddr << "::" << (int)ret << std::endl;
      return false;
    }

    if(rtype != RS485Interface::CUPS_GET_PSETTINGS_RETURN) {
      debug << Debug::Mode::Err << "read psettings returned wrong type: " << (int)rtype << std::endl;
      return false;
    }

    uint16_t temp_set = (msg[0] << 8) | msg[1];
    lastSetPoint = temp_set * 0.01f;
    uint8_t temp_range = msg[2];
    lastTempRange = temp_range * 0.01f;
    uint16_t defrost_period = (msg[3] << 8) | msg[4];
    lastDefrostPeriod = defrost_period;
    uint8_t defrost_duration = msg[5];
    lastDefrostLength = defrost_duration;
    uint16_t defrost_limit = (msg[6] << 8) | msg[7];
    lastDefrostTempLimit = defrost_limit * 0.01;
    
    
    return true;
  }

  bool ChillUPSInterface::readThermistorTemp() {
    bool ret = false;
    if(serial != nullptr) {
      ret = true;
    } else {
      i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x00);
      ret = std::get<0>(res);
      if(ret) {
        lastThermistorTemp = std::get<1>(res) * 0.01;
        debug << "Thermistor temp: " << lastThermistorTemp << "::" <<
          std::hex << std::get<1>(res) << std::dec << std::endl;
      }
    }

    return ret;
  }
 
  bool ChillUPSInterface::readDefrostPeriod() {
    if(serial != nullptr) return true;
    i2c_u16 res = i2c_read_reg_u16(i2cfd, 0x64, 0x02);
    bool ret = std::get<0>(res);

    if(ret) {
      lastDefrostPeriod = std::get<1>(res);
      debug << "Defrost Period : " << (unsigned short)lastDefrostPeriod << "::" <<
        std::hex << (unsigned short)std::get<1>(res) << std::dec << std::endl;
    }
    
    return ret;    
  }
  bool ChillUPSInterface::setDefrostPeriod(uint16_t period) {
    uint16_t val = period;
    std::vector<uint8_t> msg {0x02,
        static_cast<uint8_t>((val & 0x00FF) >> 0),
        static_cast<uint8_t>((val & 0xFF00) >> 8),
        };
    
    bool success = i2c_write(i2cfd, 0x64, msg);
    usleep(SET_READ_WAIT_TIME);
    success |= readDefrostPeriod();

    return success;
  }
 
  bool ChillUPSInterface::setDefrostParams(uint16_t period, uint8_t length, uint16_t limit) {
    std::vector<uint8_t> msg {(uint8_t)((period>>8)&0xFF), (uint8_t)(period&0xFF),
        length,
        (uint8_t)((limit>>8)&0xFF), (uint8_t)(limit&0xFF),
        0x00, 0x00, 0x00};
    
    if(!serial->Send(RS485Interface::CUPS_ADDRESS,
                 RS485Interface::CUPS_SET_DEFROST, false, msg)) {
      debug << "Failed to set defrost" << std::endl;
    } else {
      debug << "Set defrost to period " << period << " and length to " <<
        (int)length << " and  limit to " << limit << std::endl;
    }

    return true;
  }

  bool ChillUPSInterface::readChargePercent() {
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x04);
    bool ret = std::get<0>(res);

    if(ret) {
      lastChargePercent = std::get<1>(res);
      debug << "Charge % " << (unsigned short)lastChargePercent << "::" <<
        std::hex << (short)std::get<1>(res) << std::dec << std::endl;
    }
    
    return ret;
  }
 
  bool ChillUPSInterface::readSupplyVoltage() {
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x05);
    bool ret = std::get<0>(res);
    if(ret) {
      lastSupplyVoltage = std::get<1>(res) * 0.1;
      debug << "Supply voltage " << lastSupplyVoltage << "::" <<
        std::hex << (short)std::get<1>(res) << std::dec << std::endl;
    }
   
    return ret;
  }
 
  bool ChillUPSInterface::readBatteryVoltage() {
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x06);
    bool ret = std::get<0>(res);
    if(ret) {
      lastBatteryVoltage = std::get<1>(res) * 0.1;
      debug << "Battery voltage " << lastBatteryVoltage << "::" <<
        std::hex << (short)std::get<1>(res) << std::dec << std::endl;
    }
   
    return ret;
  }
 
  bool ChillUPSInterface::readBackplaneVoltage() {
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x07);
    bool ret = std::get<0>(res);
    if(ret) {
      lastBackplaneVoltage = std::get<1>(res) * 0.1;
      debug << "Backplane voltage " << lastBackplaneVoltage << "::" <<
        std::hex << (short)std::get<1>(res) << std::dec << std::endl;
    }
   
    return ret;
  }
 
  bool ChillUPSInterface::readOtherVoltage() {
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x08);
    bool ret = std::get<0>(res);

    if(ret) {
      lastOtherVoltage = std::get<1>(res) * 0.1;
      debug << "Other voltage " << lastOtherVoltage << "::" <<
        std::hex << (short)std::get<1>(res) << std::dec << std::endl;
    }
   
    return ret;
  }
  
  bool ChillUPSInterface::readTempRange() {
    if(serial != nullptr) return true;
    
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x09);
    bool ret = std::get<0>(res);

    if(ret) {
      lastTempRange = std::get<1>(res) * 0.01;
      debug << "Range: " << lastTempRange << "::" <<
        std::hex << (short)std::get<1>(res) << std::dec << std::endl;
    }
   
    return ret;
  }
  bool ChillUPSInterface::setTempRange(float range) {
    uint8_t irange = static_cast<uint8_t>(range * 100);
    
    std::vector<uint8_t> msg {0x09, irange };

    bool success = i2c_write(i2cfd, 0x64, msg);
    debug << "Wrote " << std::hex << (short)irange << std::dec << " to 0x64 0x09" << std::endl;
    usleep(SET_READ_WAIT_TIME);
    success |= readTempRange();

    return success;
  }
  
  bool ChillUPSInterface::readSetPoint() {
    if(serial != nullptr) return true;
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x0E);
    bool ret = std::get<0>(res);

    if(ret) {
      lastSetPoint = std::get<1>(res) * 0.01;
      debug << "Set Point " << lastSetPoint << "::" <<
        std::hex << std::get<1>(res) << std::dec << std::endl;
    }
   
    return ret;
  }
  bool ChillUPSInterface::setSetPoint(float setPoint) {
    if(serial != nullptr) return true;
    int16_t val = static_cast<int16_t>(setPoint * 100);

    std::vector<uint8_t> msg {0x0E,
        static_cast<uint8_t>((val & 0x00FF) >> 0),
        static_cast<uint8_t>((val & 0xFF00) >> 8),
        };
    
    bool success = i2c_write(i2cfd, 0x64, msg);
    debug << "Wrote " << std::hex << (short)val << std::dec << " to 0x64 0x0E" << std::endl;
    usleep(SET_READ_WAIT_TIME);
    success |= readSetPoint();

    return success;
  }

  bool ChillUPSInterface::setTemperature(uint16_t temp, uint8_t range) {
    std::vector<uint8_t> msg {(uint8_t)((temp>>8)&0xFF), (uint8_t)(temp&0xFF), range, 0x00};
    debug << "Setting temp to " << temp << " and " << range << std::endl;
    if(!serial->Send(RS485Interface::CUPS_ADDRESS,
                 RS485Interface::CUPS_SET_TEMPERATURE, false, msg)) {
      debug << "Failed to set temp and range" << std::endl;
    } else {
      debug << "Set temp to " << temp << " and range to " << range << std::endl;
    }

    return true;
  }

  
  bool  ChillUPSInterface::readCompressorError(uint8_t &comp_error) {
    if(serial != nullptr) {
      uint8_t raddr = RS485Interface::CUPS_ADDRESS;
      uint8_t rtype = RS485Interface::CUPS_GET_COMPR_ERROR;
      std::vector<uint8_t> msg {0x00};
      RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
      if(ret != RS485Interface::RS485Return::Success || msg.size() < 1) {
        debug << Debug::Mode::Err << "read compressor error return failed" << std::endl;
        return false;
      }

      if(rtype != RS485Interface::CUPS_GET_COMPR_ERROR_RETURN) {
        debug << Debug::Mode::Err << "read compressor error return type wrong" << std::endl;
        return false;
      }

      comp_error = msg[0];
      return true;

    } else {
      i2c_u8 err_reg = i2c_read_reg_u8(i2cfd, 0x64, 0x10);    
      if(std::get<0>(err_reg)) {
        comp_error = std::get<1>(err_reg);
        return true;
      }
    }

    return false;
  }
  
  bool ChillUPSInterface::readDefrostLength(){
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x12);
    bool ret = std::get<0>(res);

    if(ret) {
      lastDefrostLength = std::get<1>(res);
    }

    return ret;
  }
  bool ChillUPSInterface::setDefrostLength(uint8_t length) {
    std::vector<uint8_t> msg {0x12, length};
    
    bool success = i2c_write(i2cfd, 0x64, msg);
    usleep(SET_READ_WAIT_TIME);
    success |= readDefrostLength();

    return success;    
  }
  
  bool ChillUPSInterface::readCompressorBackupState(){
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x13);
    bool ret = std::get<0>(res);
    uint8_t val = std::get<1>(res);

    if((val == 0xFF || val == 0x00) & ret) {
      lastCompressorBackupState = val;
      return true;
    } else return false;
  }
  bool ChillUPSInterface::setCompressorBackupState(bool state) {
    std::vector<uint8_t> msg {0x13, static_cast<uint8_t>(((state)?0xFF:0x00))};
    
    bool success = i2c_write(i2cfd, 0x64, msg);
    usleep(SET_READ_WAIT_TIME);
    success |= readCompressorBackupState();

    return success;
  }
  
  bool ChillUPSInterface::readTempLoggingState(){
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x14);
    bool ret = std::get<0>(res);
    uint8_t val = std::get<1>(res);

    if((val == 0xFF || val == 0x00) & ret) {
      lastTempLoggingState = val;
      return true;
    } else return false;
  }
  
  bool ChillUPSInterface::setTempLoggingState(bool state) {
    std::vector<uint8_t> msg {0x14, static_cast<uint8_t>(((state)?0xFF:0x00))};

    bool success = i2c_write(i2cfd, 0x64, msg);
    usleep(SET_READ_WAIT_TIME);
    success |= readTempLoggingState();

    return success;
  }
  
  bool ChillUPSInterface::readDefrostTempLimit(){
    if(serial != nullptr) return true;
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x16);
    bool ret = std::get<0>(res);

    if(ret) {
      lastDefrostTempLimit = std::get<1>(res) * 0.01;
    }
   
    return ret;
  }
  bool ChillUPSInterface::setDefrostTempLimit(float limit) {
    int16_t val = static_cast<int16_t>(limit * 100);

    std::vector<uint8_t> msg {0x16,
        static_cast<uint8_t>((val & 0x00FF) >> 0),
        static_cast<uint8_t>((val & 0xFF00) >> 8),
        };
    
    bool success = i2c_write(i2cfd, 0x64, msg);
    usleep(SET_READ_WAIT_TIME);
    success |= readDefrostTempLimit();

    return success;
  }
  
  bool ChillUPSInterface::readCalibratedColdCubeTemp(){
    if(serial != nullptr) return true;
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x18);
    bool ret = std::get<0>(res);

    if(ret) {
      lastCalibratedColdCubeTemp = std::get<1>(res) * 0.01;
    }
   
    return ret;
  }
  
  bool ChillUPSInterface::readCalibratedAmbientTemp(){
    if(serial != nullptr) return true;
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x21);
    bool ret = std::get<0>(res);

    if(ret) {
      lastCalibratedAmbientTemp = std::get<1>(res) * 0.01;
    }
   
    return ret;
  }
  
  bool ChillUPSInterface::readVersion(){
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x11);
    bool ret = std::get<0>(res);
    uint8_t val = std::get<1>(res);

    chillups_version ver;
    if(ret) {
      ver.major = (val>>4) & 0x0F;
      ver.minor = (val) & 0x0F;
    } else {
      ver.major = 0x00;
      ver.minor = 0x00;
    }
    currentVersion = ver;

    return ret;
  }
  
  ChillUPSInterface::chillups_id  ChillUPSInterface::readColdCubeID(){
    chillups_id id;

    if(serial != nullptr) {
      uint8_t raddr = RS485Interface::CUPS_ADDRESS;
      uint8_t rtype = RS485Interface::CUPS_GET_CAL_PROBE_ID;
      std::vector<uint8_t> msg {0x00};
      RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
      if(ret != RS485Interface::RS485Return::Success || msg.size() < 8) {
        debug << Debug::Mode::Err << "read cold cube ID return failed" << std::endl;
        return id;
      }

      if(rtype != RS485Interface::CUPS_GET_CAL_PROBE_ID_RETURN) {
        debug << Debug::Mode::Err << "read cold cube ID return type wrong" << std::endl;
        return id;
      }

      id.family = msg[1];
      for(int i = 0; i < 6; i++) id.id[i] = msg[i + 2];
      
    } else {
      uint8_t *idp = reinterpret_cast<uint8_t*>(&id);
      bool ret = i2c_read_reg(i2cfd, 0x64, 0x1A, idp, sizeof(id), true);
      
      if(!ret) {
        // how to indicate failure here?
      }
    }

    return id;
  }
  
  ChillUPSInterface::chillups_id  ChillUPSInterface::readAmbientID(){
    chillups_id id;

    if(serial != nullptr) {
      uint8_t raddr = RS485Interface::CUPS_ADDRESS;
      uint8_t rtype = RS485Interface::CUPS_GET_CAL_PROBE_ID;
      std::vector<uint8_t> msg {0x01};
      RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
      if(ret != RS485Interface::RS485Return::Success || msg.size() < 8) {
        debug << Debug::Mode::Err << "read cold cube ID return failed" << std::endl;
        return id;
      }
      
      if(rtype != RS485Interface::CUPS_GET_CAL_PROBE_ID_RETURN) {
        debug << Debug::Mode::Err << "read cold cube ID return type wrong" << std::endl;
        return id;
      }
      
      id.family = msg[1];
      for(int i = 0; i < 6; i++) id.id[i] = msg[i + 2];
      
    } else {
      uint8_t *idp = reinterpret_cast<uint8_t*>(&id);
      bool ret = i2c_read_reg(i2cfd, 0x64, 0x23, idp, sizeof(id), true);
      
      if(!ret) {
        // how to indicate failure here?
      }
    }

    return id;
  }
  
  bool ChillUPSInterface::readBoardConfig(){
    if(serial != nullptr) return true;
    i2c_u8 res = i2c_read_reg_u8(i2cfd, 0x64, 0x11);
    bool ret = std::get<0>(res);
    uint8_t val = std::get<1>(res);

    board_config conf;
    if(ret) {
      conf.id = val & 0x0F;
      conf.calColdCube = (val & 0x10) != 0;
      conf.calAmbient = (val & 0x20) != 0;
    } else {

    }
    conf.valid = ret;
    currentBoardConfig = conf;

    return ret;
  }
  
  bool ChillUPSInterface::readRecordedTemps(std::vector<std::tuple<int, float> > &temps){
    bool done = false;
    while(!done) {


      if(serial != nullptr) {
        uint8_t raddr = RS485Interface::CUPS_ADDRESS;
        uint8_t rtype = RS485Interface::CUPS_GET_LOGGED_TEMP;
        std::vector<uint8_t> msg {0x00};
        RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
        if(ret != RS485Interface::RS485Return::Success || msg.size() < 4) {
          debug << Debug::Mode::Err << "read logged temps return failed" << std::endl;
          return false;
        }
        
        if(rtype != RS485Interface::CUPS_GET_LOGGED_TEMP_RETURN) {
          debug << Debug::Mode::Err << "read logged temp return type wrong" << std::endl;
          return false;
        }

        uint16_t ndx = (msg[0] << 8) | msg[1];
        uint16_t temp = (msg[2] << 8) | msg[3];
        if(ndx == 0 && temp == 0) {
          // all done
          break;
        }
        
        temps.push_back(std::make_tuple(ndx, temp * 0.01f));
        
      } else {
        i2c_u16 ndx_res = i2c_read_reg_u16(i2cfd, 0x64, 0x0A);
        i2c_u16 ndx_val = i2c_read_reg_u16(i2cfd, 0x64, 0x0C);
        
        if(std::get<0>(ndx_res) && std::get<0>(ndx_val)) {
          if(std::get<1>(ndx_res) == 0) {
            // all done
            break;
          }
          
          temps.push_back(std::make_tuple(std::get<1>(ndx_res),
                                          std::get<1>(ndx_val) * 0.01));
          
        } else {
          // What to do in the case of a i2c error here?
          return false;
        }
      }
    }

    return true;
  }

  bool ChillUPSInterface::updateMainStatus(SocketInterface &intf, bool send) {
    // Read the status register
    uint8_t mainStatus_reg;
    bool status_ret = getMainStatus(mainStatus_reg);
    if(status_ret) {
      cupsStatus mainStatus = cupsStatus(mainStatus_reg);
      if(mainStatus != lastMainStatus) {
        // Status changed, so we have to send a message.  But first we have to
        //  check on some things
        if(mainStatus.acStatus != lastMainStatus.acStatus) {
          // TODO: We have to turn the leds on or off
        }
        
        if(mainStatus.firmwareState != lastMainStatus.firmwareState) {
          if(!mainStatus.firmwareState) {
            // TODO: We should send a special catastrophic failure message here
            
          }
        }
        
        if(send) {
          //TODO: Send the status message up
        }
        
        lastMainStatus = mainStatus;
      }
      
      return true;
    } else {
      // Failed to read...  What to do?
      return false;
    }
  }

  bool ChillUPSInterface::updateSlowStatus(SocketInterface &intf, bool send) {
    bool success = true;

    if(serial != nullptr) {
      readTemperatures();
      readVoltages();

      readPersistentParams();
    } else {
      success |= readThermistorTemp();
      success |= readDefrostPeriod();
      success |= readChargePercent();
      success |= readSupplyVoltage();
      success |= readBatteryVoltage();
      success |= readBackplaneVoltage();
      success |= readOtherVoltage();
      if(currentBoardConfig.calColdCube) {
        success|= readCalibratedColdCubeTemp();
      }
      
      if(currentBoardConfig.calAmbient) {
        success |= readCalibratedAmbientTemp();
      }
      
    }
    
    // Just append to recorded temperatures.  Don't clear it
    success |= readRecordedTemps(savedTemps);

    return success;
  }
  

  bool ChillUPSInterface::Initialize(SocketInterface &intf) {
    open();

    if(serial != nullptr) {
      debug << "Running CUPS discovery" << std::endl;
      bool discover_success = discover();
      if(!discover_success) {
        debug << Debug::Mode::Err << "Failed to discover chillups" << std::endl;
        return false;
      }
    }
    
    if(serial != nullptr && !lastMainStatus.bootACK) {
      // acknowledge CUPS boot
      acknowledgeBoot();

      if(!lastMainStatus.bootACK) {
        debug << Debug::Mode::Info << "Boot ACK failed" << std::endl;
      }
    }
    
    // First we get the main status, but don't send anything yet
    if(!updateMainStatus(intf, false)) {
      close();
      return false;
    }
    
    if(!lastMainStatus.firmwareState) {
      // TODO: This is a catastrophic failure.  Send a message
    }

    if(!lastMainStatus.acStatus) {
      // TODO: Turn off leds and send a message
    }

    if(lastMainStatus.comprError) {
      uint8_t comprError;
      bool success = readCompressorError(comprError);
      if(success) {
        Message msg(Message::CUPS, Message::CUPS.CompressorError,
                     (comprError), 0, 0, 0);
        debug << "Sending compressor error as an event " << std::endl;
        intf.Send(msg);
      } else {
        // Catastrophic system failure
        // TODO: Need a communication failed event
      }
    }

    {
      // TODO: Set the temp logging and battery backup with values from aggregate
      setTempLoggingState(true);
      setCompressorBackupState(false);
    }

    readVersion();

    // TODO: What to do with this?
    readBoardConfig();

    // Read any saved temperatures
    savedTemps.clear();
    readRecordedTemps(savedTemps);
    if(savedTemps.size() > 0) {
      // TODO:? send an event saying saved temps are available
      //  Currently this is handled when the client requests status.  All the saved temps will be sent then
      //  There is no message for temps available
      //  Current plan is to leave it that way

      debug << Debug::Mode::Info << "We have " << savedTemps.size() << " stored temps to send" << std::endl;
    }

    readSetPoint();
    readTempRange();

    // Then we run the slow update to populate initial values
    updateSlowStatus(intf, false);

    close();
    return true;
  }

  std::string NibbleToHex(uint8_t nibble) {
    std::string ret = "";
    if(nibble >= 0 && nibble <= 9) ret.push_back((char)(nibble + '0'));
    else if(nibble >= 0x0A && nibble < 0x0F) ret.push_back((char)(nibble + 'A'));
    else ret = "X";

    return ret;
  }
    
  std::string ByteToHex(uint8_t byte) {
    return NibbleToHex( (byte >> 4) & 0x0F) + NibbleToHex(byte & 0x0F);
  }
  
  std::string ChillUPSInterface::IdToString(chillups_id id) {
    return ByteToHex(id.family) + ":" +
      ByteToHex(id.id[0]) + ":" +
      ByteToHex(id.id[1]) + ":" +
      ByteToHex(id.id[2]) + ":" +
      ByteToHex(id.id[3]) + ":" +
      ByteToHex(id.id[4]) + ":" +
      ByteToHex(id.id[5]);      
  }

  //TODO: Instead of having all these close() operations sprinkled everywhere, rely on RAII
  //      and object destructor
  std::unique_ptr<Message> ChillUPSInterface::ProcessMessage(const Message &m) {
    if(m.header.type != Message::CUPS) return Message::MakeNACK(m, 0, "Invalid message passed to ChillUPSInterface");

    open();
    
    switch(m.header.subType) {
    case Message::CUPS.SetTemperature:
      {
        int16_t temp = static_cast<int16_t>(m.header.imm[0]);
        int16_t range = static_cast<int16_t>(m.header.imm[1]);

        if(temp < -2000 || temp > 4000) {
          close();
          return Message::MakeNACK(m, 0, "Temperature out of range");
        }

        // TODO: No limit to range in the docs - determine what a good limit is
        if(serial != nullptr) {
          setTemperature(temp, range);
          usleep(2500);
          readPersistentParams();
        } else {          
          setSetPoint(temp * 0.01);
          setTempRange(range * 0.01);
        }
        
        debug << "Set temp to " << temp << " +- " << range << std::endl;
      }
      break;

    case Message::CUPS.GetTemperature:
      {
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetTemperature,
                                                    static_cast<int>(lastSetPoint * 100),
                                                    static_cast<int>(lastTempRange * 100),
                                                    0, 0));
      }
      break;
      
    case Message::CUPS.GetAllTemperatures:
      {
        int count = 0;
        std::vector<uint8_t> payload;
        {
          std::string name("thermistor");
          std::string val = std::to_string(lastThermistorTemp);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        
        if(currentBoardConfig.calColdCube) {
          std::string name("calibrated_cold_cube");
          std::string val = std::to_string(lastCalibratedColdCubeTemp);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }

        if(currentBoardConfig.calAmbient) {
          std::string name("calibrated_ambient");
          std::string val = std::to_string(lastCalibratedAmbientTemp);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }

        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetAllTemperatures,
                                                    count, 0, 0, 0,
                                                    payload));
      }
      break;
      
    case Message::CUPS.SetDefrostParams:
      {
        if(m.header.imm[0] > 65536) {
          close();
          return Message::MakeNACK(m, 0, "Defrost period out of range");
        }
        uint16_t newDefrostPeriod = m.header.imm[0];

        if(m.header.imm[1] > 255) {
          close();
          return Message::MakeNACK(m, 0, "Defrost length out of range");
        }
        uint8_t newDefrostLength = m.header.imm[1];

        int val2 = m.header.imm[2];
        float newDefrostLimit = val2 * 0.01;
        if(newDefrostLimit < -20.0 || newDefrostLimit > 40.0) {
          close();
          return Message::MakeNACK(m, 0, "Defrost limit out of range");
        }

        bool success = true;
        if(serial != nullptr) {
          success = setDefrostParams(newDefrostPeriod, newDefrostLength, newDefrostLimit*100);
          usleep(2500);
          readPersistentParams();
        } else {
          success |= setDefrostPeriod(newDefrostPeriod);
          success |= setDefrostLength(newDefrostLength);
          success |= setDefrostTempLimit(newDefrostLimit);
        }

        if(!success) {
          close();
          return Message::MakeNACK(m, 0, "Failed to set defrost settings");
        }
      }
      break;
    case Message::CUPS.GetDefrostParams:
      {
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetDefrostParams,
                                                    lastDefrostPeriod,
                                                    lastDefrostLength, 
                                                    static_cast<int>(lastDefrostTempLimit * 100),
                                                   0));
      }
      break;
      
    case Message::CUPS.InitiateDefrost:
      {
        bool success = true;
        if(auto_chill) {
          std::vector<uint8_t> msg1 {0x20};
          success |= i2c_write(i2cfd, 0x60, msg1);
          std::vector<uint8_t> msg2 {0x60};
          success |= i2c_write(i2cfd, 0x60, msg2);
        } else {
          std::vector<uint8_t> msg1 {0x00};
          success |= i2c_write(i2cfd, 0x60, msg1);
          std::vector<uint8_t> msg2 {0x40};
          success |= i2c_write(i2cfd, 0x60, msg2);
        }

        close();
        if(!success) return Message::MakeNACK(m, 0, "Failed to initiate defrost");
      }
      break;
      
    case Message::CUPS.IntiiateBatteryTest  :
      {
        bool success;
        if(auto_chill) {
          std::vector<uint8_t> msg {0x21};
          success = i2c_write(i2cfd, 0x60, msg);
        } else {
          std::vector<uint8_t> msg {0x01};
          success = i2c_write(i2cfd, 0x60, msg);
        }

        if(!success) {
          close();
          return Message::MakeNACK(m, 0, "Failed to initiate battery test");
        }
      }
      break;
      
    case Message::CUPS.GetAllVoltages:
      {
        int count = 0;
        std::vector<uint8_t> payload;
        {
          std::string name("supply");
          std::string val = std::to_string(lastSupplyVoltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        {
          std::string name("battery");
          std::string val = std::to_string(lastBatteryVoltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        {
          std::string name("backplane");
          std::string val = std::to_string(lastBackplaneVoltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        {
          std::string name("charger");          
          std::string val = std::to_string(lastOtherVoltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }

        debug << "Returning " << count << " voltages" << std::endl;
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetAllVoltages,
                                                    count, 0, 0, 0,
                                                    payload));        
      }
      break;

    case Message::CUPS.GetBatteryPercent:
      {
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetBatteryPercent,
                                                    static_cast<int>(lastChargePercent * 100),
                                                    0, 0, 0));
      }
      break;
      
    case Message::CUPS.GetStoredTemperatures:
      {
        std::vector<uint8_t> payload;
        int count = savedTemps.size();
        for(auto t : savedTemps) {
          std::string ndx = std::to_string(std::get<0>(t));
          std::string val = std::to_string(std::get<1>(t));
          for(auto c : ndx) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');          
        }
        savedTemps.clear();
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetStoredTemperatures,
                                                    count, 0, 0, 0,
                                                    payload));        
      }
      break;

    case Message::CUPS.GetProbeIDs:
      {
        chillups_id ccid = readColdCubeID();
        chillups_id aid = readAmbientID();
        std::vector<uint8_t> payload;
        std::string ccid_s = std::string("coldcube:") + IdToString(ccid);
        for(auto c : ccid_s) payload.push_back(c);
        payload.push_back('\0');
        
        std::string aid_s = std::string("ambient:") + IdToString(aid);
        for(auto c : aid_s) payload.push_back(c);
        payload.push_back('\0');
        
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetProbeIDs,
                                                    2, 0, 0, 0,
                                                    payload));
      }
      break;
      
    case Message::CUPS.CompressorError:
      {
        close();
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.CompressorError,
                                                    static_cast<int>(lastCompressorError),
                                                    0, 0, 0));        
      }
      break;
      
    default:
      {
        close();
        return Message::MakeNACK(m, 0, "Invalid CUPS message subtype");
      }

          
    }

    close();
    return Message::MakeACK(m);
  }

  bool ChillUPSInterface::acknowledgeBoot() {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_INITIATE_OPERATION;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Boot ack failed for address " << raddr << std::endl;
      usleep(1000);
      return false;
    }
    
    // Make sure the message is the correct size
    if(msg.size() < 1) {
      debug << "Message size wrong for book ack: " << msg.size() << std::endl;
      return false;
    }
    
    // Make sure this is a discovery response
    if(rtype != RS485Interface::CUPS_GET_STATUS_RETURN) {
      debug << Debug::Mode::Info << "boot ack return is not correct message type: " <<
        std::hex << (int)rtype << std::dec << std::endl;
      return false;
    }

    lastMainStatus = cupsStatus(msg[0]);

    return true;
  }
  
  // This will populate the dsbs variable
  bool ChillUPSInterface::discover() {
    
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::DISCOVERY;    
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Discovery failed for address " << raddr << std::endl;
      usleep(1000);
      return false;
    }
    
    // Sanity check the address
    if(raddr != 15) {
      debug << "discovery read address wrong: " << (int)raddr << "::" << 15 << std::endl;
      return false;
    }
    
    // Make sure the message is the correct size
    if(msg.size() != 8) {
      debug << "Message size wrong for discovery: " << msg.size() << std::endl;
      return false;
    }
    
    // Make sure this is a discovery response
    if(rtype != RS485Interface::DISCOVERY_RETURN) {
      debug << Debug::Mode::Info << "Discovery return is not correct message type: " <<
        std::hex << (int)rtype << std::dec << std::endl;
      return false;
    }
    
    // Check the address against the device type
    uint8_t dtype = msg[0] & 0x0F;
    if(dtype == 7) {
      if(raddr != 15) { // CUPS sends response to me
        debug << Debug::Mode::Err << "CUPS at wrong address: " << (int)raddr << std::endl;
        return false;
      }
    } else {
      debug << Debug::Mode::Err << "Unknown device type: " << (int)dtype << std::endl;
      return false;
    }

    // Now to get versions and _stuff_
    currentVersion.major = (msg[7]>>4) & 0x0F;
    currentVersion.minor = msg[7] & 0x0f;
    
    bool calColdPresent = ((msg[1] & 0x20) != 0);
    bool calAmbiPresent = ((msg[1] & 0x10) != 0);

    debug << Debug::Mode::Info <<
      "CUPS discovered -" <<
      " id: " << std::hex << (int)(msg[1] & 0x0F) << std::dec << 
      " ver: " << currentVersion.major << "." << currentVersion.minor << 
      " Calibrated Cold:    " << (calColdPresent?"*":" ") <<
      " Calibrated Ambient: " << (calAmbiPresent?"*":" ") <<
      std::endl;
    return true;
  }


  bool ChillUPSInterface::ProcessMainLoop(SocketInterface &intf) {
    time_t tnow = time(nullptr);

    bool success = true;
    if((tLastFastUpdate > tnow) ||
       ((tnow - tLastFastUpdate) > chillupsFastUpdateFreq)) {
      open();
      success |= updateMainStatus(intf);
      tLastFastUpdate = tnow;
      close();
    }

    if((tLastSlowUpdate > tnow) ||
       ((tnow - tLastSlowUpdate) > chillupsSlowUpdateFreq)) {
      open();
      success |= updateSlowStatus(intf);
      tLastSlowUpdate = tnow;
      close();
    }
    return success;
  }
  
};
