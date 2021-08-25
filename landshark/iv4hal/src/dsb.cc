
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "debug.hh"
#include "dsb.hh"
#include "support.hh"
#include "rs485.hh"

using namespace iv4;


DSBInterface::DSBInterface(RS485Interface *serial, unsigned int update_rate_s) {
  discoverCountdown = 0;
  tLastUpdate = 0;
  sendEnumEvent = false;
  this->serial = serial;

  if(update_rate_s > 0 && update_rate_s < (60 * 5)) {
    dsbUpdateFreq = update_rate_s;
  } else {
    dsbUpdateFreq = DSB_UPDATE_FREQ;
  }

  // Broadcast a reset to everyone listening at the start of the world -- this will include CUPS
  std::vector<uint8_t> msg {0x00};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::GLOBAL_RESET_TYPE, false, msg)) {
    debug << Debug::Mode::Err << "Failed to broadcast reset during startup" << std::endl;
  }
}

DSBInterface::~DSBInterface() {
}

bool DSBInterface::int_send(uint8_t addr, uint8_t type, bool reading,
                            std::vector<uint8_t> &dat) {
  debug << "Sending: " << std::hex << addr << ":" << std::hex << type << std::dec << std::endl;
  return serial->Send(addr, type, reading, dat);
}

std::unique_ptr<Message> DSBInterface::ProcessMessage(const Message &msg) {
  if(msg.header.type != Message::DSB)
    return Message::MakeNACK(msg, 0, "Invalid message pacssed to DSBInterface");

  bool success = false;
  switch(msg.header.subType) {
  case Message::DSB.SetBootLoaderMode:
    {
      bool mode = (msg.header.imm[0] != 0);
      success = setBootLoaderMode(mode);
    }
    break;
    
  case Message::DSB.Reset:
    {
      success = globalReset();
    }
    break;

  case Message::DSB.SetGlobalLock:
    {
      globalLockState = (msg.header.imm[0] != 0);
      solenoidManualState = (msg.header.imm[1] != 0);
      debug << "Set global lock: " << globalLockState << ":" << solenoidManualState << std::endl;
      success = setGlobalLockState(globalLockState, solenoidManualState);
    }
    break;

  case Message::DSB.DrawerOverride:
    {
      uint8_t index = msg.header.imm[0] & 0x1F;
      bool lock = msg.header.imm[1] != 1;
      success = drawerOverride(index, lock);
    }
    break;

  case Message::DSB.SetFactoryMode:
    {
      factoryModeState = (msg.header.imm[0] != 0);
      success = setFactoryMode(factoryModeState);
    }
    break;

  case Message::DSB.ClearDrawerIndices:
    {
      uint8_t override_val = 0;
      debug << "Clear message: " << std::hex << (msg.header.imm[0] & 0xFFFFFF00) <<
        std::dec << std::endl;
      if((msg.header.imm[0] & 0xFFFFFF00) == 0x4F564400) {
        override_val = msg.header.imm[0] & 0x000000FF;
      }
      success = clearDrawerIndices(override_val);
    }
    break;

  case Message::DSB.AssignDrawerIndex:
    {
      uint8_t ndx = static_cast<uint8_t>(msg.header.imm[0]);
      if(ndx >= 14 || ndx <= 0) {
        return Message::MakeNACK(msg, 0, "Invalid index");
      }
      
      success = assignDrawerIndex(ndx);
    }
    break;

  case Message::DSB.DrawerRecalibration:
    {
      bool save = (msg.header.imm[0] == 1);
      debug << "Drawer recalibration message: " << save << std::endl;
      success = drawerRecalibration(save);
    }
    break;

  case Message::DSB.GetDrawerStates:
    {
      std::vector<uint8_t> payload;
      int numDrawers = 0;
      for(auto dsb : dsbs) {
        for(auto d : dsb.drawers) {
          std::string dnum = std::to_string(d.index);
          for(char c : dnum) payload.push_back(c);
          payload.push_back(':');

          std::string dss = std::to_string(d.solenoidState);
          for(char c : dss) payload.push_back(c);
          payload.push_back(':');

          if(d.open)payload.push_back('1');
          else payload.push_back('0');
          payload.push_back(':');

          std::string dp = std::to_string(d.position);
          for(char c : dp) payload.push_back(c);
          payload.push_back(':');

          std::string dt = std::to_string(dsb.temperature);
          for(char c : dt) payload.push_back(c);
          payload.push_back(':');
          
          std::string ds = std::to_string(dsb.status_byte);
          for(char c : ds) payload.push_back(c);

          payload.push_back('\0');
          numDrawers++;
        } // end for over dsb.drawers
      } // end for over DSBs

      // Send the message
      return std::unique_ptr<Message>(new Message(Message::DSB, Message::DSB.GetDrawerStates,
                                                  numDrawers, 0, 0, 0, payload));
    }
    break;

  case Message::DSB.GetDebugData:
    {
      uint8_t dsb_index = msg.header.imm[0] & 0x1F;

      std::string resp = "";
      std::vector<uint8_t> payload;
      if(getDebugData(dsb_index, resp)) {
        for(char c : resp) payload.push_back(c);
        return std::unique_ptr<Message>(new Message(Message::DSB, Message::DSB.GetDebugData,
                                                    dsb_index, 0, 0, 0, payload));
      } else {
        debug << "Bad response to debug request: " << (int)dsb_index << ":" << std::endl;
      }
    }
    break;

   // -------------------------- End DSB messages -------------------------
  default:
    return Message::MakeNACK(msg, 0, "Unknown DSB message");
  }

  if(!success) return Message::MakeNACK(msg, 0, "Command failed");
  else return Message::MakeACK(msg);
}

