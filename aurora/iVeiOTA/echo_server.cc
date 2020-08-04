#include <iostream>
#include <signal.h>

#include "iveiota.hh"
#include "socket_echo_interface.hh"
#include "debug.hh"


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
  debug.SetThreshold(Debug::Mode::Info);

  debug << Debug::Mode::Info << "Starting server" << std::endl;
  // Register a signal handler so that we can exit gracefully when ctrl-c is pressed
  signal (SIGINT, signalHandler);

  // Create our listening socket
  SocketEchoInterface server([&server](uint8_t *dat, int len) {
                           debug << Debug::Mode::Debug << "Echoing " << len << " bytes" << std::endl;
                           server.Send(dat, len);
                         }, true, "/tmp/iveia_echo");

  // Our event loop
  bool done = false;
  while(!exiting) {

    // Check to see if we have been signaled to stop
    if(exiting && !done) {
      server.Stop();
      done = true;
    }
    
    // Process any data we need to from the server
    if(!server.Process()) {
      break;
    }

    // TODO: Implement a timeout here so that if we are killed but the sockets dont close
    //  we don't stay around anyway
  }
}
