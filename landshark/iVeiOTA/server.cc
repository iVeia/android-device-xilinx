#include <iostream>
#include <signal.h>

#include "iveiota.hh"
#include "socket_interface.hh"

#include "message.hh"

#include "uboot.hh"
#include "ota_manager.hh"
#include "config.hh"

#include "debug.hh"


using namespace iVeiOTA;

// signal handling is not thread safe
static volatile bool exiting = false;
static volatile bool brokenPipe = false;
void signalHandler(int sig) {
  switch(sig) {
  case SIGINT:
    debug << Debug::Mode::Info << "sigint received: exiting" << std::endl;
    exiting = true;
    break;

  case SIGPIPE:
    // We get a broken pipe when the client disconnects.  This is expected and we can safely ignnore it
    //  we do have to close the client socket when it happens though
    debug << Debug::Mode::Info << "sigpipe received: doing nothing" << std::endl;
    brokenPipe = true;
    break;
  }
}

int main(int argc, char ** argv) {
  debug.SetThreshold(Debug::Mode::Info); // Don't print out debugging information by default
  debug.SetDefault(Debug::Mode::Debug);  // Default all debug statements to Mode::Debug

  debug << Debug::Mode::Info << "Starting OTA server" << std::endl;
  // Register a signal handler so that we can exit gracefully when ctrl-c is pressed
  signal (SIGINT, signalHandler);
  signal (SIGPIPE, signalHandler);

  // For development convenience, fake things that don't exist on a dev system
  bool simulate = false;
  for(int i = 1; i < argc; i++) {
    if(std::string(argv[i]) == "-s") simulate = true;
    
    else if(std::string(argv[i]) == "-d") debug.SetThreshold(Debug::Mode::Debug);
    else if(std::string(argv[i]) == "-q") debug.SetThreshold(Debug::Mode::Warn);
  }

  // Read our configuration file
  config.Init();

  // Create our cache location
  // TODO: Would like a better method than just calling out to the shell
  // TODO: Should make sure this works and isn't a non-directory file or something
  RunCommand(std::string("mkdir -p ") + IVEIOTA_CACHE_LOCATION);
  RunCommand(std::string("mkdir -p ") + IVEIOTA_MNT_POINT);

  // Create an interface to the uboot env processing
  UBootManager uboot;
  OTAManager   manager(uboot);
  bool initialized = false;

  debug << Debug::Mode::Debug << "Debug statements visible" << std::endl;
  debug << Debug::Mode::Info  << "Info  statements visible" << std::endl;
  debug << Debug::Mode::Warn  << "Warn  statements visible" << std::endl;
  debug << Debug::Mode::Err   << "Error statements visible" << std::endl;

  // Create our listening socket
  SocketInterface server([&uboot, &manager, &server, &initialized](const Message &message) {
    debug << "Message received: " << message.header.toString() << std::endl;
    std::vector<std::unique_ptr<Message>> resp;

    // We handle management messages here ourselves
    if(message.header.type == Message::Management &&
       message.header.subType == Message::Management.Initialize) {
      // Initialize has been called - send back our state and our revision
      initialized = true;
      uint32_t updated = uboot.GetUpdated(Container::Active) ? 1 : 0;
      uint32_t rev =
        ((IVEIOTA_MAJOR << 16) & 0x00FF0000) |
        ((IVEIOTA_MINOR <<  8) & 0x0000FF00) |
        ((IVEIOTA_PATCH <<  0) & 0x000000FF);
      resp.push_back(std::unique_ptr<Message>(new Message(Message::Management, Message::Management.Initialize,
                                                          updated, 0, 0, rev)));
    } else if(!initialized) {
      // If we haven't been initialized yet, we can't continue
      resp.push_back(Message::MakeNACK(message, 0, "Not yet initialized"));
    } else if(!config.Valid()) {
      // If this doesn't seem to be a valid system, we always just NACK
      resp.push_back(Message::MakeNACK(message, 0, "System not OTA capable"));
    } else {
      // Otherwise we pass the message to the appropriate consumer
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
    
    debug << "Sending " << resp.size() << " messages as a response" << std::endl;
    if(resp.size() < 1) {
      // We got a message but don't have a response for it
      debug << Debug::Mode::Err << "No response to " << (int)message.header.type << ":" << (int)message.header.subType << std::endl;
      resp.push_back(Message::MakeNACK(message, 0, "Internal Error: No response to this message"));
    } else if(resp.size() > 1) {
      // We shouldn't be trying to send more than one message in response anymore
      //  That was an idea that didn't work so well in practice
      debug << Debug::Mode::Warn << "More than one response to the message " << (int)message.header.type << ":" << (int)message.header.subType << std::endl;
    }
    for(unsigned int i = 0; i < resp.size(); i++) {
      server.Send(*resp[i]);
    }
  }, true);

  // Our event loop
  bool done = false;
  while(!exiting) {

    // Check to see if we have been signaled to stop, and if so kill the manager and the server
    if(exiting && !done) {
      server.Stop();
      manager.Cancel();
      done = true;
    }

    // If the client disconnected, we have to close the client connection
    if(brokenPipe) {
      server.CloseConnection();
      brokenPipe = false;
    }
    
    // Process any data we need to from the server
    if(!server.Process()) {
      break;
    }

    if(!manager.Process()) {
      // What to do here?
    }

    // TODO: Implement a timeout here so that if we are killed but the sockets dont close
    //  we don't stay around anyway
  }
}
