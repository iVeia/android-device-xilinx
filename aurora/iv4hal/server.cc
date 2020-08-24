#include <iostream>
#include <signal.h>

#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <memory>

#include "iv4_hal.hh"

#include "debug.hh"
#include "socket_interface.hh"
#include "support.hh"
#include "message.hh"
#include "camera.hh"

using namespace iv4;
using namespace std;



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
    // We do not have a way - at the moment - to differentiate between the event socket and the comm socket
    debug << Debug::Mode::Info << "sigpipe received: doing nothing" << std::endl;
    brokenPipe = true;
    break;
  }
}

int main(int argc, char ** argv) {
  debug.SetThreshold(Debug::Mode::Info); // Don't print out debugging information by default
  debug.SetDefault(Debug::Mode::Debug);  // Default all debug statements to Mode::Debug

  debug << Debug::Mode::Warn << "Starting iv4 HAL server v" << 
    IV4HAL_MAJOR << "." <<
    IV4HAL_MINOR << "." <<
    IV4HAL_PATCH << std::endl;
    
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

  debug << Debug::Mode::Debug << "Debug statements visible" << std::endl;
  debug << Debug::Mode::Info  << "Info  statements visible" << std::endl;
  debug << Debug::Mode::Warn  << "Warn  statements visible" << std::endl;
  debug << Debug::Mode::Err   << "Error statements visible" << std::endl;

  bool initialized = false;

  // Create all our cameras -
  //   Order matters here, as we interact with user applications via an index
  //   TODO: Add a string identifier to each camera?
  //             Not sure if that is really needed as this is a very specific application and doesn't
  //             reeally need to be generic
  std::vector<CameraInterface> cameras;

  // Camera0 First
  std::tuple<int,int> res0 = CameraInterface::InitializeBaslerCamera(0);
  cameras.push_back(CameraInterface("/dev/video0",
                                    std::get<0>(res0),
                                    std::get<1>(res0)));
  debug << Debug::Mode::Info << "Initialized /dev/video0 " << (void*)&cameras.back() << "with resolution " <<
    std::get<0>(res0) << "," <<  std::get<1>(res0) << std::endl;
  // Next Camera would go here
  

  // TODO: Keep track of images for now, but we will be moving to a streaming system later
  std::unique_ptr<Image> activeImage0;
  
  // Create our listening socket
  SocketInterface server([&server, &cameras, &initialized](const Message &message) {
    debug << "Message received: " << message.header.toString() << std::endl;
    
    std::vector<std::unique_ptr<Message>> resp;

    // We handle management messages here ourselves
    if(message.header.type == Message::Management &&
       message.header.subType == Message::Management.Initialize) {
      // Initialize has been called - send back our state and our revision
      uint32_t rev =
        ((IV4HAL_MAJOR << 16) & 0x00FF0000) |
        ((IV4HAL_MINOR <<  8) & 0x0000FF00) |
        ((IV4HAL_PATCH <<  0) & 0x000000FF);
      resp.push_back(std::unique_ptr<Message>(new Message(Message::Management, Message::Management.Initialize,
                                                          0, 0, 0, rev)));
      if(!initialized) {
        debug << "Initializing " << cameras.size() << " cameras" << std::endl;
        for(CameraInterface &cam : cameras) {
          cam.InitializeV4L2();
          debug << "Initialized camera " << (void*)&cam << std::endl;
        }
      } else {
        // We are trying to initialize when we already are.  No harm there?        
      }

      // Keep track of the fact that we have been initialized
      initialized = true;
    } else if(!initialized) {
      // If we haven't been initialized yet, we can't continue
      resp.push_back(Message::MakeNACK(message, 0, "Not yet initialized"));
    } else {
      // Otherwise we pass the message to the appropriate consumer
      switch(message.header.type) {
      case Message::Image:
        {
          debug << "Processing Image message" << std::endl;
          int wcam = message.header.imm[0];
          if(wcam >= (int)cameras.size()) {              
            resp.push_back(Message::MakeNACK(message, 0, "Invalid camera"));
          } else {
            resp.push_back(cameras[wcam].ProcessMessage(message));
          }
        } // ------------ End case Message::Image ------------
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

  SocketInterface eventServer([] (const Message &msg) {
    //  What to do?  This interface is for sending async events to the client
    //  We should not be getting messages on it...
  }, true, IV4HAL_EVENT_SOCK_NAME);

  // Our event loop
  bool done = false;
  while(!exiting) {

    // Check to see if we have been signaled to stop, and if so kill the manager and the server
    if(exiting && !done) {
      server.Stop();
      eventServer.Stop();
      done = true;
    }

    // If the client disconnected, we have to close the client connection
    // TODO: Figure out what to do here about the event socket
    //       The socket generating the SIGPIPE should return an EPIPE error on read/write
    //         so we should be using that instead
    if(brokenPipe) {
      server.CloseConnection();
      //eventServer.CloseConnection();
      
      brokenPipe = false;
    }

    // We need to do this because we don't have each server in its own thread.  Bascially
    //  we need to collect all the file descriptors into one select statement so that we
    //  don't chew up the processor, but we also don't spend time waiting on e.g. the server
    //  when the camera has work to do
    fd_set sockset;
    FD_ZERO(&sockset);
    int maxfd = -1;
    int sfd = server.ReadySet(&sockset);
    int efd = eventServer.ReadySet(&sockset);
        
    if(sfd == -1 || efd == -1) {
      debug << Debug::Mode::Failure << "One of the servers is not running! " <<
        sfd << ":" << efd << std::endl;
    }
    
    maxfd = max(sfd, efd);
    //for(CameraInterface &cam : cameras) {
    //  int cfd = cam.ReadySet(&sockset);
    //  if(cfd > 0) maxfd = std::max(maxfd, cfd);
    //}

    
    // TODO: Make this number configurable?  Decide on the best value here
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000 * 25; // 25ms

    int sret = select(maxfd + 1, &sockset, NULL, NULL, &timeout);
    if(sret < 0) {
      // We timed out, but that doesn't matter
      //debug << "Timeout on select" << std::endl;
    } else {
      //debug << "select returned: " <<  sret << std::endl;
    }
    
    // Process any data we need to from the server
    if(FD_ISSET(sfd, &sockset) && !exiting) {
      debug << " ---------------------------- Processing server" << std::endl;
      if(!server.Process(&sockset)) {
        break;
      }
    }

    if(FD_ISSET(efd, &sockset) && !exiting) {
      debug << " ----------------------------- Processing events" << std::endl;
      if(!eventServer.Process(&sockset)) {
        
      }
    }

    // Check to see if we have any cameras here that we need to process
    // TODO: Split this off into a new thread, that way we can reduce latency
    //       on the main communication thread
    // Always call the camera stuff regardless of the select return value
    //  If they don't have anything to do they just return as they are non-blocking
    for(CameraInterface &cam : cameras) {
      //debug << "Processing camera: " << (void*)&cam << std::endl;
      
      cam.ProcessMainLoop(eventServer);
    }

    // TODO: Implement a timeout here so that if we are killed but the sockets dont close
    //  we don't stay around anyway
  }

}