bool DSBInterface::Initialize(SocketInterface &intf, bool send) {
  debug << Debug::Info::Time << "Initializing dsb subsystem" << std::endl;

  //bool ret = discover();
  
  return true;
}

uint32_t DSBInterface::Count() const {
  return dsbs.size();
}

uint32_t DSBInterface::GetVersions() const {
  uint32_t vers = 0xFFFFFFFF;

  for(const DSB &dsb : dsbs) {
    vers = ((vers << 8) & 0xFFFFFF00) | (dsb.version & 0x000000FF);
    debug << std::hex << "DSB: 0x" << vers << " :: ";
  }
  debug << std::dec << std::endl;

  return vers;
}


// This will populate the dsbs variable
bool DSBInterface::discover() {
  {
    // LS4 issues a global lock, global solenoid disable, and global proximity disable to all DSB nodes
    //   to ensure they do not report MT99 (on drawer change) during discovery
    std::vector<uint8_t> msg {0x00}; // disallow opening, disable solenoids, disable prox sensors
    if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::GLOBAL_LOCK_TYPE, false, msg)) {
      debug << "Failed to send disable message on discover" << std::endl;
    }
  }

  dsbs.clear();
  
  for(int addr = 1; addr < 14; addr++) {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = addr;
    uint8_t rtype = RS485Interface::DISCOVERY_TYPE;    
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Discovery failed for address " << raddr << std::endl;
      usleep(1000);
      continue;
    }
    
    // Make sure this is the drawer we think it is
    if(raddr != 15) {
      debug << "discovery read address wrong: " << (int)raddr << "::" << 15 << std::endl;
      continue;
    }
    
    // Make sure the message is the correct size
    if(msg.size() != 8) {
      debug << "Message size wrong: " << msg.size() << std::endl;
      continue;
    }
    
    // Make sure this is a discovery response
    if(rtype != RS485Interface::DISCOVERY_RETURN) {
      debug << Debug::Mode::Info << "Discovery return is not correct message type: " <<
        std::hex << (int)rtype << std::dec << std::endl;
      continue;
    }
    
    // Check the address against the device type
    uint8_t dtype = msg[0] & 0x0F;
    if(dtype == 3 || dtype == 2) {
      // DSB two and three drawer
      if(addr >= 14 || addr <= 0) {
        debug << Debug::Mode::Err << "DSB at wrong address: " << (int)addr << std::endl;
        continue;
      }
    } else if(dtype == 7 && addr != 14) { // CUPS
      debug << Debug::Mode::Err << "CUPS at wrong address: " << (int)addr << std::endl;
      continue;
    } else {
      debug << Debug::Mode::Err << "Unknown device type: " << (int)dtype << std::endl;
      continue;
    }
    
    if(dtype == 3 || dtype == 2) {
      // If we are here a DSB responded at address <addr>
      DSB dsb;
      dsb.errors = 0;
      dsb.temperature = 0;
      dsb.status_byte = 0;
      dsb.factoryMode = false;
      dsb.proxStatus = false;
      dsb.solenoidStatus = 0;
      dsb.gunlock = dsb.lunlock = false;
      dsb.proxState = 0xFF;

      dsb.address = addr;
      dsb.bootLoaderMode = ((msg[1] & 0x10) != 0);
      dsb.version = msg[7];

      //uint8_t drawer_count = msg[1] & 0x0F;

      debug << Debug::Mode::Debug << "DSB v." << std::hex << (int)dsb.version << std::dec << " @ " <<
        ((dsb.bootLoaderMode)?(" *"):("  ")) << (int)dsb.address <<
        std::endl;

      // TODO: At the moment we have a 3 drawer max.  This may change
      //  The reason I don't use drawer count here is because the index
      //  may be in the middle of the response
      for(int dn = 0; dn < 3; dn++) {
        uint8_t drawer_ndx = (msg[dn + 2] & 0x1F);

        // Check for an unassigned drawer first
        if(drawer_ndx <= 0 || drawer_ndx >= 0x1F) continue;
        
        DSB::drawer drawer;
        drawer.open = false;
        drawer.position = 0;
        drawer.solenoidState = 0;
        drawer.index = drawer_ndx;
        
        debug << Debug::Mode::Debug << "    Drawer @ " << (int)drawer.index << std::endl;
        dsb.drawers.push_back(drawer);
      }

      dsbs.push_back(dsb);      
    }
  } // Close for over each address
  
  debug << Debug::Mode::Debug << "Discovered " << dsbs.size() << "dsbs" << std::endl;

  {
    std::vector<uint8_t> msg {0x07}; // allow opening, solenoids in auto mode, enable prox sensors
    if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::GLOBAL_LOCK_TYPE, false, msg)) {
      debug << "Failed to send disable message on discover" << std::endl;
    }
  }

  sendEnumEvent = true;
  serial->DumpSerialPortStats();
  
  return true;
}

