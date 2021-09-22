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
  ChillUPSInterface::ChillUPSInterface(RS485Interface *serial)
    : serial(serial) {
    tLastFastUpdate = 0;
    tLastSlowUpdate = 0;
    cupsResetting = false;

    lastMainStatusRead = false;

    chillupsFastUpdateFreq = (FAST_UPDATE_FREQ_S);
    chillupsSlowUpdateFreq = (SLOW_UPDATE_FREQ_S);
  }

  ChillUPSInterface::~ChillUPSInterface() {
  }


  // -----------------------------------------------------------------
  // -----------------------------------------------------------------
  //  Start functions for reading chillups status
  // -----------------------------------------------------------------
  // -----------------------------------------------------------------
  
  bool ChillUPSInterface::getMainStatus(uint8_t &status_reg, bool &error_bit) {
    bool success = false;
    
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_STATUS;
    std::vector<uint8_t> msg {0x00};
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success || msg.size() < 2) {
      debug << Debug::Mode::Err << "Failed to get status" << msg.size() << std::endl;
      return false;
    } else {
      debug << "Status return ======== " << std::hex << (int)msg[0] << ":" <<
        (int)msg[1] << std::dec << std::endl;
      status_reg = msg[0];
      error_bit = (msg[1] & 0x80) !=0;
      success = true;
    }
    
    return success;
  }

  bool ChillUPSInterface::getCUPSErrorCodes(std::vector<uint8_t> &errors) {
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::DSB_GET_ERRORS;
    std::vector<uint8_t> msg {0x00};
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success || msg.size() < 4) {
      debug << "Failed to get cups errors :: wrong size" << std::endl;
      return false;
    } else {
      uint8_t num_errors = msg[0] & 0x0F;
      for(unsigned int i = 0; i < msg.size(); i++) {
        uint8_t err1 = msg[i] & 0x0F;
        uint8_t err2 = (msg[i] >> 4) & 0x0F;
        
        // The first error message is combined with the count
        if(i != 0) errors.push_back(err1);
        if(errors.size() == num_errors) break;
        errors.push_back(err2);
        if(errors.size() == num_errors) break;
      } // end for over errors message payload
    }
    
    return true;
  }
  
  bool ChillUPSInterface::getErrorCode(uint8_t &error_code) {
    bool success = false;
    
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_COMPR_ERROR;
    std::vector<uint8_t> msg {0x00};
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success || msg.size() < 1) {
      return false;
    } else {
      error_code = msg[0] & 0x0F;;
      success = true;
    }
    
    return success;
  }

  bool ChillUPSInterface::readTemperatures() {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_TEMPERATURE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read temperatures failed for address " << (int)raddr << std::endl;
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

    debug << "Read temps: " << lastThermistorTemp << ":" << lastCalibratedColdCubeTemp << ":" <<
      lastCalibratedAmbientTemp << std::endl;

    return true;
  }

  bool ChillUPSInterface::readVoltages() {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_VOLTAGE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read voltages failed for address " << (int)raddr << std::endl;
      return false;
    }

    if(rtype != RS485Interface::CUPS_GET_VOLTAGE_RETURN) {
      debug << Debug::Mode::Err << "read voltage returned wrong type: " << (int)rtype << std::endl;
      return false;
    }

    lastChargePercent = msg[0];
    lastPSU1Voltage = msg[1] * 0.1f;
    lastPSU2Voltage = msg[2] * 0.1f;
    lastPSU3Voltage = msg[3] * 0.1f;

    lastBatteryVoltage = msg[4] * 0.1f;
    lastChargerVoltage = msg[5] * 0.1f;
    lastCompressorVoltage = msg[6] * 0.1f;
    
    
    return true;
  }

  bool ChillUPSInterface::readPersistentSettings() {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_PSETTINGS;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read psettings failed for address " << (int)raddr << "::" << (int)ret << std::endl;
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

  bool ChillUPSInterface::readDynamicSettings() {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_GET_DSETTINGS;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "read dsettings failed for address " << (int)raddr << "::" << (int)ret << std::endl;
      return false;
    }

    if(rtype != RS485Interface::CUPS_GET_DSETTINGS_RETURN) {
      debug << Debug::Mode::Err << "read dsettings returned wrong type: " << (int)rtype << std::endl;
      return false;
    }

    currentDynamicSettings.parseByte(msg[0]);
    return true;
  }

  bool ChillUPSInterface::setDynamicSettings() {
    std::vector<uint8_t> msg {currentDynamicSettings.toByte(), 0x00};
    
    if(!serial->Send(RS485Interface::CUPS_ADDRESS,
                 RS485Interface::CUPS_SET_DSETTINGS, false, msg)) {
      debug << "Failed to set dsettings" << std::endl;
    } else {
      debug << "Set dsettings to " << std::hex << (int)currentDynamicSettings.toByte() << std::dec << std::endl;
    }

    return true;
  }

 
  bool ChillUPSInterface::setDefrostSettings(uint16_t period, uint8_t length, uint16_t limit) {
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
  }
  
  ChillUPSInterface::chillups_id  ChillUPSInterface::readColdCubeID(){
    chillups_id id;
    
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
    
    return id;
  }
  
  ChillUPSInterface::chillups_id  ChillUPSInterface::readAmbientID(){
    chillups_id id;

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
      
    return id;
  }
  
  
  bool ChillUPSInterface::readRecordedTemps(std::vector<std::tuple<int, float> > &temps){
    bool done = false;
    while(!done) {

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
        debug << " ________ stored temp: " << ndx << ":" << temp << std::endl;
        if(ndx == 0 && temp == 0) {
          // all done
          break;
        }
        
        temps.push_back(std::make_tuple(ndx, temp * 0.01f));
    }

    return true;
  }

  bool ChillUPSInterface::updateMainStatus(SocketInterface &intf, bool send) {
    // Read the status register
    uint8_t mainStatus_reg;
    bool error_bit;
    bool status_ret = getMainStatus(mainStatus_reg, error_bit);
    debug << "Got a status message ******** " << std::hex << (int)mainStatus_reg << " " <<
      error_bit << std::dec << std::endl;
    if(status_ret) {
      cupsStatus mainStatus = cupsStatus(mainStatus_reg);

      if(!mainStatus.bootACK) {
        // acknowledge CUPS boot
        debug << "CUPS rebooted!!" << std::endl;
        uint8_t newStatusByte;
        acknowledgeBoot(newStatusByte, error_bit);
        mainStatus = cupsStatus(newStatusByte);
        debug << "Got an ACK status message ******** " << std::hex << (int)mainStatus_reg << " " <<
          error_bit << std::dec << std::endl;
      }

      if(send && error_bit) {
        debug << "Error bit was set" << std::endl;
        // Get the errors
        std::vector<uint8_t> errors;
        getCUPSErrorCodes(errors);
        
        std::vector<uint8_t> payload;
        for(uint8_t e : errors) {
          std::string es = std::to_string(e);
          for(auto c : es) payload.push_back(c);
          payload.push_back('\0');
        }
        Message msg(Message::CUPS, Message::CUPS.Failure,
                    0, errors.size(), 0, 0, payload);
        debug << "Sending cups errors as an event " << std::endl;
        intf.Send(msg);
      }
      
      debug << "Main status update " << std::hex <<
        (int)lastMainStatus._reg << "::" << (int)mainStatus._reg << std::dec << std::endl;
      if(mainStatus != lastMainStatus) {
        // Status changed, so we have to send a message.  But first we have to
        //  check on some things

        debug << "AC State: " << mainStatus.acStatus << "::" << lastMainStatus.acStatus << std::endl;
        if(send && (mainStatus.acStatus != lastMainStatus.acStatus)) {
          // Update our temps and voltages when ac state changes
          updateSlowStatus(intf, send);
          debug << "AC Status changed" << std::endl;
          Message msg(Message::CUPS, Message::CUPS.ACStateChanged,
                      mainStatus.acStatus?1:0,
                      0,0,0);
          intf.Send(msg);
        }
        
        if(send && !mainStatus.firmwareState) {
          Message msg(Message::CUPS, Message::CUPS.Failure,
                      1,0,0,0);
          intf.Send(msg);            
        }
        
        if(send && !mainStatus.comprOK) {
          uint8_t compr_error;
          if(getErrorCode(compr_error)) {
            Message msg(Message::CUPS, Message::CUPS.CompressorError,
                        compr_error,0,0,0);
            intf.Send(msg);
          } else {
            debug << Debug::Mode::Err << "Failed ot get error code for cups" << std::endl;
          }
        }

        debug << "Main status update **** " << std::hex << (int)lastMainStatus._reg << "::" << (int)mainStatus._reg << std::dec << std::endl;
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

    readTemperatures();
    readVoltages();
    
    readPersistentSettings();
    readDynamicSettings();
    
    // Just append to recorded temperatures.  Don't clear it
    success |= readRecordedTemps(savedTemps);

    return success;
  }
  

  bool ChillUPSInterface::Initialize(SocketInterface &intf) {

    savedTemps.clear();
    if(serial != nullptr) {
      debug << "Running CUPS discovery" << std::endl;
      bool discover_success = discover();
      if(!discover_success) {
        debug << Debug::Mode::Err << "Failed to discover chillups" << std::endl;
        return false;
      }
    }
    
    // First we get the main status, but don't send anything yet
    if(!updateMainStatus(intf, false)) {
      return false;
    }

    if(serial != nullptr && !lastMainStatus.bootACK) {
      // acknowledge CUPS boot
      uint8_t status_byte;
      bool error_bit = false;
      acknowledgeBoot(status_byte, error_bit);
      lastMainStatus = cupsStatus(status_byte);

      if(!lastMainStatus.bootACK) {
        debug << Debug::Mode::Info << "Boot ACK failed" << std::endl;
      }
    }
    
    if(!updateSlowStatus(intf, false)) {
      return false;
    }
    
    if(!lastMainStatus.firmwareState) {
      // TODO: This is a catastrophic failure.  Send a message
    }

    if(!lastMainStatus.acStatus) {
      // TODO: Turn off leds and send a message
    }

    if(!lastMainStatus.comprOK) {
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

    readDynamicSettings();
    currentDynamicSettings.log_enable = true;
    currentDynamicSettings.auto_chill = true;
    setDynamicSettings();

    // Read any saved temperatures
    readRecordedTemps(savedTemps);
    if(savedTemps.size() > 0) {
      // TODO:? send an event saying saved temps are available
      //  Currently this is handled when the client requests status.  All the saved temps will be sent then
      //  There is no message for temps available
      //  Current plan is to leave it that way

      debug << Debug::Mode::Info << "We have " << savedTemps.size() << " stored temps to send" << std::endl;
    }

    // Then we run the slow update to populate initial values
    updateSlowStatus(intf, false);

    return true;
  }

  std::string NibbleToHex(uint8_t nibble) {
    std::string ret = "";
    if(nibble >= 0 && nibble <= 9) ret.push_back((char)(nibble + '0'));
    else if(nibble >= 0x0A && nibble <= 0x0F) ret.push_back((char)((nibble-0x0A) + 'A'));
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

    switch(m.header.subType) {

    case Message::CUPS.Reset:
      {
        debug << "Resetting CUPS" << std::endl;
        globalReset();
      }
      break;

    case Message::CUPS.Refresh:
      {
        debug << "Refreshing CUPS" << std::endl;
        readTemperatures();
        readVoltages();
      }
      break;
      
    case Message::CUPS.SetTemperature:
      {
        int16_t temp = static_cast<int16_t>(m.header.imm[0]);
        int16_t range = static_cast<int16_t>(m.header.imm[1]);

        if(temp < -2000 || temp > 4000) {
          return Message::MakeNACK(m, 0, "Temperature out of range");
        }

        // TODO: No limit to range in the docs - determine what a good limit is
        if(serial != nullptr) {
          setTemperature(temp, range);
          usleep(2500);
          readPersistentSettings();
        }
        
        debug << "Set temp to " << temp << " +- " << range << std::endl;
      }
      break;

    case Message::CUPS.GetTemperature:
      {
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

        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetAllTemperatures,
                                                    count, 0, 0, 0,
                                                    payload));
      }
      break;
      
    case Message::CUPS.SetFans:
      {
        readDynamicSettings();
        if(m.header.imm[0] == 1) currentDynamicSettings.large_fan = true;
        else if(m.header.imm[0] == 0) currentDynamicSettings.large_fan = false;
        if(m.header.imm[1] == 1) currentDynamicSettings.small_fan = true;
        else if(m.header.imm[1] == 0) currentDynamicSettings.small_fan = false;
        if(m.header.imm[2] == 1) currentDynamicSettings.ambient_fan = true;
        else if(m.header.imm[2] == 0) currentDynamicSettings.ambient_fan = false;
        
        setDynamicSettings();
        usleep(1000);
        readDynamicSettings();
      }
      break;

    case Message::CUPS.SetAuto:
      {        
        readDynamicSettings();
        if(m.header.imm[0] > 0) currentDynamicSettings.auto_chill = true;
        else currentDynamicSettings.auto_chill = false;
        if(m.header.imm[1] > 0) currentDynamicSettings.auto_defrost = true;
        else currentDynamicSettings.auto_defrost = false;

        setDynamicSettings();
        usleep(1000);
        readDynamicSettings();
      }
      break;

    case Message::CUPS.SetLEDBackup:
      {
        readDynamicSettings();
        if(m.header.imm[0] > 0) currentDynamicSettings.led_backup = true;
        else currentDynamicSettings.led_backup = false;
        
        setDynamicSettings();
        usleep(1000);
        readDynamicSettings();
      }
      break;

    case Message::CUPS.LogEnable:
      {
        readDynamicSettings();
        if(m.header.imm[0] > 0) currentDynamicSettings.log_enable = true;
        else currentDynamicSettings.log_enable = false;
        
        setDynamicSettings();
        usleep(1000);
        readDynamicSettings();
      }
      break;

    case Message::CUPS.SetDefrostSettings:
      {
        if(m.header.imm[0] > 65536) {
          return Message::MakeNACK(m, 0, "Defrost period out of range");
        }
        uint16_t newDefrostPeriod = m.header.imm[0];

        if(m.header.imm[1] > 255) {
          return Message::MakeNACK(m, 0, "Defrost length out of range");
        }
        uint8_t newDefrostLength = m.header.imm[1];

        int val2 = m.header.imm[2];
        float newDefrostLimit = val2 * 0.01;
        if(newDefrostLimit < -20.0 || newDefrostLimit > 40.0) {
          return Message::MakeNACK(m, 0, "Defrost limit out of range");
        }

        bool success = true;
        if(serial != nullptr) {
          success = setDefrostSettings(newDefrostPeriod, newDefrostLength, newDefrostLimit*100);
          usleep(2500);
          readPersistentSettings();
        }

        if(!success) {
          return Message::MakeNACK(m, 0, "Failed to set defrost settings");
        }
      }
      break;
    case Message::CUPS.GetDefrostSettings:
      {
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetDefrostSettings,
                                                    lastDefrostPeriod,
                                                    lastDefrostLength, 
                                                    static_cast<int>(lastDefrostTempLimit * 100),
                                                   0));
      }
      break;
      
    case Message::CUPS.InitiateDefrost:
      {
        bool success = startDefrost();
        if(!success) return Message::MakeNACK(m, 0, "Failed to initiate defrost");
      }
      break;
      
    case Message::CUPS.InitiateBatteryTest  :
      {
        bool success = startBatteryTest();
        
        if(!success) {
          return Message::MakeNACK(m, 0, "Failed to initiate battery test");
        }
      }
      break;
      
    case Message::CUPS.GetAllVoltages:
      {
        int count = 0;
        std::vector<uint8_t> payload;
        {
          std::string name("psu1");
          std::string val = std::to_string(lastPSU1Voltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        {
          std::string name("psu2");
          std::string val = std::to_string(lastPSU2Voltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        {
          std::string name("psu3");
          std::string val = std::to_string(lastPSU3Voltage);
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
          std::string name("charger");
          std::string val = std::to_string(lastChargerVoltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }
        {
          std::string name("compressor");          
          std::string val = std::to_string(lastCompressorVoltage);
          for(auto c : name) payload.push_back(c);
          payload.push_back(':');
          for(auto c : val) payload.push_back(c);
          payload.push_back('\0');
          count++;
        }

        debug << "Returning " << count << " voltages" << std::endl;
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetAllVoltages,
                                                    count, 0, 0, 0,
                                                    payload));        
      }
      break;

    case Message::CUPS.GetBatteryPercent:
      {
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetBatteryPercent,
                                                    static_cast<int>(lastChargePercent),
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
        std::string ccid_s = std::string("coldcube_") + IdToString(ccid);
        for(auto c : ccid_s) {
          payload.push_back(c);
        }
        payload.push_back('\0');
        
        std::string aid_s = std::string("ambient_") + IdToString(aid);
        for(auto c : aid_s) {
          payload.push_back(c);
        }
        payload.push_back('\0');
        
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetProbeIDs,
                                                    2, 0, 0, 0,
                                                    payload));
      }
      break;


    case Message::CUPS.CompressorError:
      {
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.CompressorError,
                                                    static_cast<int>(lastCompressorError),
                                                    0, 0, 0));        
      }
      break;

    case Message::CUPS.GetDynamicSettings:
      {
        debug << "Getting dynamic settings " << std::hex << (int)lastMainStatus._reg << std::dec << std::endl;
        return std::unique_ptr<Message>(new Message(Message::CUPS, Message::CUPS.GetDynamicSettings,
                                                    static_cast<int>(currentDynamicSettings.toByte()),
                                                    lastMainStatus._reg,
                                                    0,0));
                                                                     
      }
      break;
      
    default:
      {
        return Message::MakeNACK(m, 0, "Invalid CUPS message subtype");
      }

          
    }

    return Message::MakeACK(m);
  }

  bool ChillUPSInterface::acknowledgeBoot(uint8_t &status_byte, bool &error_bit) {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_INITIATE_OPERATION;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, false, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Boot ack failed for address " << (int)raddr << std::endl;
      usleep(1000);
      return false;
    }
    
    // Make sure the message is the correct size
    if(msg.size() < 1) {
      debug << "Message size wrong for boot ack: " << msg.size() << std::endl;
      return false;
    }
    
    // Make sure this is a discovery response
    if(rtype != RS485Interface::CUPS_GET_STATUS_RETURN) {
      debug << Debug::Mode::Info << "boot ack return is not correct message type: " <<
        std::hex << (int)rtype << std::dec << std::endl;
      return false;
    }

    debug << "Got a status message ******** " << std::hex << (int)msg[0] << std::dec << std::endl;
    status_byte = msg[0];
    error_bit = (msg[1]&0x80) != 0;

    return true;
  }

  
  bool ChillUPSInterface::startDefrost() {
    std::vector<uint8_t> msg {0x01};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_INITIATE_OPERATION;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, false, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "start defrost failed for address " << (int)raddr << std::endl;
      usleep(1000);
      return false;
    }
    return true;
  }

  bool ChillUPSInterface::startBatteryTest() {
    std::vector<uint8_t> msg {0x02};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::CUPS_INITIATE_OPERATION;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, false, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "start battery test failed for address " << (int)raddr << std::endl;
      usleep(1000);
      return false;
    }
    return true;
  }

  bool ChillUPSInterface::globalReset() {
      std::vector<uint8_t> msg {0x00};
      if(!serial->Send(RS485Interface::CUPS_ADDRESS, RS485Interface::CUPS_RESET, false, msg)) {
        debug << "Failed to send global reset broadcast" << std::endl;
        return false;
      }

      cupsResetting = true;
      return true;
  }

  // This will populate the dsbs variable
  bool ChillUPSInterface::discover() {
    
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = RS485Interface::CUPS_ADDRESS;
    uint8_t rtype = RS485Interface::DISCOVERY;    
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Discovery failed for address " << (int)raddr << std::endl;
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
    currentBoardConfig.calColdCube = calColdPresent;
    currentBoardConfig.calAmbient = calAmbiPresent;
    currentBoardConfig.id = (msg[1] & 0x0F);

    debug << Debug::Mode::Info <<
      "CUPS discovered -" <<
      " id: " << std::hex << (int)(msg[1] & 0x0F) << std::dec << 
      " ver: " << (int)currentVersion.major << "." << (int)currentVersion.minor << 
      " Calibrated Cold:" << (calColdPresent?"*":" ") <<
      " Calibrated Ambient:" << (calAmbiPresent?"*":" ") <<
      std::endl;
    return true;
  }

  bool ChillUPSInterface::ProcessMainLoop(SocketInterface &intf) {
    time_t tnow = time(nullptr);
    if(tnow >= (cupsResettingTime + 2)) {
      cupsResetting = false;
    } else {
      return true;
    }

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
    return success;
  }
  
};
