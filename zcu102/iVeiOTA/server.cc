#include <iostream>
#include <signal.h>

#include "container_info.hh"
#include "iveiota.hh"
#include "socket_interface.hh"

#include "message.hh"

#include "uboot.hh"
#include "ota_manager.hh"
#include "config.hh"

#include "debug.hh"


using namespace iVeiOTA;

// Not thread safe
static volatile bool exiting = false;
static volatile bool brokenPipe = false;
void signalHandler(int sig) {
  switch(sig) {
  case SIGINT:
    std::cout << "sigint received: exiting" << std::endl;
    exiting = true;
    break;
    
  case SIGPIPE:
    std::cout << "sigpipe received: doing nothing" << std::endl;
    brokenPipe = true;
    break;
  }
}

int main(int argc, char ** argv) {
  debug.SetThreshold(Debug::Mode::Info);

  debug << Debug::Mode::Info << "Starting server" << std::endl;
  // Register a signal handler so that we can exit gracefully when ctrl-c is pressed
  signal (SIGINT, signalHandler);
  signal (SIGPIPE, signalHandler);

  // For development convenience, fake things that don't exist on a dev system
  bool simulate = false;
  for(int i = 1; i < argc; i++) {
    if(std::string(argv[i]) == "-s") simulate = true;
  }

  // Read our configuration file
  config.Init();

  // Create an interface to the uboot env processing
  UBootManager uboot;
  OTAManager   manager(uboot);

  // Create our listening socket
  SocketInterface server([&uboot, &manager, &server](const Message &message) {
    std::cout << "Message received: " << message.header.toString() << std::endl;
    std::vector<std::unique_ptr<Message>> resp;
    debug << "Got a message " << std::endl;
    switch(message.header.type) {
      case static_cast<uint32_t>(Message::Management):
        switch(message.header.subType) {
          case Message::Management.Initialize:
            // We don't really have anything to do here at the moment as the system inits itself
            //  on startup at the moment
            resp.push_back(Message::MakeACK(message));
          break;
          default: break;
        }
        break;

      case Message::OTAUpdate:
      case Message::OTAStatus:
        debug << "Processing OTAManager message" << std::endl;
        resp = manager.ProcessCommand(message);
        break;

      case Message::BootManagement:
        debug << "Processing Boot message" << std::endl;
        resp = uboot.ProcessCommand(message);
        break;

      default:
        break;
    } // end switch

    debug << "Sending " << resp.size() << " messages" << std::endl;
    for(unsigned int i = 0; i < resp.size(); i++) {
      server.Send(*resp[i]);
    }
  }, true);

  // Our event loop
  bool done = false;
  while(!exiting) {

    // Check to see if we have been signaled to stop
    if(exiting && !done) {
      server.Stop();
      done = true;
    }

    if(brokenPipe) {
      server.CloseConnection();
      brokenPipe = false;
    }
    
    // Process any data we need to from the server
    if(!server.Process()) {
      break;
    }

    // TODO: Implement a timeout here so that if we are killed but the sockets dont close
    //  we don't stay around anyway
  }
}