bool DSBInterface::drawerRecalibration(bool save) {
  uint8_t val = (save) ? (0x02) : (0x01);
  std::vector<uint8_t> msg {val};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::DRAWER_RECALIBRATION_TYPE, false,  msg)) {
    debug << "Failed to send drawer recalibration " << save << " message" << std::endl;
    return false;
  }

  debug << "Sent recalibration message" << std::endl;

  return true;
}

bool DSBInterface::drawerOverride(uint8_t index, bool lock) {
  uint8_t val = index & 0x1F;
  if(!lock) val |= 0x20;
  std::vector<uint8_t> msg {val};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::DRAWER_OVERRIDE_TYPE, false, msg)) {
    debug << "Failed to send drawer override command" << std::endl;
    return false;
  }
  
  return true;
}

bool DSBInterface::setGlobalLockState(bool state, bool manual) {
  uint8_t val = (state) ? (0x02) : (0x03);
  
  if(manual) val |= 0x08; // Manual mode
  else val |= 0x04;    // auto mode
  
  std::vector<uint8_t> msg {val};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::GLOBAL_LOCK_TYPE, false, msg)) {
    debug << "Failed to send set global lock broadcast" << std::endl;
    return false;
  }

  globalLockState = state;
  
  return true;
}

bool DSBInterface::setFactoryMode(bool state) {
  uint8_t val = (state) ? (0x01) : (0x00);
  std::vector<uint8_t> msg {val};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::FACTORY_MODE_TYPE, false, msg)) {
    debug << "Failed to send set factory mode broadcast" << std::endl;
    return false;
  }

  factoryModeState = state;
  
  return true;
}

