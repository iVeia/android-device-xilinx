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


namespace iv4 {
#define SET_READ_WAIT_TIME (1000 * 100) // 100ms
  ChillUPSInterface::ChillUPSInterface(std::string dev) :_dev(dev) {
    i2cfd = -1;
    tLastFastUpdate = 0;
    tLastSlowUpdate = 0;

    lastMainStatusRead = false;

    chillupsFastUpdateFreq = (10);      // Every 10s
    chillupsSlowUpdateFreq = (60 * 2);  // Every 2m
  }

  ChillUPSInterface::~ChillUPSInterface() {
    if(opened()) close();
  }

  bool ChillUPSInterface::opened() const {
    return i2cfd >= 0;
  }

  bool ChillUPSInterface::open() {
    i2cfd = ::open(_dev.c_str(), O_RDWR);
    if(opened()) return true;
    else {
      debug << Debug::Mode::Failure << "Failed to open i2c dev: " << _dev << ":" <<
        strerror(errno) << std::endl;
      return false;
    }
  }

  bool ChillUPSInterface::close() {
    if(!opened()) return false;
    ::close(i2cfd);
    i2cfd = -1;

    return true;
  }

  inline bool i2c_write(int fd,
                        uint8_t addr,
                        std::vector<uint8_t> &buf) {
    int ret;
    if(fd < 0) {
      debug << Debug::Mode::Warn << "Tried to write register " <<
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

 
  i2c_u8  ChillUPSInterface::getMainStatus() {
    if(!opened()) {
      debug  << Debug::Mode::Warn << "Tried to read main status register when i2c is not open" << std::endl;
      return std::make_tuple(false, 0);
    }

    uint8_t regval = 0;
    bool ret = i2c_read_reg(i2cfd, 0x60, 0x00, &regval, 1, false);

    return std::make_tuple(ret, regval);
  }

 
  bool ChillUPSInterface::readThermistorTemp() {
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x00);
    bool ret = std::get<0>(res);
    if(ret) {
      lastThermistorTemp = std::get<1>(res) * 0.01;
      debug << "Thermistor temp: " << lastThermistorTemp << "::" <<
        std::hex << std::get<1>(res) << std::dec << std::endl;
    }

    return ret;
  }
 
  bool ChillUPSInterface::readDefrostPeriod() {
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
 
  bool ChillUPSInterface::readChargePercent() {
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
  
  i2c_u8  ChillUPSInterface::readCompressorError() {
    return i2c_read_reg_u8(i2cfd, 0x64, 0x10);    
  }
  
  bool ChillUPSInterface::readDefrostLength(){
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
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x18);
    bool ret = std::get<0>(res);

    if(ret) {
      lastCalibratedColdCubeTemp = std::get<1>(res) * 0.01;
    }
   
    return ret;
  }
  
  bool ChillUPSInterface::readCalibratedAmbientTemp(){
    i2c_i16 res = i2c_read_reg_i16(i2cfd, 0x64, 0x21);
    bool ret = std::get<0>(res);

    if(ret) {
      lastCalibratedAmbientTemp = std::get<1>(res) * 0.01;
    }
   
    return ret;
  }
  
  bool ChillUPSInterface::readVersion(){
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
    uint8_t *idp = reinterpret_cast<uint8_t*>(&id);
    bool ret = i2c_read_reg(i2cfd, 0x64, 0x1A, idp, sizeof(id), true);

    if(!ret) {
      // how to indicate failure here?
    }

    return id;
  }
  
  ChillUPSInterface::chillups_id  ChillUPSInterface::readAmbientID(){
    chillups_id id;
    uint8_t *idp = reinterpret_cast<uint8_t*>(&id);
    bool ret = i2c_read_reg(i2cfd, 0x64, 0x23, idp, sizeof(id), true);

    if(!ret) {
      // how to indicate failure here?
    }

    return id;
  }
  
  bool ChillUPSInterface::readBoardConfig(){
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

    return true;
  }

  bool ChillUPSInterface::updateMainStatus(SocketInterface &intf, bool send) {
      // Read the status register
      i2c_u8 sReg = getMainStatus();
      if(std::get<0>(sReg)) {
        cupsStatus mainStatus = cupsStatus(std::get<1>(sReg));
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

    success |= readThermistorTemp();
    success |= readDefrostPeriod();
    success |= readChargePercent();
    success |= readSupplyVoltage();
    success |= readBatteryVoltage();
    success |= readBackplaneVoltage();
    success |= readOtherVoltage();

    // Just append to recorded temperatures.  Don't clear it
    success |= readRecordedTemps(savedTemps);

    if(currentBoardConfig.calColdCube) {
      success|= readCalibratedColdCubeTemp();
    }

    if(currentBoardConfig.calAmbient) {
      success |= readCalibratedAmbientTemp();
    }
    
    return success;
  }
  

  bool ChillUPSInterface::Initialize(SocketInterface &intf) {
    open();
    
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
      // TODO: There was a compressor error.  Read it and send a message
      i2c_u8 comprError = readCompressorError();
      if(std::get<0>(comprError)) {
        Message msg(Message::CUPS, Message::CUPS.CompressorError,
                     std::get<1>(comprError), 0, 0, 0);
        debug << "Sending compressor error as an event " << std::endl;
        intf.Send(msg);
      } else {
        // Catastrophic system failure
        // TODO: Need a "I2C" failed event
      }
    }

    {
      // TODO: Set the temp logging and battery backup with values from aggregate
      setTempLoggingState(true);
      setCompressorBackupState(false);
    }

    // TODO: Send the verion up
    readVersion();

    // TODO: What to do with this?
    readBoardConfig();

    // Read any saved temperatures
    savedTemps.clear();
    readRecordedTemps(savedTemps);
    if(savedTemps.size() > 0) {
      // TODO: send an event saying saved temps are available
    }

    // TODO: Need to set this based on data from aggregate
    readSetPoint();
    readTempRange();

    // Then we run the slow update to populate initial values
    updateSlowStatus(intf, false);

    close();
    return true;
  }

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

        // TODO: No limit to range in the docs
        setSetPoint(temp * 0.01);
        setTempRange(range * 0.01);

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
        success |= setDefrostPeriod(newDefrostPeriod);
        success |= setDefrostLength(newDefrostLength);
        success |= setDefrostTempLimit(newDefrostLimit);


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
  
  bool ChillUPSInterface::ProcessMainLoop(SocketInterface &intf) {
    time_t tnow = time(nullptr);

    open();
    bool success = true;
    if((tLastFastUpdate > tnow) ||
       ((tnow - tLastFastUpdate) > chillupsFastUpdateFreq)) {
      success |= updateMainStatus(intf);
      tLastFastUpdate = tnow;
    }

    if((tLastSlowUpdate > tnow) ||
       ((tnow - tLastSlowUpdate) > chillupsSlowUpdateFreq)) {
      success |= updateSlowStatus(intf);
      tLastSlowUpdate = tnow;
    }
    close();
    return success;
  }
  
};
