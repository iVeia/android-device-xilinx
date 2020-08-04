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

using namespace std;
using namespace iv4;

// Give ourselves a hint in console dumps that this is from our test client
#define IV4HAL_TEST_CLIENT "<halCli> "

// Not thread safe
static volatile bool exiting = false;
void signalHandler(int sig) {
  switch(sig) {
    case SIGINT:
      std::cout << IV4HAL_TEST_CLIENT << "sigint received: exiting" << std::endl;
      exiting = true;
      break;
  }
}

int main(int argc, char ** argv) {
    signal (SIGINT, signalHandler);

    cout << IV4HAL_TEST_CLIENT << "iv_v4_hal test client starting..." << endl;

    struct {
        char const *arg;
        bool more;
        uint16_t cmd, subCmd;
    } commands[] = {
        {"--init",     false, Message::Management,     Message::Management.Initialize},
        {"--cimg",     true, Message::Image,          Message::Image.CaptureImage},
        {"--gimg",     true,  Message::Image,          Message::Image.GetImage},

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
                  }
                } // end if command.more
                
                cout << IV4HAL_TEST_CLIENT << "pushing message: " <<
                  (int)commands[j].cmd << ":" << (int)commands[j].subCmd <<
                  ":" << i1 << ":" << i2 << ":" << i3 << ":" << i4 << ":" << payload.size() << endl;
                
                messages.push_back(Message(commands[j].cmd, commands[j].subCmd,
                                           i1, i2, i3, i4,
                                           payload));
            }

            j++;
        }
    }

    cout << IV4HAL_TEST_CLIENT << "Connecting to server." << endl;
    SocketInterface intf(
      [&saveImgTo](const Message &message) {
        cout << IV4HAL_TEST_CLIENT << "Received message: " <<
          (int)message.header.type << ":" << (int)message.header.subType << endl;
        
        cout << "\t" << message.header.imm[0] << ":" << message.header.imm[1] << ":" <<
          message.header.imm[2] << ":" << message.header.imm[3] << endl;
        
        cout << "\t" << message.header.pLen << endl;

        // Print out the payload if it is less than 1KB
        if(message.payload.size() < (1024)) {
          for(unsigned int i = 0; i < message.payload.size(); i++) {
            printf(" %c|%02X ", (char)message.payload[i], message.payload[i]);
            if(((i + 1) % 16) == 0) printf(" -- \n");
          }
        }
        
        if(message.payload.size() > 0) {          
          cout << "Payload size: " << message.payload.size() << endl << endl;
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
    
    // Sending all messages
    for(auto m : messages) {
      cout << IV4HAL_TEST_CLIENT << "Sending message: " <<
        (int)m.header.type << ":" << (int)m.header.subType << endl;
      intf.Send(m);
    }

    while(!exiting) {
      // Process any data we need to from the server
      if(!intf.Process()) {
        break;
      }
    }


    return 0;
}
