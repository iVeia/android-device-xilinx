#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "dsb.hh"
#include "util.hh"

using namespace std;

namespace iveia {
  DSB::DSB(const string &devFile, const string &speed, int delay) :
    device(devFile), speed(speed), delayMs(delay) {
    verbose = false;
    opened = open();
    for(int i = 0; i < 16; i++) dsbs[i] = true;
  }

  DSB::~DSB() {
    if(fd >= 0) close(fd);

    fd = -1;
  }

  bool DSB::open() {
    struct termios options;
    int rc;

    cout << "Opening device" << endl;
    
    fd = ::open(device.c_str(), O_RDWR | O_NOCTTY);
    if(fd < 0) {
      cerr << "Failed to open RS-485 device: " << device << std::endl;
      cerr << strerror(errno) << std::endl;
      return false;
    }

    if((rc = tcgetattr(fd, &options)) < 0){
      cerr <<"Failed to get RS485 options" << std::endl;
      return false;
    }

    cout << "Setting serial port options" << endl;


    speed_t spd = B115200;
    if(speed == "115200") {
      cout << "Speed is 115200" << endl;
      spd = B115200;
    } else if(speed == "38400") {
      cout << "Speed is 38400" << endl;
      spd = B38400;
    } else {
      cerr << "Invalid speed: " << speed << endl;
      return false;
    }
    
    // Set the baud rates to 115200
    cfsetispeed(&options, spd);

    // Set the baud rates to 115200
    cfsetospeed(&options, spd);

    cfmakeraw(&options);
    options.c_cflag |= (CLOCAL | CREAD);   // Enable the receiver and set local mode
    options.c_cflag &= ~CSTOPB;            // 1 stop bit
    options.c_cflag &= ~CRTSCTS;           // Disable hardware flow control
    options.c_cc[VMIN]  = 1;
    options.c_cc[VTIME] = 2;

    // Set the new attributes
    if((rc = tcsetattr(fd, TCSANOW, &options)) < 0){
      cerr << "Failed to set RS485 options" << std::endl;
      return false;
    }

    cout << "Opened and set up RS-485: " << fd << std::endl;
    return true;
  }

  bool DSB::recv(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
                 int timeoutMS, std::vector<uint8_t> & dat) {
    int count = 0;
    int avail = 0;
    int state = 0;
    uint8_t len = -1;
    uint8_t crc = -1;

    // Get our time for timeout checking purposes
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
        cout << "Timed out on receive: " << count << endl;
        return false;
      }
      count++;

      // Then check to see if we have any bytes to read via ioctl
      int ret = ioctl(fd, FIONREAD, &avail);

      // check for errors first
      if(ret < 0) {
        cout << "dsb ioctl < 0: " << ret << std::endl;
        if(errno == EAGAIN) {
          // On an EAGAIN, should we just try again?  For now print it and exit
          cerr << " **** DSB serial return EAGAIN" << std::endl;
        }

        cerr << "Failed to get available serial port bytes: " << strerror(errno) << endl;
        return false;
      }

      // No errors and we have data available, so read a byte at a time and process it
      else if(avail > 0) {
        while(avail > 0) {
          uint8_t byte;
          read(fd, &byte, 1);
          switch(state) {
          case 0: // This is the header
            if(byte == 0) continue; // Skip all leading zero bytes as per ICD
            dat.push_back(byte);
            addr = byte & 0x1F;
            len = (byte >> 5) & 0x03;
            state++;
            break;
          case 1: // The type byte
            dat.push_back(byte);
            type = byte;
            state++;
            break;
          case 2: // Payload
            dat.push_back(byte);
            msg.push_back(byte);
            if((len  < 2 && (msg.size() == (len+1))) ||
               (len == 2 && (msg.size() == 4)))
              state++;
            break;
          case 3: // CRC                                                                                                                                                                                                                                              
          {
            crc = byte;
            uint8_t calc_crc = CalcCRC(dat);
            dat.push_back(byte);
            if(crc != calc_crc) {
              cerr << "CRC Mismatch: " << std::hex << (int)crc << " vs " << (int)calc_crc <<
                std::dec << endl;
              return false;
            }
            
            //cout << "MSG: " << std::hex << (int)addr << ":" <<  (int)len << ":" <<  (int)type << ":";                                                                                                                                                                
            //for(unsigned int i = 0; i < msg.size(); i++) cout <<  (int)msg[i] << ":";                                                                                                                                                                                
            //cout <<  (int)crc << std::dec << std::endl;                                                                                                                                                                                                              
            done = true;
          }
          break;
          default:
            // This is an error -- bad message
            cerr << "Bad state on DSB receive: " << state << std::endl;
            break;
          }
          avail--;
        }
      }

