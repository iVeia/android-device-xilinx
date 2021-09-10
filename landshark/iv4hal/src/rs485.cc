#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>


#include "debug.hh"
#include "support.hh"
#include "rs485.hh"

using namespace iv4;

bool RS485Interface::open() {
  struct termios   options;
  int rc;
  
  fd = ::open(devFile.c_str(), O_RDWR | O_NOCTTY);
  if(fd < 0) {
    debug << Debug::Mode::Failure << "Failed to open RS-485 device: " << devFile << std::endl;
    debug << Debug::Mode::Failure << strerror(errno) << std::endl;
    fd = -1;
    return false;
  }

  if((rc = tcgetattr(fd, &options)) < 0){
    debug << Debug::Mode::Err << "Failed to get RS485 options" << std::endl;
    ::close(fd);
    fd = -1;
    return false;
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
    ::close(fd);
    fd = -1;
    return false;
  }

  debug << "Set up RS-485" << std::endl;
  return true;
}

bool RS485Interface::close() {
  if(fd >= 0) {
    ::close(fd);
    fd = -1;

    return true;
  }

  return false;
}

void RS485Interface::DumpSerialPortStats() {
  struct serial_icounter_struct icount = { };

    int ret = ioctl(fd, TIOCGICOUNT, &icount);
    if (ret >= 0) {
      debug << devFile << " stats -" << 
        " RX: " << icount.rx <<
        " TX: " << icount.tx <<
        " Frame: " << icount.frame <<
        " Overrun: " << icount.overrun <<
        " Parity: " << icount.parity <<
        " BRK: " << icount.brk <<
        " Buf Overrun: " << icount.buf_overrun << std::endl;
    } else {
      debug << "Failed to get serial port stats ****" << std::endl;
    }
}

RS485Interface::RS485Interface(const std::string &devFile) {
  this->devFile = devFile;

  dsb_interface = nullptr;

  open();
}

RS485Interface::~RS485Interface() {
  close();
}

void RS485Interface::SetInterfaces(DSBInterface *dsbs) {
  dsb_interface = dsbs;
}

RS485Interface::RS485Return RS485Interface::SendAndReceive(uint8_t &addr, uint8_t &type, bool read,
                                          std::vector<uint8_t> &msg, int timeoutMS) {
  bool sret = Send(addr, type, read, msg);
  if(!sret) return RS485Interface::RS485Return::SendFailed;

  return Receive(addr, type, msg, timeoutMS);
}