bool DSBInterface::getDebugData(uint8_t dsb_index, std::string &ret) {
  if(dsb_index >= dsbs.size()) {
    debug << "Index is " << (int)dsb_index << " max is " << dsbs.size() << std::endl;
    ret = std::string("Index is ") + std::to_string( (int)dsb_index) + ".  Max index is " + std::to_string(dsbs.size());
    return true;
  }
  
  uint8_t addr = dsbs[dsb_index].address;

  debug << "Getting debug data" << std::endl;

  struct dats {
    std::string name;
    uint8_t dat;
  };
  dats requests[] = {
    //{"rx_timeout", 1},
    //{"tx_timeout", 2},
    //{"crc_errors", 3},
    //{"framing_errors", 4},

    {"S0_OSC_offset",  6},
    {"S0_OSC_val",     9},
    {"S0_OSC_adj",    12},
    {"S0_DAC_val",    15},
    {"S0_trip_val",   18},
    {"\n",           251},
    {"S1_OSC_offset",  7},
    {"S1_OSC_val",    10},
    {"S1_OSC_adj",    13},
    {"S1_DAC_val",    16},
    {"S1_trip_val",   19},
    {"\n",           251},
    {"S2_OSC_offset",  8},
    {"S2_OSC_val",    11},    
    {"S2_OSC_adj",    14},
    {"S2_DAC_val",    17},
    {"S2_trip_val",   20},
  };
     
  ret = "";
  for(auto dat : requests) {
    if(dat.dat == 251) {
      ret += dat.name;
      continue;
    }
    
    std::vector<uint8_t> msg {dat.dat};
    uint8_t raddr = addr;
    uint8_t rtype = RS485Interface::GET_DEBUG_TYPE;
    RS485Interface::RS485Return sret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(sret != RS485Interface::RS485Return::Success) {
      debug << "Debug failed for address " << raddr << std::endl;
      return false;
    }

    if(rtype != RS485Interface::GET_DEBUG_RETURN) {
      debug << "incorrect return type: " << std::hex << (int)rtype << " != " << (int)RS485Interface::GET_DEBUG_RETURN <<
        std::dec << std::endl;
      return false;
    }
    
    if(msg.size() != 8) {
      debug << "debug response wrong size: " << msg.size() << std::endl;
      return false;
    }
    
    if(msg[0] != dat.dat) {
      debug << "debug response data type incorrect: " << std::hex << (int)msg[0] << " != " <<
        (int)dat.dat << std::dec << std::endl;
      return false;        
    }
    
    int32_t rval =
      ((msg[4] << 24) & 0xFF000000) |
      ((msg[5] << 16) & 0x00FF0000) |
      ((msg[6] <<  8) & 0x0000FF00) |
      ((msg[7]      ) & 0x000000FF);
    
    if(ret != "" && ret[ret.length()-1] != '\n') ret += "      ";
    ret += dat.name + " = " + std::to_string(rval);
  }

  debug << "Debug return: " << ret << std::endl;
  return true;
}

bool DSBInterface::clearDrawerIndices(uint8_t override_val) {
  // LS4 issues a global lock, global solenoid disable, and global proximity disable to all DSB nodes
  //   to ensure they do not report MT99 (on drawer change) during discovery
  std::vector<uint8_t> msg {override_val};
  debug << "Clearing indices: " << (int)override_val << std::endl;
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::CLEAR_INDICES_TYPE, false, msg)) {
    debug << "Failed to send clear indices broadcast" << std::endl;
    return false;
  }

  return true;
}

bool DSBInterface::assignDrawerIndex(uint8_t index) {
  if(index <= 0 || index > 0x1F) return false;

  // Send the index value
  uint8_t val = index & 0x1F;
  std::vector<uint8_t> msg {val};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::ASSIGN_INDEX_TYPE, false, msg)) {
    debug << "Failed to send set index broadcast" << std::endl;
    return false;
  }

  return true;
}

bool DSBInterface::setBootLoaderMode(bool enable) {
  uint8_t val = (enable)?(1):(0);
  std::vector<uint8_t> msg {val};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::BOOTLOADER_MODE_TYPE, false, msg)) {
    debug << "Failed to send bootloader mode broadcast" << std::endl;
    return false;
  }

  return true;
}

bool DSBInterface::globalReset() {
  // Clear everything out
  std::vector<uint8_t> msg {0x00};
  if(!int_send(RS485Interface::BROADCAST_ADDRESS, RS485Interface::GLOBAL_RESET_TYPE, false, msg)) {
    debug << "Failed to send global reset broadcast" << std::endl;
    return false;
  }

  // After waiting TBD seconds, run discover again
  discoverCountdown = time(nullptr) + RESET_DISCOVER_WAIT;

  return true;
}