      // There is no data yet - so sleep 1ms and wait for a byte
      else {
        usleep(1000);
      }
    } // end while(true)

    return true;
  }

  // addr = address to send to
  // type = message type
  // read = is this a read operation
  // dat = the data to send
  //       dat is passed by reference so it will have the message we sent after
  //       this function returns
  bool DSB::send(uint8_t addr, uint8_t type,
                 bool read, std::vector<uint8_t> &dat) {
    // address 31 is a broadcast - keep track of that
    bool bcast = (addr == 31);
    int msg_size = dat.size();

    // Sanity check type here - then add it to the front of the message
    // TODO: Sanity check
    dat.insert(dat.begin(), type);

    // Put the starting byte together
    //  7  : R/W (R = 1)
    //  6:5: Length (0=1, 1=2, 2=4, 3=hex_record)
    //  4:0: Address
    // For the length, if we are trying to send three bytes, pad it to 4 with a zero
    //   Maybe that should be an error though?
    // TODO: If we ever have a hex record of <4 bytes this will not work
    //       I don't think those exist (given the type and address fields)
    //       But something to bear in mind
    uint8_t start = ((read)?(0x80) : (0x00)) | (addr & 0x1F);
    switch(msg_size) {
    case 1: break;
    case 2: start |= 0x20; break;
    case 3: start |= 0x40; dat.push_back(0x00); break;
    case 4: start |= 0x40; break;
    default:
      if(type == MSG_HEX_TYPE && addr == ADDR_DOWNLOAD) {
        start |= 0x60; break;
      } else {
        cerr << "Tried to call send with data of size " << dat.size() << std::endl;
        return false;
      }
    }
    dat.insert(dat.begin(), start);

    // add CRC
    uint8_t calc_crc = CalcCRC(dat);
    dat.push_back(calc_crc);

    //cout << "DSB sending: addr:" <<(int)addr << " type:" <<  std::hex << (int)type << " read:" << (int)read << " data: ";
    //for(unsigned int q = 0; q < dat.size(); q++) {
    //  cout << std::hex << (int)dat[q] << "," << std::dec;
    //}
    //cout << std::endl;

    //  Broadcast messages are always sent three times
    int count = (bcast) ? 3 : 1;

    // Send the message
    while(count > 0) {
      int ret = 0;
      ret = write(fd, dat.data(), dat.size());

      if(ret < 0) {
        cerr << "Failed to write DSB message: " << strerror(errno) <<
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


  bool DSB::DiscoverLight() {
    return discoverLight(false);
  }

  bool DSB::Program(const std::string &hexFile) {
    ifstream in;
    bool success = false;
    in.open(hexFile, ifstream::in);

    if(!in.is_open()) return false;

    // Put everyone into bootloader mode
    cout << "Sending bootloader broadcast message" << endl;
    vector<uint8_t> sblm {0x01};
    send(ADDR_BROADCAST, MSG_SETBLM_TYPE, false, sblm);

    // Sleep one second
    sleep(1);
    discoverLight(true);

    cout << "Programming with a delay of " << delayMs << endl;

    // Clear out any errors
    // TODO: Get these and report them?
    checkErrors();
    
    string line;
    int line_num = -1;
    int errors_num = 0;
    while(getline(in, line)) {
      line_num++;

      while(true) {
        if(line[line.length() - 1] == '\n') line.pop_back();
        else if(line[line.length() - 1] == '\r') line.pop_back();
        else break;
      }

      int length = line.length();

      // Check for odd length because of ':' start char
      if((length % 2) != 1) {
        cerr << "Line data has an odd length: " << length << "(" << line << ")" << endl;
        continue;
      }

      if(line[0] != ':') {
        cerr << "line is not valid: " << line << endl;
        continue;
      }

      vector<uint8_t> dat;
      for(unsigned int i = 1; i < line.length(); i += 2) {
        uint8_t t1 = GetNibble(line[i]);
        uint8_t t2 = GetNibble(line[i+1]);
        if(t1 > 0x0f || t2 > 0x0f) {
          cerr << "Failed to get nibble: " << (int)t1 << ":" << (int)t2 << endl;
          break;
        }

        uint8_t tmp = (t1 << 4) | t2;
        dat.push_back(tmp);
      }

      //cout << "Sending data: " << std::hex << setw(2);
      //for(unsigned int i = 0; i < dat.size(); i++) {
      //  cout << setw(2) << (int)dat[i] << " ";
      //}
      //cout << std::dec << setw(1) << endl;
      //
      //cout << "            " << line[0] << " ";
      //for(unsigned int i = 1; i < line.length(); i+=2) {
      //  cout << line[i] << line[i+1] << " ";
      //}
      //cout << "\t<- from file" << endl;

      if(dat[3] == 0x01) {
        // EOF, we need to get errors first
        cout << "Found EOF -- Checking for errors" << endl;

        usleep(100);
        checkErrors();
        //if(!success) {
        // TODO: Make the return an array of bools so we know what happened
        //       or mayble a tuple<addr, list<errors>> so we know which failed with what errors?
        //  cerr << "Failed to program some boards!" << endl;
        //}
      }

      unsigned int num_bytes = dat.size();
      dat.insert(dat.begin(), (uint8_t)num_bytes);
      send(ADDR_DOWNLOAD, MSG_HEX_TYPE, false, dat);
      usleep(delayMs * 1000); // Sleep for 100ms between each record

      // At the moment we check for errors after every record
      vector<uint8_t> errors;
      if(debug) errors = checkErrors();
      
      if(errors.size() > 0) {
        cout << endl << "Errors on line number " << setw(4) << line_num << ":";
        for(unsigned int i = 0; i < errors.size(); i++) cout << setw(2) << (int)errors[i] << ",";
        cout << "          <";
        cout << line[0];
        for(unsigned int i = 1; i < line.length(); i+=2) {cout << line[i] << line[i+1] << ((i==line.length()-1)?(""):("_"));} 
        cout << ">" << endl;;

        uint8_t *pdat = dat.data();
        cout << "                                        Sent ->   <";
        for(unsigned int i = 0; i < dat.size(); i++) cout << setw(2) << setfill('0') << std::hex << (int)pdat[i] << ((i==dat.size()-1)?(""):("_"));
        cout << ">" << std::dec << setfill(' ') << endl;
        errors_num++;
      } else if(verbose) {
        cout << endl << "Sent line                 " << setw(4) << line_num << ":";
        cout << "                           <";
        cout << line[0];
        for(unsigned int i = 1; i < line.length(); i+=2) {cout << line[i] << line[i+1] << ((i==line.length()-1)?(""):("_"));} 
        cout << ">" << endl;;

        uint8_t *pdat = dat.data();
        cout << "                                        Sent ->   <";
        for(unsigned int i = 0; i < dat.size(); i++) cout << setw(2) << setfill('0') << std::hex << (int)pdat[i] << ((i==dat.size()-1)?(""):("_"));
        cout << ">" << std::dec << setfill(' ') << endl;
      }

      if(!debug && !verbose) {
        if((line_num % 10) == 0) {
          cout << line_num << "..";
          cout.flush();
        }
        if((line_num % 200) == 199) cout << endl;
      }

    } // End for over each line

    cout << "Processed " << line_num << " lines with " << errors_num << " errors" << endl << endl;

    in.close();

    // Wait one second then run discover
    sleep(1);
    discoverLight(false);

    return success;
  }

  vector<uint8_t> DSB::checkErrors() {
    vector<uint8_t> errors;
    for(int addr = 1; addr < 14; addr++) {
      {
        if(!dsbs[addr]) continue;
        
        // Request info from  the address
        vector<uint8_t> msg {0x00};
        if(!send(addr, MSG_GET_ERRORS, true, msg)) {
          cout << "Failed to send get errors message to addr: " << addr << std::endl;
          continue;
        }
      }
      
      // Wait for the response
      vector<uint8_t> rmsg;
      vector<uint8_t> rdat;
      uint8_t raddr, rtype;
      if(!recv(raddr, rtype, rmsg, 100, rdat)) {
        cout << "Got errors reading from " << (int)addr << ". timed out" << std::endl;
        continue;
      }

      if(rtype != MSG_GET_ERRORS_RETURN) {
        cout << "Message from " << (int)addr << " was not get errors return: Type: " <<
          std::hex << (int)rtype << std::dec << endl;
        continue;
      }
      
      if((rmsg[0] & 0x0F) != 0) {        
        uint8_t error = (rmsg[0]>>4) & 0x0F;
        errors.push_back(error);
        
        for(unsigned int i = 1; i < rmsg.size(); i++) {
          uint8_t error1 = rmsg[i] & 0x0F;
          uint8_t error2 = (rmsg[i]>>4) & 0x0F;
          errors.push_back(error1);
          errors.push_back(error2);
        }
      }
    } // end for over addresses

    return errors;
  }

  // Discover devices.  inBLM determines the mode we are looking for
  bool DSB::discoverLight(bool inBLM) {
    cout << "Discovering" << endl;
    
    for(int addr = 1; addr < 14; addr++) {
      dsbs[addr] = false;
      {
        // Request info from  the address
        std::vector<uint8_t> msg {0x00};
        if(!send(addr, MSG_DISCOVER_LIGHT, true, msg)) {
          cout << "Failed to send discover light message to addr: " << addr << std::endl;
          continue;
        }
      }
      
      // Wait for the response
      std::vector<uint8_t> rmsg;
      std::vector<uint8_t> rdat;
      uint8_t raddr, rtype;
      if(!recv(raddr, rtype, rmsg, 100, rdat)) {
        cout << "Read from " << (int)addr << " timed out" << std::endl;
        continue;
      }
      
      cout << "Repsonse from discover to " << addr;
      for(unsigned int i = 0; i < rdat.size(); i++) cout << " " << std::hex << setw(2) << (int)rdat[i];
      cout << std::dec << setw(1) << endl;

      // Make sure this is addressed to me
      if(raddr != 15) {
        cout << "discovery read address wrong: " << (int)raddr << "::" << 15 << std::endl;
        continue;
      }
      
      // Make sure the message is the correct size
      if(rmsg.size() != 2) {
        cout << "Message size wrong: " << rmsg.size() << std::endl;
        continue;
      }
      
      // Make sure this is a discovery response
      //  TODO: Deal with broadcasts during broadcast.  Even though they should not be happening
      if(rtype != MSG_DISCOVER_LIGHT_RETURN) {
        cerr << "Discovery read is not correct message type: " <<
          std::hex << (int)rtype << std::dec << std::endl;
        continue;
      }
      
      dsbs[addr] = true;
      
      bool bootLoaderMode = ((rmsg[0] & 0x10) != 0);
      bool success = ((inBLM)?(bootLoaderMode):(!bootLoaderMode));
      if(!success) {
        cerr << "DSB " << addr << " not in correct mode: " << inBLM << ":" << bootLoaderMode << endl;
        return false;
      } else {
        cout << "DSB " << addr << " is in the correct mode.  BLM:" << inBLM << endl;
      }

      uint8_t version = rmsg[1];
      cout << "DSB Version: " << GetNibbleChar( (version >> 4) & 0x0F) << "." <<
        GetNibbleChar( version & 0x0f) << endl;
    } // for each address
    
    return true;
    
  }
}
