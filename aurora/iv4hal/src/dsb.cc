
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "debug.hh"
#include "dsb.hh"

using namespace iv4;

static int setupFD(std::string &file) {
  struct termios   options;
  int rc;
  
  int fd = open(file.c_str(), O_RDWR | O_NOCTTY);
  if(fd < 0) {
    debug << Debug::Mode::Failure << "Failed to open RS-485 device: " << file << std::endl;
    debug << Debug::Mode::Failure << strerror(errno) << std::endl;
    return -1;
  }

  if((rc = tcgetattr(fd, &options)) < 0){
    debug << Debug::Mode::Err << "Failed to get RS485 options" << std::endl;
    return -1;
  }
  
  // Set the baud rates to 115200
  cfsetispeed(&options, B115200);
  
  // Set the baud rates to 115200
  cfsetospeed(&options, B115200);

  cfmakeraw(&options);
  options.c_cflag |= (CLOCAL | CREAD);   // Enable the receiver and set local mode
  options.c_cflag &= ~CSTOPB;            // 1 stop bit
  options.c_cflag &= ~CRTSCTS;           // Disable hardware flow control
  options.c_cc[VMIN]  = 1;
  options.c_cc[VTIME] = 2;

  // Set the new attributes
  if((rc = tcsetattr(fd, TCSANOW, &options)) < 0){
    debug << Debug::Mode::Err << "Failed to set RS485 options" << std::endl;
    return -1;
  }

  debug << "Set up RS-485" << std::endl;
  return fd;
}

DSBInterface::DSBInterface(std::string dev, unsigned int update_rate_s) {
  discoverCountdown = 0;
  tLastUpdate = 0;
  checkCount = 0;

  if(update_rate_s > 0 && update_rate_s < (60 * 5))
    dsbUpdateFreq = update_rate_s;
  else
    dsbUpdateFreq = DSB_UPDATE_FREQ;
  
  _dev = dev;
  devFD = setupFD(_dev);
}

DSBInterface::~DSBInterface() {
  if(devFD >= 0) close(devFD);
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
      success = setGlobalLockState(globalLockState);
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
      success = clearDrawerIndices();
    }
    break;

  case Message::DSB.AssignDrawerIndex:
    {
      uint8_t ndx = static_cast<uint8_t>(msg.header.imm[0]);
      if(ndx > 13 || ndx < 1) {
        return Message::MakeNACK(msg, 0, "Invalid index");
      }
      
      success = assignDrawerIndex(ndx);
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

  case Message::DSB.UpdateFirmware:
    {
      std::string path(msg.payload.begin(), msg.payload.end());
      success = updateFirmware(path);
    }
    break;

  default:
    return Message::MakeNACK(msg, 0, "Unknown DSB message");
  }

  if(!success) return Message::MakeNACK(msg, 0, "Command failed");
  else return Message::MakeACK(msg);
}

bool DSBInterface::Initialize(SocketInterface &intf, bool send) {
  debug << "Initializing dsb subsystem" << std::endl;
  bool ret = discover();

  return ret;
}

bool DSBInterface::ProcessMainLoop(SocketInterface &intf, bool send) {
  time_t tnow = time(nullptr);

  // First, check to see if we are awaiting on a reset discovery
  if(discoverCountdown > 0) {
    if((tnow >= discoverCountdown)) {
      // We have passed the threshold to perform a discovery
      discover();
      discoverCountdown = 0;
      // Then we fall through to do an update
      tLastUpdate = 0;
    } else {
      // We are waiting to do a discover following a reset, so
      //  we don't want to try and talk to anything yet, just return
      return true;
    }
  }
  
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
  } else if((checkCount++ % 10) == 0) {
    // If we don't have to do an update:
    // get a message (with a very small timeout) to check to see if there are any
    //  pending messages
    std::vector<uint8_t> rmsg;
    uint8_t raddr, rtype;
    if(!recv(raddr, rtype, rmsg, 1)) {
      // Timed out, no big deal
      //  debug << "Update loop read timed out" << std::endl;
    }
  }

  // If there are broadcast messages send them now
  if(send) {
    if(events.size() > 0) debug << "Sending " << events.size() << " events" << std::endl;
    for(DrawerEvent evt : events) {
      Message msg(Message::DSB, Message::DSB.DrawerStateChanged,
                  evt.index, evt.solenoid, evt.event, evt.position);
      debug << "Sending dsb drawer event" << std::endl;
      intf.Send(msg);      
    }

    // Clear out the events so we don't sent them more than once
    events.clear();
  }
  return true;  
}