bool DSBInterface::getDrawerTemps() {
  // For each DSB in dsbs, get the temp
  for(DSB &dsb : dsbs) {
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = dsb.address;
    uint8_t rtype = RS485Interface::GET_TEMP_TYPE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Get drawer temps failed for address " << raddr << std::endl;
      return false;
    }
    
    // Got the return message, so parse it
    if(raddr != 15) {
      debug << "Read temp return wrong address: " << raddr << std::endl;
      continue;
    }
    
    if(rtype != RS485Interface::GET_TEMP_RETURN) {
      debug << "Wrong type " << rtype << " in get temp return" << std::endl;
      continue;
    }
    
    if(msg.size() != 2) {
      debug << "get temp returned " << msg.size() << " bytes" << std::endl;
      continue;
    }
    
    dsb.temperature = static_cast<char>(msg[0]);
    dsb.voltage = static_cast<uint8_t>(msg[1]);
  }        

  return true;
}

bool DSBInterface::getDrawerStatus() {
  // For each DSB in dsbs:
  for(DSB &dsb : dsbs) {
    std::vector<uint8_t> msg {0};
    uint8_t raddr = dsb.address;
    uint8_t rtype = RS485Interface::GET_STATUS_TYPE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Get drawer status failed for address " << raddr << std::endl;
      return false;
    }
    
    // Got the return message, so parse it
    if(raddr != 15) {
      debug << "Read staus return wrong address: " << raddr << std::endl;
      continue;
    }
    
    if(rtype != RS485Interface::GET_STATUS_RETURN) {
      debug << "Wrong type " << rtype << " in get status return" << std::endl;
      continue;
    }
    
    if(msg.size() != 8) {
      debug << "get status returned " << msg.size() << " bytes" << std::endl;
      continue;
    }
    
    debug << "Status message: " <<
      std::hex <<
      (int)msg[0] << ":" <<
      (int)msg[1] << ":" <<
      (int)msg[2] << ":" <<
      (int)msg[3] << ":" <<
      (int)msg[4] << ":" <<
      (int)msg[5] << ":" <<
      (int)msg[6] << ":" <<        
      (int)msg[7] <<
      std::dec << std::endl;
    
    for(unsigned int curr = 0; curr < dsb.drawers.size(); curr++) {        
      DSB::drawer &d = dsb.drawers[curr];
      
      // Find the drawer in the repsonse
      bool found = false;
      for(int im = 0; im < 3; im++) {
        uint8_t which = 2*im;
        uint8_t ndxm = msg[which] & 0x1F;
        if(d.index == ndxm) {
          found = true;
          d.solenoidState = (msg[which+1] >> 6) & 0x03;
          d.open = (((msg[which+1]) & 0x20) != 0);
          d.position = (msg[which+1] & 0x0F);
          debug << "      @ " << (int)d.index << ":" << (int) d.solenoidState << ":" <<
            (int)d.open << ":" << (int)d.position << std::endl;
        }
      } // end for over response
      if(!found) {
        debug << Debug::Mode::Err << "Did not find drawer " << (int)d.index << " in MT83 repsonse" << std::endl;
      }
    } // end for over drawers
    
    dsb.status_byte = msg[7];
    
    dsb.errors         = (msg[7]       & 0x01) != 0;
    dsb.factoryMode    = (msg[7]       & 0x02) != 0;
    dsb.proxStatus     = (msg[7]       & 0x04) != 0;
    dsb.solenoidStatus = (msg[7] >> 3) & 0x03;
    dsb.gunlock        = (msg[7]       & 0x40) != 0;
    dsb.lunlock        = (msg[7]       & 0x80) != 0;
    
    debug << "DSB status -- " << 
      (int)dsb.errors          << ":" <<
      (int)dsb.factoryMode     << ":" <<
      (int)dsb.proxStatus      << ":" <<
      (int)dsb.solenoidStatus  << ":" <<
      (int)dsb.gunlock         << ":" <<
      (int)dsb.lunlock         << std::endl;
    
  } // end for over DSBs

  return true;
}

bool DSBInterface::getErrors(DSB &dsb, std::vector<uint8_t> &errors) {
  // Get the errors from a single DSB.  This clears the error log
    std::vector<uint8_t> msg {0x00};
    uint8_t raddr = dsb.address;
    uint8_t rtype = RS485Interface::GET_ERRORS_TYPE;
    RS485Interface::RS485Return ret = serial->SendAndReceive(raddr, rtype, true, msg, DEFAULT_TIMEOUT);
    if(ret != RS485Interface::RS485Return::Success) {
      debug << "Get errors failed for address " << raddr << std::endl;
      return false;
    }
  
    // Got the return message, so parse it
    if(raddr != 15) {
      debug << "Read errors return wrong address: " << raddr << std::endl;
      return false;
    }
    
    if(rtype != RS485Interface::GET_ERRORS_RETURN) {
      debug << "Wrong type " << rtype << " in get errors return" << std::endl;
      return false;
    }
    
    if(msg.size() != 4) {
      debug << "get errors returned " << msg.size() << " bytes" << std::endl;
      return false;
    }
    
    errors.clear();

    // Parse the message
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

  return true;
}
  
  
  