bool RS485Interface::Send(uint8_t addr,
                          uint8_t type,
                          bool read,
                          std::vector<uint8_t> &dat) {
  // address 31 is a broadcast  
  bool bcast = (addr == 31);
  int msg_size = dat.size();
  
  // Sanity check type here - then add it to the front of the message
  // TODO: Sanity check message type to make sure it exists
  dat.insert(dat.begin(), type);

  // Put the starting byte together
  //  7  : R/W (R = 1)
  //  6:5: Length (0=1b, 1=2b, 2=4b, 3=8b)
  //  4:0: Address
  // For the length, if we are trying to send three bytes, pad it to 4 with a zero
  uint8_t start = ((read)?(0x80) : (0x00)) | (addr & 0x1F);
  switch(msg_size) {
  case 1: break;
  case 2: start |= 0x20; break;
  case 4: start |= 0x40; break;
  case 8: start |= 0x60; break;
  default:
    debug << Debug::Mode::Err << "485 Tried to call send with data of size " << dat.size() << std::endl;
    return false;
  }
  dat.insert(dat.begin(), start);

  // add CRC
  uint8_t crc = CalcCRC(dat);
  dat.push_back(crc);

  debug << Debug::Info::Time <<
    "485 sending: addr:" << (int)addr <<
    " type:" << std::hex << (int)type <<
    " read:" << std::dec << (int)read <<
    " data";
  debug << " <--> ";
  for(unsigned int q = 0; q < dat.size(); q++) {
    debug << std::hex << (int)dat[q] << "," << std::dec;
  }
  debug << std::endl;

  // Send the message
  //  Broadcast messages are always sent three times
  int count = (bcast) ? 3 : 1;
  while(count > 0) {
    int ret = write(fd, dat.data(), dat.size());

    if(ret < 0) {
      debug << Debug::Mode::Err << "Failed to write 485 message: " << strerror(errno) <<
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

int RS485Interface::BytesAvailable() {
  // Then check to see if we have any bytes
  int avail;
  int ret = ioctl(fd, FIONREAD, &avail);
  if(ret < 0) {
    debug << "dsb ioctl < 0" << ret << std::endl;
    if(errno == EAGAIN) {
      debug << Debug::Mode::Err << " **** DSB serial return EAGAIN" << std::endl;
    }
    
    debug << Debug::Mode::Err << "Failed to get available serial port bytes: " <<
      strerror(errno) << std::endl;
    return -1;    
  } else return avail;
}

bool RS485Interface::ProcessMainLoop() {
  int crcFailCount = 0;
  while(true) {
    int avail = BytesAvailable();
    if(avail < 0) {
      return false;
    } else if(avail == 0) {
      return true;
    } else {
      uint8_t addr = BROADCAST_ADDRESS;
      uint8_t type = 0x00;
      std::vector<uint8_t> msg;
      RS485Return ret = Receive(addr, type, msg, DEFAULT_MAINLOOP_TIMEOUT);
      if(ret == RS485Return::Success) {
        // This actually, kind of shouldn't happen
        return true;      
      } else if(ret == RS485Return::RecvCRCFailure) {
        // Try again, but after many CRC failures give someone else a chance
        if(++crcFailCount == 10) return false;      
      }
      else return false;
    }
  }
}

RS485Interface::RS485Return RS485Interface::Receive(uint8_t &addr,
                                                    uint8_t &type,
                                                    std::vector<uint8_t> &msg,
                                                    int timeoutMS) {
  bool done = false;
  int bcount = 0;
  while(!done) {
    msg.clear();

    // Make sure we haven't received too many broadcasts -- we have to let the others get in
    if(bcount > BROADCASTS_PER_LOOP) {
      return RS485Return::RecvTooManyBroadcasts;
    }
    
    RS485Return ret = ReceiveSingleMessage(addr, type, msg, timeoutMS);

    if(ret == RS485Return::Success) {
      if(addr == BROADCAST_ADDRESS) {
        // We received a broadcast, which is an unsolicited event - send it to the appropriate location
        bcount++;

        switch(type) {
          
        case DRAWER_STATE_CHANGE_EVENT:
          {
            if(msg.size() != 2) {
              debug << Debug::Mode::Err << "drawer state change broadcast wrong size: " << msg.size() << std::endl;
              continue;
            }
            if(dsb_interface != nullptr) dsb_interface->ReceiveDrawerEvent(msg);
            else debug << Debug::Mode::Err << "DSB Interface is not defined for drawer event" << std::endl;
          }
          break;
          
        case DSB_SELF_ASSIGN_EVENT:
          {
            //uint8_t sa_index = msg[0] & 0x1F;
            //uint8_t sa_type = (msg[0] >> 5) >> 0x7;
            //debug << Debug::Info::Time <<
            //  "Ignoring DSB self assignment: " << (int)sa_type << ":" << (int)sa_index << std::endl;
            if(dsb_interface != nullptr) dsb_interface->SelfAssignEvent();
            else debug << Debug::Mode::Err << "DSB Interface not defined for self assign event" << std::endl;
          }
          break;
          
        default:
          {
            debug << Debug::Mode::Err << "Unknown event type: " << std::hex << (int)type <<
              std::dec << std::endl;
          }
          break;
        };   
      } else {
        // This is not a broadcast, so return it
        return RS485Interface::RS485Return::Success;
      }
    } else {
      // ReceiveSingleMessage returned failure, which means timeout
      return ret;
    }
  } // end while (!done)

  // If we got here we received too many broadcasts
  return RS485Interface::RS485Return::RecvTooManyBroadcasts;
}

RS485Interface::RS485Return
RS485Interface::ReceiveSingleMessage(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
                                     int timeoutMS) {
  struct timeval start;
  gettimeofday(&start, nullptr);
  std::vector<uint8_t> full_msg;
  
  // These messages are short so just read them one byte at a time to make the state machine easier
  //  If this is a bottleneck we can redo it later
  bool done = false;
  int waitingLen = 0;
  RS485Interface::RecvState state =  RS485Interface::RecvState::WaitHeader;

  debug << "<";
  
  while(!done) {
    // We have data available, so read a byte and process it
    int avail = BytesAvailable();
    if(avail < 0) {
      return RS485Return::RecvFailed;
    } else if(avail > 0) {    
      uint8_t byte;
      int bread = read(fd, &byte, 1);
      debug << "(" << std::hex << (int)byte << "-" << (int)state << ")" << std::dec;
      if(bread <= 0) {
        // Failed to read a byte
        debug << Debug::Mode::Err << "Failed to read a byte from serial port" << std::endl;
        return RS485Return::RecvFailed;
      }
      gettimeofday(&start, nullptr);

      switch(state) {
      case RS485Interface::RecvState::WaitHeader: // This is the header
        {
          if(byte & 0x80) continue; // read bit is set -- only landshark can do that
          
          addr = (byte & 0x1F);
          if(addr == 0 || (addr >= 16 && addr < 0x1F)) continue; // invalid address

          full_msg.push_back(byte);      
          
          int len = (byte >> 5) & 0x03;
          if(len == 0)      waitingLen = 1;
          else if(len == 1) waitingLen = 2;
          else if(len == 2) waitingLen = 4;
          else if(len == 3) waitingLen = 8;
          else {
            debug << Debug::Mode::Err << "Invalid RS485 length: " << len << std::endl;
            return RS485Return::RecvFailed;
          }
            
          state =  RS485Interface::RecvState::WaitType;
        }
        break;
        
      case  RS485Interface::RecvState::WaitType: // The type byte
        {
          full_msg.push_back(byte);      
          type = byte;
          state =  RS485Interface::RecvState::ReadPayload;
        }
        break;
        
      case  RS485Interface::RecvState::ReadPayload: // Payload
        {
          full_msg.push_back(byte);      
          msg.push_back(byte);
          if(--waitingLen == 0) state =  RS485Interface::RecvState::WaitCRC;
        }
        break;
        
      case  RS485Interface::RecvState::WaitCRC: // CRC
        {
          uint8_t crc = CalcCRC(full_msg);
          full_msg.push_back(byte);

          {
            Debug::Mode dm = Debug::Mode::Debug;
            if(crc != byte) dm = Debug::Mode::Err;
            
            debug << dm << Debug::Info::Time << "recv <-> ";
            for(unsigned int q = 0; q < full_msg.size(); q++) {
              debug << dm << std::hex << (int)full_msg[q];
              if(q != (full_msg.size()-1)) debug << ":";
            }
            debug << dm << std::hex << " (" << std::hex << (int)crc << ")";
            if(crc != byte) {
              debug << dm << " *** CRC Failed *** ";
            }
            debug << dm << std::dec << " -- avail: " << avail << std::endl;
          }
        
          if(crc != byte) {
            return RS485Return::RecvCRCFailure;;
          } else {
            // We have a message, so return it
            return RS485Return::Success;
          }
          
        }
        break;
      default:
        // This is an error -- bad message
        debug << Debug::Mode::Err << "Bad state on DSB receive: " << (int)state << std::endl;
        return RS485Return::RecvFailed;;
      }
    }
    
    else if(timeoutMS == 0)  {
      // there is no data and our timeout is 0, so we will be timing out
      debug << ">a";
      return RS485Return::RecvTimeout;
    }
      
    // Check to make sure we haven't timed out
    struct timeval end;
    gettimeofday(&end, nullptr);
    long seconds  = end.tv_sec  - start.tv_sec;
    long useconds = end.tv_usec - start.tv_usec;
    long delta_ms = ((seconds) * 1000 + useconds/1000.0) + 0.5;

    // If we timeout - reset the state machine
    if(delta_ms >= timeoutMS) {
      debug << ">b:" << seconds << ":" << useconds << ":" << delta_ms << ":";
      return RS485Return::RecvTimeout;
    } else {
      usleep(500);
    }
  } // end while(true)

  return RS485Return::Success;
}