uint32_t DSBInterface::Count() const {
  return dsbs.size();
}

uint32_t DSBInterface::GetVersions() const {
  uint32_t vers = 0xFFFFFFFF;

  for(const DSB &dsb : dsbs) {
    vers = ((vers << 8) & 0xFFFFFF00) | (dsb.version & 0x000000FF);
    debug << std::hex << "0x" << vers << " :: ";
  }
  debug << std::dec << std::endl;

  return vers;
}

bool DSBInterface::send(uint8_t addr, uint8_t type,
                        bool read, std::vector<uint8_t> dat) {
  // address 31 is a broadcast  
  bool bcast = (addr == 31);

  int msg_size = dat.size();
  
  // Sanity check type here - then add it to the front of the message
  // TODO: Sanity check
  dat.insert(dat.begin(), type);

  // Put the starting byte together
  //  7  : R/W (R = 1)
  //  6:5: Length (0=1, 1=2, 2=4)
  //  4:0: Address
  // For the length, if we are trying to send three bytes, pad it to 4 with a zero
  uint8_t start = ((read)?(0x80) : (0x00)) | (addr & 0x1F);
  switch(msg_size) {
  case 1: break;
  case 2: start |= 0x20; break;
  case 3: start |= 0x40; dat.push_back(0x00); break;
  case 4: start |= 0x40; break;
  default:
    debug << Debug::Mode::Err << "Tried to call send with data of size " << dat.size() << std::endl;
    return false;
  }
  dat.insert(dat.begin(), start);


  // add CRC
  dat.push_back(0x00);

  debug << "DSB sending: addr:" << (int)addr << " type:" << (int)type <<
    " read:" << (int)read << " data:";
  debug << " <--> ";
  for(unsigned int q = 0; q < dat.size(); q++) {
    debug << std::hex << (int)dat[q] << "," << std::dec;
  }
  debug << std::endl;

  // Send the message
  //  Broadcast messages are always sent three times
  int count = (bcast) ? 3 : 1;
  while(count > 0) {
    int ret = write(devFD, dat.data(), dat.size());

    if(ret < 0) {
      debug << Debug::Mode::Err << "Failed to write DSB message: " << strerror(errno) <<
        " :: try(" << count << "): addr: " << std::hex << (int)addr << " type: " << type <<
        std::dec << std::endl;
      return false;
    }
    if((--count) <= 0) break;

    // Sleep between 5 and 20ms between each broadcast message
    int delay = (rand() % 15) + 5;
    usleep(delay * 1000);
  }

  return true;
}

