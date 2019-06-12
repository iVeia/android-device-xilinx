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
  bool initialized = false;

  // Create our listening socket
  SocketInterface server([&uboot, &manager, &server, &initialized](const Message &message) {
    std::cout << "Message received: " << message.header.toString() << std::endl;
    std::vector<std::unique_ptr<Message>> resp;
    debug << "Got a message " << std::endl;
    if(message.header.type == Message::Management &&
       message.header.subType == Message::Management.Initialize) {
      // INitialize here if needed
      initialized = true;
      resp.push_back(Message::MakeACK(message));
    } else if(!initialized) {
      resp.push_back(Message::MakeNACK(message, 0, "Not yet initialized"));
    } else if(!config.Valid()) {
      resp.push_back(Message::MakeNACK(message, 0, "System not OTA capable"));
    } else {
      switch(message.header.type) {
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
    } // end if(!initialized)
    
    debug << "Sending " << resp.size() << " messages" << std::endl;
    if(resp.size() < 1) {
      // We got a message but don't have a response for it
      debug << "No response to " << (int)message.header.type << ":" << (int)message.header.subType << std::endl;
      resp.push_back(Message::MakeNACK(message, 0, "No response to this message"));
    }
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