bool DSBInterface::ReceiveDrawerEvent(std::vector<uint8_t> &msg) {
  DrawerEvent evt;
  evt.index = msg[0] & 0x1F;
  evt.solenoid = (msg[1] >> 6) & 0x03;
  evt.position = (msg[1] & 0x0F);
  evt.open = ( (msg[1] & 0x20) != 0);
  evt.event = ((msg[1] & 0x10) == 0); // In the message, 0 is unlock, 1 is lock
  events.push_back(evt);
  debug << " *** DSB drawer event *** :: " << evt.index << " " << evt.solenoid << " " <<
    evt.position << " " << evt.open << " " << evt.event << std::endl;

  return true;
}

bool DSBInterface::SelfAssignEvent() {
  discoverCountdown = time(nullptr) + RESET_DISCOVER_WAIT;
  return true;
}

bool DSBInterface::ProcessMainLoop(SocketInterface &intf, bool initialized, bool send) {
  time_t tnow = time(nullptr);

  // First, check to see if we are awaiting on a reset discovery
  if(discoverCountdown > 0) {
    if((tnow > discoverCountdown)) {
      // We have passed the threshold to perform a discovery
      discover();
      discoverCountdown = 0;
      // Then we fall through to do an update
      tLastUpdate = 0;
    } else {
      return true;
    }
  }

  if(!initialized) return true;
  
  // Check to see if we need to do an update
  if((tLastUpdate > tnow) ||
     ((tnow - tLastUpdate) > dsbUpdateFreq)) {
    getDrawerStatus();
    // The drawer is supposed to send us status changes as broadcast messages,
    //  but we will check here to see if there are any errors
    for(DSB &dsb : dsbs) {
      if(dsb.errors) {
        debug << "DSB " << dsb.address << " had errors" << std::endl;
        // Get the errors and send them as an event
        std::vector<uint8_t> errs;
        if(getErrors(dsb, errs)) {
          // Put all the errors into the payload
          std::vector<uint8_t> payload;
          for(uint8_t e : errs) {
            std::string es = std::to_string(e);
            for(auto c : es) payload.push_back(c);
            payload.push_back('\0');
          }
          Message msg(Message::DSB, Message::DSB.DrawerErrors,
                      dsb.address, errs.size(), 0, 0, payload);
          debug << "Sending dsb errors as an event " << std::endl;
          intf.Send(msg);
        } else {
          debug << "Failed to get errors for dsb " << dsb.address << std::endl;
        }
      }
    }
    tLastUpdate = time(nullptr);
  }

  // If there are broadcast messages send them now
  if(send) {
    if(events.size() > 0) debug << "DSB Sending " << events.size() << " events" << std::endl;
    for(DrawerEvent evt : events) {
      Message msg(Message::DSB, Message::DSB.DrawerStateChanged,
                  evt.index, evt.solenoid, evt.event, evt.position);
      debug << "Sending dsb drawer event" << std::endl;
      intf.Send(msg);      
    }

    // Clear out the events so we don't sent them more than once
    events.clear();

    if(sendEnumEvent) {
      // Send an enumeration event
      std::vector<uint8_t> payload;
      int num_dsbs = 0;
      
      for(DSB &d: dsbs) {
        payload.push_back(d.address);
        payload.push_back(d.version);
        for(unsigned int ii = 0; ii < 5; ii++) {
          if(ii < d.drawers.size()) payload.push_back(d.drawers[ii].index);
          else payload.push_back(0xFF);          
        }
        payload.push_back(0);
        num_dsbs++;
      }
      Message msg(Message::DSB, Message::DSB.EnumerationEvent,
                  num_dsbs, 0, 0, 0, payload);
      debug << "Sending Enumeration event" << std::endl;
      intf.Send(msg);

      sendEnumEvent = false;
    }
  }
  return true;  
}