bool DSBInterface::recv(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
                        int timeoutMS) {
  int bcount = 0;

  // If we get too many broadcasts return, so that we can process the servers and send images
  while(bcount < 50) {
    msg.clear();
    bool ret = _recv(addr, type, msg, timeoutMS);

    if(ret && (addr == BROADCAST_ADDRESS)) {
      // We received a broadcast - queue it up
      if(type == DRAWER_STATE_CHANGE_EVENT) {
        if(msg.size() != 2) {
          debug << Debug::Mode::Err << "drawer state change broadcast wrong size: " << msg.size() << std::endl;
          continue;
        }
        // This is a drawer event
        DrawerEvent evt;
        evt.index = msg[0] & 0x0F;
        evt.solenoid = (msg[1] >> 6) & 0x03;
        evt.position = (msg[1] & 0x0F);
        evt.open = ( (msg[1] & 0x20) != 0);
        evt.event = ((msg[1] & 0x10) == 0); // In the message, 0 is unlock, 1 is lock
        events.push_back(evt);
        bcount++;
        debug << "                 * DSB Broadcast: " << (int)addr << ":" << std::hex << (int)type << ":" << (int)msg[0] << ":" << (int)msg[1] << std::dec << std::endl;
      } else {
        debug << Debug::Mode::Err << "Unknown event type: " << std::hex << (int)type <<
          std::dec << std::endl;
        continue;
      }
    } else {
      // This is not a broadcast, so return it
      return ret;
    }
  }

  // If we got here we received too many broadcasts
  return false;
}

bool DSBInterface::_recv(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
                        int timeoutMS) {
  // Check to see if we have any bytes to read
  int avail = 0;
  int state = 0;
  uint8_t len = -1;
  uint8_t crc = -1;
  struct timeval start;
  gettimeofday(&start, nullptr);
  // These messages are short so just read them one byte at a time to make the state machine easier
  //  If this is a bottleneck we can redo it later
  bool done = false;
  while(!done) {
    // Check to make sure we haven't timed out
    struct timeval end;
    gettimeofday(&end, nullptr);
    long seconds  = end.tv_sec  - start.tv_sec;
    long useconds = end.tv_usec - start.tv_usec;
    long delta_ms = ((seconds) * 1000 + useconds/1000.0) + 0.5;

    if((delta_ms) > timeoutMS) {
      //debug << Debug::Mode::Info << "Timed out on DSB recv" << std::endl;
      return false;
    }

    // Then check to see if we have any bytes
    int ret = ioctl(devFD, FIONREAD, &avail);
    if(ret < 0) {
      debug << "dsb ioctl < 0" << ret << std::endl;
      if(errno == EAGAIN) {
        debug << Debug::Mode::Err << " **** DSB serial return EAGAIN" << std::endl;
      }
      
      debug << Debug::Mode::Err << "Failed to get available serial port bytes: " <<
        strerror(errno) << std::endl;
      return false;
    }

    // We have data available, so read a byte and process it
    else if(avail > 0) {
    
      uint8_t byte;
      read(devFD, &byte, 1);
      switch(state) {
      case 0: // This is the header
        if(byte == 0) continue; // Skip all leading zero bytes
        addr = byte & 0x1F;
        len = (byte >> 5) & 0x03;
        state++;
        break;
      case 1: // The type byte
        type = byte;
        state++;
        break;
      case 2: // Payload
        msg.push_back(byte);
        if((len  < 2 && (msg.size() == (len+1))) ||
           (len == 2 && (msg.size() == 4)))
          state++;             
        break;
      case 3: // CRC
        crc = byte;
        debug << "MSG: " << std::hex << (int)addr << ":" <<  (int)len << ":" <<  (int)type << ":";
        for(unsigned int i = 0; i < msg.size(); i++) debug <<  (int)msg[i] << ":";
        debug <<  (int)crc << std::dec << std::endl;
        done = true;
        break;
      default:
        // This is an error -- bad message
        debug << Debug::Mode::Err << "Bad state on DSB receive: " << state << std::endl;
        break;
      }
    }
    
    // There is no data yet - so sleep 5ms and wait for a byte
    else {
      usleep(5 * 1000);
    }
  } // end while(true)

  return true;
}

