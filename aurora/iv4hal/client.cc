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
#include <fstream>

#include "iv4hal.hh"
#include "socket_interface.hh"
#include "message.hh"
#include "camera.hh"
#include "debug.hh"
#include "time.h"

using namespace std;
using namespace iv4;

// Give ourselves a hint in console dumps that this is from our test client
#define IV4HAL_TEST_CLIENT "<halCli> "

// Not thread safe
static volatile bool exiting = false;
void signalHandler(int sig) {
  switch(sig) {
    case SIGINT:
      debug << Debug::Mode::Info << IV4HAL_TEST_CLIENT << "sigint received: exiting" << std::endl;
      exiting = true;
      break;
  }
}

int main(int argc, char ** argv) {
    signal (SIGINT, signalHandler);

    debug.SetThreshold(Debug::Mode::Info); // Don't print out debugging information by default
    debug.SetDefault(Debug::Mode::Debug);  // Default all debug statements to Mode::Debug
    
    for(int i = 1; i < argc; i++) {
      if(std::string(argv[i]) == "-d") debug.SetThreshold(Debug::Mode::Debug);
      else if(std::string(argv[i]) == "-q") debug.SetThreshold(Debug::Mode::Warn);
    }

    debug << Debug::Mode::Debug << "Debug statements visible" << std::endl;
    debug << Debug::Mode::Info  << "Info  statements visible" << std::endl;
    debug << Debug::Mode::Warn  << "Warn  statements visible" << std::endl;
    debug << Debug::Mode::Err   << "Error statements visible" << std::endl;
    
    struct {
        char const *arg;
        bool more;
        uint16_t cmd, subCmd;
    } commands[] = {
        {"--init",     false, Message::Management,     Message::Management.Initialize},
        {"--cimg",     true,  Message::Image,          Message::Image.CaptureImage},
        {"--gimg",     true,  Message::Image,          Message::Image.GetImage},
        {"--ccap",     true,  Message::Image,           Message::Image.ContinuousCapture},

        {0, false, 0, 0},
    };

    // The file to save the "GetImage" command results to
    std::string saveImgTo = "";
    
    // TODO: We just queue up a bunch of images here and blast them out.  There isn't any
    //       form of interactivity.  That would be a nice feature to add
    vector<Message> messages;
    for(int i = 1; i < argc; i++) {
        int j = 0;
        while(true) {
            if(commands[j].arg == 0) break;

            if(strncmp(commands[j].arg, argv[i], strlen(commands[j].arg)) == 0) {
                uint32_t i1=0, i2=0, i3=0, i4=0;
                vector<uint8_t> payload;

                if(commands[j].more) {
                  switch(commands[j].subCmd) {
                  case Message::Image.GetImage:
                    {
                      if(++i >= argc) {
                        cerr << "Not enough arguments to GetImage" << endl;
                        return -1;
                      }
                      saveImgTo = std::string(argv[i]);
                    }
                    // fall through - CaptureImage does not send the image back in the payload
                  case Message::Image.CaptureImage:
                    {
                      i1 = 0; // Which camera
                      i2 = ToInt(ImageType::UYVY); // Image type
                    }
                    break;
                case Message::Image.ContinuousCapture:
                  {                    
                    if(++i >= argc) {
                      cerr << "Not enough arguments to ContinuousCapture [on|off] [skip]" << endl;
                      return -1;
                    }
                    std::string action = std::string(argv[i]);

                    if(++i >= argc) {
                      cerr << "Not enough arguments to ContinuousCapture [on|off] [skip]" << endl;
                      return -1;
                    }
                    int skip = atoi(argv[i]);

                    i1 = 0;
                    i2 = ToInt(ImageType::UYVY);
                    if(action == "on" || action == "On" || action == "1") i3 = 1;
                    else i3 = 0;
                    i4 = skip;
                  }
                  break;
                  
                  } // end switch(subCmd)

                } // end if command.more
                
                debug << IV4HAL_TEST_CLIENT << "pushing message: " <<
                  (int)commands[j].cmd << ":" << (int)commands[j].subCmd <<
                  ":" << i1 << ":" << i2 << ":" << i3 << ":" << i4 << ":" << payload.size() << endl;
                
                messages.push_back(Message(commands[j].cmd, commands[j].subCmd,
                                           i1, i2, i3, i4,
                                           payload));
            }

            j++;
        }
    }

    debug << IV4HAL_TEST_CLIENT << "Connecting to server." << endl;
    SocketInterface intf(
      [&saveImgTo](const Message &message) {
        debug << IV4HAL_TEST_CLIENT << "Received message: " <<
          (int)message.header.type << ":" << (int)message.header.subType << endl;
        
        debug << "\t" << message.header.imm[0] << ":" << message.header.imm[1] << ":" <<
          message.header.imm[2] << ":" << message.header.imm[3] << endl;
        
        debug << "\t" << message.header.pLen << endl;

        // Print out the payload if it is less than 1KB
        if(message.payload.size() < (1024)) {
          for(unsigned int i = 0; i < message.payload.size(); i++) {
            printf(" %c|%02X ", (char)message.payload[i], message.payload[i]);
            if(((i + 1) % 16) == 0) printf(" -- \n");
          }
        }
        
        if(message.payload.size() > 0) {          
          debug << "Payload size: " << message.payload.size() << endl << endl;
        }

        if(message.header.type == Message::Image &&
           message.header.subType == Message::Image.SendImage) {
          // Save the image to a file
          ofstream outfile(saveImgTo, std::ios::binary | std::ios::out);
          int howMany = message.payload.size();
          const char *pDat = reinterpret_cast<const char*>(message.payload.data());

          // Write out the payload in 4K chunks
          while(howMany > 0) {
            int toWrite = (howMany > 4096) ? 4096 : howMany;
            outfile.write(pDat, toWrite);
            howMany = howMany - toWrite;
            pDat += toWrite;
          }
          
        }
      });

    int currFrame = 0;
    time_t thent, nowt;
    time(&thent);
    SocketInterface evtIntf( [&nowt, &thent, &currFrame] (const Message &message) {
        debug << "Got an event: " << message.toString() << endl;
        
        if(message.header.subType == Message::Image.SendImage) {
          /*
          char name[64];
          sprintf(name, "/data/cap%d.yuv", currFrame);
          currFrame++;
          
          ofstream outfile(std::string(name), std::ios::binary | std::ios::out);
          int howMany = message.payload.size();
          const char *pDat = reinterpret_cast<const char*>(message.payload.data());
          
          // Write out the payload in 4K chunks
          while(howMany > 0) {
            int toWrite = (howMany > 4096) ? 4096 : howMany;
            outfile.write(pDat, toWrite);
            howMany = howMany - toWrite;
            pDat += toWrite;
          }
          debug << Debug::Mode::Info << "Wrote an image to " << name << std::endl;
          */
        }
        if((++currFrame % 50) == 0) {
          time(&nowt);
          cout << currFrame << " frames in " << (nowt - thent) << " seconds" << std::endl;
          cout << (double)currFrame / (double)(nowt - thent) << " fps" << std::endl;
        }
        
      }, false, IV4HAL_EVENT_SOCK_NAME);
    
    // Sending all messages
    for(auto m : messages) {
      debug << IV4HAL_TEST_CLIENT << "Sending message: " <<
        (int)m.header.type << ":" << (int)m.header.subType << endl;
      intf.Send(m);
    }

    while(!exiting) {
      // Process any data we need to from the server
      // We need to do this because we don't have each server in its own thread.  Bascially
      //  we need to collect all the file descriptors into one select statement so that we
      //  don't chew up the processor, but we also don't spend time waiting on e.g. the server
      //  when the camera has work to do
      fd_set sockset;
      FD_ZERO(&sockset);
      int maxfd = -1;
      int sfd = intf.ReadySet(&sockset);
      int efd = evtIntf.ReadySet(&sockset);
      
      if(sfd == -1 || efd == -1) {
        debug << Debug::Mode::Failure << "One of the servers is not running! " <<
          sfd << ":" << efd << std::endl;
      }
      
      maxfd = max(sfd, efd);
      
      
      // TODO: Make this number configurable?  Decide on the best value here
      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      
      if(select(maxfd + 1, &sockset, NULL, NULL, &timeout) < 0) {
        // We timed out, but that doesn't matter
        debug << "Error on select" << std::endl;
      } else {
      }
      
      // Process any data we need to from the server
      if(FD_ISSET(sfd, &sockset) && !exiting) {
        debug << " ---------------------------- Processing server" << std::endl;
        if(!intf.Process(&sockset)) {
          break;
        }
      }
      
      if(FD_ISSET(efd, &sockset) && !exiting) {
        debug << " ----------------------------- Processing events" << std::endl;
        if(!evtIntf.Process(&sockset)) {
          
        }
      }

    }


    return 0;
}
