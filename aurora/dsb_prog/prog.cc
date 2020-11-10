
#include <iostream>

#include "prog.hh"
#include "dsb.hh"

using namespace std;
using namespace iveia;

void print_usage(const string &name) {
  cout << "Usage: " << name << endl <<
    "\t-d <device>  : RS-485 dev file (default /dev/ttyUSB0)" << endl <<
    "\t-f <file>    : Hex file" << endl <<
    "\t-p           : Program the DSB" << endl <<
    "\t-i           : Run discovery" << endl <<
    "\t-l <delay>   : How long to delay between hex records (in ms).  Valid range: 10 to 10000" << endl <<
    "\t-b <baud>    : Baud rate.  Supported: 38400, 115200" << endl <<
    "\t-v           : Verbose.  Print out every hex record sent" << endl <<
    "\t-g           : Debug.  Check for errors after every hex record" << endl << 
    endl;
}

int main(int argc, char ** argv) {

  string dev = "/dev/ttyUSB0";
  string file = "";
  string baud = "115200";
  int delay = 100;
  bool prog = false;
  bool disc = false;
  bool dbg = false;
  bool verbose = false;

  cout << "Programmer version: " << PROGRAMMER_MAJOR << "." << PROGRAMMER_MINOR << endl;
  
  for(int i = 1; i < argc; i++) {
    string command(argv[i]);
    if(command == "-d") {
      if(++i >= argc) {
        cerr << "Device argument needs parameter" << endl;
        print_usage(argv[0]);
        return -1;
      } else {
        dev = string(argv[i]);
      }
    }

    else if(command == "-f") {
      if(++i >= argc) {
        cerr << "Hex file argument needs parameter" << endl;
        print_usage(argv[0]);
        return -1;
      } else {
        file = string(argv[i]);
      }
    }

    else if(command == "-g") dbg = true;
    
    else if(command == "-p") prog = true;

    else if(command == "-i") disc = true;

    else if(command == "-v") verbose = true;

    else if(command == "-b") {
      if(++i >= argc) {
        cerr << "Baud rate needs a parameter" << endl;
        print_usage(argv[0]);
        return -1;
      } else {
        baud = string(argv[i]);
      }
    }

    else if(command == "-l") {
      if(++i >= argc) {
        cerr << "Delay needs an argument" << endl;
        print_usage(argv[0]);
        return -1;
      } else {
        delay = atoi(argv[i]);
      }
    }
    
    else {
      cerr << "Unknown command" << endl;
      print_usage(argv[0]);
      return -1;
    }
  }

  // Sanity check everything
  if(file == "") {
    cerr << "No programming file specified" << endl;
    print_usage(argv[0]);
    return -1;
  }

  if(baud != "115200" &&
     baud != "38400") {
    cerr << "Invalid baud rate" << endl;
    print_usage(argv[0]);
    return -1;
  }

  if(delay <= 10 || delay >= 10000) {
    cerr << "Delay out of bounds" << endl;
    print_usage(argv[0]);
    return -1;
  }

  cout << "Programming " << file << " to device " << dev << endl;

  DSB dsb(dev, baud, delay);
  if(!dsb.IsOpen()) {
    cerr << "Could not open device: " << dev << endl;
    return -2;
  }

  if(verbose) dsb.Verbose(true);
  if(dbg) dsb.Debug(true);
  
  bool result = true;
  if(disc) result |= dsb.DiscoverLight();
  if(prog) result |= dsb.Program(file);

  if(!result) {
    cerr << "Programming failed" << endl;
    return -3;
  }
  
  
  return 0;
}