// This will populate the dsbs variable
bool DSBInterface::discover() {
  {
    // LS4 issues a global lock, global solenoid disable, and global proximity disable to all DSB nodes
    //   to ensure they do not report MT99 (on drawer change) during discovery
    std::vector<uint8_t> msg {0x00}; // disallow opening, disable solenoids, disable prox sensors
    if(!send(BROADCAST_ADDRESS, GLOBAL_LOCK_TYPE, false, msg)) {
      debug << "Failed to send disable message on discover" << std::endl;
    }
  }

  dsbs.clear();
  
  for(int addr = 1; addr < 14; addr++) {
    {
    // Request info from  the address
      std::vector<uint8_t> msg {0x00};
      if(!send(addr, 0x01, true, msg)) {
        debug << "Failed to send read message to addr: " << addr << std::endl;
        continue;
      }
    }
    
    // Wait for the response
    std::vector<uint8_t> rmsg;
    uint8_t raddr, rtype;
    if(!recv(raddr, rtype, rmsg)) {
      debug << "Read from " << (int)addr << " timed out" << std::endl;
      continue;
    }
    
    // Make sure this is the drawer we think it is
    if(raddr != 15) {
      debug << "discovery read address wrong: " << (int)raddr << "::" << 15 << std::endl;
      continue;
    }
    
    // Make sure the message is the correct size
    if(rmsg.size() != 4) {
      debug << "Message size wrong: " << rmsg.size() << std::endl;
      continue;
    }
    
    // Make sure this is a discovery response
    if(rtype != 0x81) {
      debug << Debug::Mode::Info << "Discovery read is not correct message type: " <<
        std::hex << (int)rtype << std::dec << std::endl;
      continue;
    }
    
    // Check the address against the device type
    uint8_t dtype = rmsg[0] & 0x0F;
    if(dtype == 3) {
      // DSB 1 and three drawer
      if(addr >= 14 || addr <= 0) {
        debug << "DSB at wrong address: " << (int)addr << std::endl;
        continue;
      }
    } else if(dtype == 7 && addr != 14) { // CUPS
      debug << "CUPS at wrong address: " << (int)addr << std::endl;
      continue;
    } else {
      debug << "Unknown device type: " << (int)dtype << std::endl;
      continue;
    }
    
    if(dtype == 3) {
      // If we are here a DSB responded at address <addr>
      DSB dsb;
      dsb.errors = 0;
      dsb.temperature = 0;
      dsb.status_byte = 0;
      dsb.factoryMode = false;
      dsb.proxStatus = false;
      dsb.solenoidStatus = 0;
      dsb.gunlock = dsb.lunlock = false;

      dsb.address = addr;
      dsb.bootLoaderMode = ((rmsg[1] & 0x10) != 0);
      dsb.version = rmsg[3];
      uint8_t drawer_count = rmsg[1] & 0x0F;
      dsb.end   = (rmsg[2]>>4) & 0x0F;
      dsb.start = (rmsg[2]>>0) & 0x0F;

      debug << Debug::Mode::Debug << "DSB v." << std::hex << (int)dsb.version << std::dec << " @ " <<
        ((dsb.bootLoaderMode)?(" *"):("  ")) << (int)dsb.address << " d: " << 
        (int)dsb.start << " to " << (int) dsb.end <<
        std::endl;
        
      for(int dn = 0; dn < drawer_count; dn++) {
        DSB::drawer drawer;
        drawer.open = false;
        drawer.position = 0;
        drawer.solenoidState = 0;
        
        if(dsb.start == 15) drawer.index = 15; // This is a special "unassigned" case
        else drawer.index = dsb.start + dn;

        debug << Debug::Mode::Debug << "    Drawer @ " << (int)drawer.index << std::endl;
        dsb.drawers.push_back(drawer);
      }

      dsbs.push_back(dsb);      
    }
  } // Close for over each address
  
  debug << Debug::Mode::Debug << "Discovered " << dsbs.size() << "dsbs" << std::endl;

  {
    std::vector<uint8_t> msg {0x07}; // allow opening, solenoids in auto mode, enable prox sensors
    if(!send(BROADCAST_ADDRESS, GLOBAL_LOCK_TYPE, false, msg)) {
      debug << "Failed to send disable message on discover" << std::endl;
    }
  }
  
  return true;
  }

