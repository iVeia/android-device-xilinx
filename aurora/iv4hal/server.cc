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
    debug << Debug::Mode::Info << "sigpipe received: doing nothing" << std::endl;
    brokenPipe = true;
    break;
  }
}

int main(int argc, char ** argv) {
  debug.SetThreshold(Debug::Mode::Info); // Don't print out debugging information by default
  debug.SetDefault(Debug::Mode::Debug);  // Default all debug statements to Mode::Debug

  debug << Debug::Mode::Info << "Starting iv4 HAL server" << std::endl;
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

  // We need to initialize our cameras here

  // Start out with the 13MP camera at /dev/video0
  // TODO: Most of this is hard-coded and needs to be configurable
  //       At the moment adding more cameras is hard
  CameraInterface::InitializeBaslerCamera("/system/etc/basler_out.csv",
                                          "/dev/v4l-subdev0");

  // And create all our cameras - just one for now
  CameraInterface cam0("/dev/video0", 4208, 3120);
  std::unique_ptr<Image> activeImage0;
  
  // Create our listening socket
  SocketInterface server([&server, &cam0, &activeImage0, &initialized](const Message &message) {
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
        debug << "Initializing camera 0" << std::endl;
        cam0.InitializeV4L2();
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
        // TODO: This should go in a manager somwhere.  Big, long, switch/if/else
        //       statements are horrible
      case Message::Image:
        debug << "Processing Image message" << std::endl;
        switch(message.header.subType) {
        case Message::Image.CaptureImage:
        case Message::Image.GetImage:
          {
            int wcam = message.header.imm[0];
            if(wcam != 0) {
              resp.push_back(Message::MakeNACK(message, 0, "Invalid camera"));
            } else {
              int itype = message.header.imm[1];
              // TODO: Need a better way to handle types
              if(itype != 0x01) {
                resp.push_back(Message::MakeNACK(message, 0, "Type not supported yet"));                
              } else {
                // If we are here, we actually want to capture an image!
                if(message.header.subType == Message::Image.CaptureImage) {
                  activeImage0 = cam0.GetRawImage();
                  debug << "Captured an image: " << activeImage0.get() << std::endl;
                  resp.push_back(Message::MakeACK(message));
                } else if(activeImage0 != nullptr) {
                  // Has to be getimage, or else we won't be in this case
                  int imageSize = activeImage0->Size();
                  int w = activeImage0->Width();
                  int h = activeImage0->Height();
                  debug << "Getting image: " << w << ":" << h << "(" << imageSize << ")" << endl;

                  uint32_t res = ((w<<16) & 0xFFFF0000) | (h & 0x0000FFFF);
                  uint32_t msgs = 0x00010001; // At the moment we only send one message / image
                  uint32_t itype = ToInt(activeImage0->Type());
                  
                  std::vector<uint8_t> sendDat;
                  activeImage0->GetData(sendDat);
                  resp.push_back(std::unique_ptr<Message>(new Message(Message::Image,
                                                                      Message::Image.SendImage,
                                                                      0, // camera number
                                                                      itype,
                                                                      res, msgs,
                                                                      sendDat)));
                } else {
                  resp.push_back(Message::MakeNACK(message, 0, "No active image to get"));
                }
              }
            }
          } // end case CaptureImage / GetImage
          break;
        case Message::Image.ContinuousCapture:
          resp.push_back(Message::MakeNACK(message, 0, "Continuous capture not yet supported"));
          break;
        default:
          resp.push_back(Message::MakeNACK(message, 0, "Unknown Image subtype"));
        } // Switch over image subtype
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

    // TODO: Implement a timeout here so that if we are killed but the sockets dont close
    //  we don't stay around anyway
  }

}
