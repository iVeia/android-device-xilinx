#include <iostream>
#include <string>
#include <cstdlib>
#include <functional>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/select.h>
#include <cstring>

#include "iveiota.hh"
#include "socket_echo_interface.hh"

using namespace std;
using namespace iVeiOTA;

// Not thread safe
static volatile bool exiting = false;
void signalHandler(int sig) {
  switch(sig) {
    case SIGINT:
      std::cout << "sigint received: exiting" << std::endl;
      exiting = true;
      break;      
  }
}

int main(int argc, char ** argv) {
  signal (SIGINT, signalHandler);
  
  cout << "iVeiOTA client starting..." << endl;
  
  cout << "Connecting to server." << endl;
  SocketEchoInterface intf(
    [](uint8_t *dat, int len) {
      cout << "Got some data back" << std::endl;
    }, false, "/tmp/iveia_echo");

  char c[128];
  strcpy(c, "hello this is a string");
  intf.Send((uint8_t*)c, strlen(c) + 1);
  
  while(!exiting) {
    // Process any data we need to from the server
    if(!intf.Process()) {
      break;
    }
  }
  
  return 0;
}