bool DSBInterface::setGlobalLockState(bool state) {
  uint8_t val = (state) ? (0x06) : (0x07);
  std::vector<uint8_t> msg {val};
  if(!send(BROADCAST_ADDRESS, 0x02, false, msg)) {
    debug << "Failed to send set global lock broadcast" << std::endl;
    return false;
  }

  globalLockState = state;
  
  return true;
}

bool DSBInterface::setFactoryMode(bool state) {
  uint8_t val = (state) ? (0x01) : (0x00);
  std::vector<uint8_t> msg {val};
  if(!send(BROADCAST_ADDRESS, FACTORY_MODE_TYPE, false, msg)) {
    debug << "Failed to send set factory mode broadcast" << std::endl;
    return false;
  }

  factoryModeState = state;
  
  return true;
}

bool DSBInterface::clearDrawerIndices() {
  // LS4 issues a global lock, global solenoid disable, and global proximity disable to all DSB nodes
  //   to ensure they do not report MT99 (on drawer change) during discovery
  std::vector<uint8_t> msg {0x00};
  if(!send(BROADCAST_ADDRESS, 0x21, false, msg)) {
    debug << "Failed to send clear indices broadcast" << std::endl;
    return false;
  }

  return true;
}

bool DSBInterface::assignDrawerIndex(uint8_t index) {
  if(index < 0 || index > 13) return false;

  // Send the index value
  uint8_t val = index & 0x0F;
  std::vector<uint8_t> msg {val};
  if(!send(BROADCAST_ADDRESS, 0x22, false, msg)) {
    debug << "Failed to send set index broadcast" << std::endl;
    return false;
  }

  return true;
}

bool DSBInterface::updateFirmware(std::string fw_path) {
  return false;
}

bool DSBInterface::setBootLoaderMode(bool enable) {
  uint8_t val = (enable)?(1):(0);
  std::vector<uint8_t> msg {val};
  if(!send(BROADCAST_ADDRESS, BOOTLOADER_MODE_TYPE, false, msg)) {
    debug << "Failed to send bootloader mode broadcast" << std::endl;
    return false;
  }

  return true;
}

bool DSBInterface::globalReset() {
  // Clear everything out
  std::vector<uint8_t> msg {0x00};
  if(!send(BROADCAST_ADDRESS, GLOBAL_RESET_TYPE, false, msg)) {
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
    if(!send(dsb.address, GET_TEMP_TYPE, true, msg)) {
      debug << "Failed to send get temp message to " << (int)dsb.address << std::endl;
      continue;
    }

    // Get the message back
    {
      std::vector<uint8_t> rmsg;
      uint8_t raddr, rtype;
      if(!recv(raddr, rtype, rmsg)) {
        debug << "Read from " << (int)dsb.address << " timed out" << std::endl;
        continue;
      }

      // Got the return message, so parse it
      if(raddr != 15) {
        debug << "Read temp return wrong address: " << raddr << std::endl;
        continue;
      }

      if(rtype != GET_TEMP_RETURN) {
        debug << "Wrong type " << rtype << " in get temp return" << std::endl;
        continue;
      }

      if(rmsg.size() != 1) {
        debug << "get temp returned " << rmsg.size() << " bytes" << std::endl;
        continue;
      }

      dsb.temperature = static_cast<char>(rmsg[0]);
    }        
  }

  return true;
}

bool DSBInterface::getDrawerStatus() {
  // For each DSB in dsbs:
  for(DSB &dsb : dsbs) {
    {
      std::vector<uint8_t> msg {0x00};
      if(!send(dsb.address, GET_STATUS_TYPE, true, msg)) {
        debug << "Failed to send get status message to " << (int)dsb.address << std::endl;
        continue;
      }
    }

    // Get the message back
    {
      std::vector<uint8_t> rmsg;
      uint8_t raddr, rtype;
      if(!recv(raddr, rtype, rmsg)) {
        debug << "Read from " << (int)dsb.address << " timed out" << std::endl;
        continue;
      }

      // Got the return message, so parse it
      if(raddr != 15) {
        debug << "Read staus return wrong address: " << raddr << std::endl;
        continue;
      }

      if(rtype != GET_STATUS_RETURN) {
        debug << "Wrong type " << rtype << " in get status return" << std::endl;
        continue;
      }

      if(rmsg.size() != 4) {
        debug << "get status returned " << rmsg.size() << " bytes" << std::endl;
        continue;
      }

      debug << "Status message: " << std::hex << (int)rmsg[0] << ":" <<
        (int)rmsg[1] << ":" <<
        (int)rmsg[2] << ":" <<
        (int)rmsg[3] << std::endl;
            
      // We will be assuming the DSB reported the drawers in order
      //  TODO: For the test systems this is not always true, so the check was removed
      //        add it back in for better testing if needed
      // For each drawer in dsb.drawers      
      for(unsigned int curr = 0; curr < dsb.drawers.size(); curr++) {
        // Get the drawer
        DSB::drawer &d = dsb.drawers[curr];
        // and set the status
        d.solenoidState = (rmsg[curr] >> 6) & 0x03;
        d.open = (((rmsg[curr]) & 0x20) != 0);
        d.position = (rmsg[curr] & 0x0F);
      } // end for over drawers

      dsb.status_byte = rmsg[3];

      dsb.errors         = (rmsg[3]       & 0x01) != 0;
      dsb.factoryMode    = (rmsg[3]       & 0x02) != 0;
      dsb.proxStatus     = (rmsg[3]       & 0x04) != 0;
      dsb.solenoidStatus = (rmsg[3] >> 3) & 0x03;
      dsb.gunlock        = (rmsg[3]       & 0x40) != 0;
      dsb.lunlock        = (rmsg[3]       & 0x80) != 0;

    } // end receive message scope
  } // end for over DSBs

  return true;
}

bool DSBInterface::getErrors(DSB &dsb, std::vector<uint8_t> &errors) {
  // Get the errors from a single DSB.  This clears the error log
  {
    std::vector<uint8_t> msg {0x00};
    if(!send(dsb.address, GET_ERRORS_TYPE, true, msg)) {
      debug << "Failed to send get errors message to " << (int)dsb.address << std::endl;
      return false;
    }
  }
  
  // Get the message back
  {
    std::vector<uint8_t> rmsg;
    uint8_t raddr, rtype;
    if(!recv(raddr, rtype, rmsg)) {
      debug << "Read from " << (int)dsb.address << " timed out" << std::endl;
      return false;
    }
    
    // Got the return message, so parse it
    if(raddr != 15) {
      debug << "Read errors return wrong address: " << raddr << std::endl;
      return false;
    }
    
    if(rtype != GET_ERRORS_RETURN) {
      debug << "Wrong type " << rtype << " in get errors return" << std::endl;
      return false;
    }
    
    if(rmsg.size() != 4) {
      debug << "get errors returned " << rmsg.size() << " bytes" << std::endl;
      return false;
    }

    errors.clear();

    // Parse the message
    uint8_t num_errors = rmsg[0] & 0x0F;
    for(unsigned int i = 0; i < rmsg.size(); i++) {
      uint8_t err1 = rmsg[i] & 0x0F;
      uint8_t err2 = (rmsg[i] >> 4) & 0x0F;

      // The first error message is combined with the count
      if(i != 0) errors.push_back(err1);
      if(errors.size() == num_errors) break;
      errors.push_back(err2);
      if(errors.size() == num_errors) break;
    } // end for over errors message payload
  } // end return message scope

  return true;
}
  
  
  
