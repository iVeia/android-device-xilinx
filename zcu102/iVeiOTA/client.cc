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
#include "socket_interface.hh"
#include "message.hh"

using namespace std;
using namespace iVeiOTA;

// Give ourselves a hint in console dumps that this is from our test client
#define IVEIOTA_TEST_CLIENT "<testCli> "

// Not thread safe
static volatile bool exiting = false;
void signalHandler(int sig) {
  switch(sig) {
    case SIGINT:
      std::cout << IVEIOTA_TEST_CLIENT << "sigint received: exiting" << std::endl;
      exiting = true;
      break;
  }
}

int main(int argc, char ** argv) {
    signal (SIGINT, signalHandler);

    cout << IVEIOTA_TEST_CLIENT << "iVeiOTA test client starting..." << endl;

    struct {
        char const *arg;
        bool more;
        uint16_t cmd, subCmd;
    } commands[] = {
        {"--init",     false, Message::Management,     Message::Management.Initialize},

        {"--begin",    true,  Message::OTAUpdate,      Message::OTAUpdate.BeginUpdate},
        {"--cancel",   false, Message::OTAUpdate,      Message::OTAUpdate.CancelUpdate},
        {"--continue", false, Message::OTAUpdate,      Message::OTAUpdate.ContinueUpdate},
        {"--process",  true,  Message::OTAUpdate,      Message::OTAUpdate.ProcessChunk},
        {"--finalize", false, Message::OTAUpdate,      Message::OTAUpdate.Finalize},

        {"--ostatus",  false, Message::OTAStatus,      Message::OTAStatus.UpdateStatus},
        {"--cstatus",  false, Message::OTAStatus,      Message::OTAStatus.ChunkStatus},

        {"--valid",    false, Message::BootManagement, Message::BootManagement.SetValidity},
        {"--usuccess", true, Message::BootManagement, Message::BootManagement.MarkUpdateSuccess},
        {"--bsuccess", true, Message::BootManagement, Message::BootManagement.ResetBootCount},

        {0, false, 0, 0},
    };

    vector<Message> messages;
    bool noCopy = false;
    for(int i = 1; i < argc; i++) {
        int j = 0;
        while(true) {
            if(commands[j].arg == 0) break;

            if(strncmp(argv[i], "--nocopy", 8) == 0) {
              cout << IVEIOTA_TEST_CLIENT << "Doing a no copy" << endl;
              noCopy = true;
            }

            if(strncmp(commands[j].arg, argv[i], strlen(commands[j].arg)) == 0) {
                uint32_t i1=0, i2=0, i3=0, i4=0;
                vector<uint8_t> payload;

                if(commands[j].more) {
                    // Command requires more processing
                  if(strcmp(commands[j].arg, "--begin") == 0) {
                    i++;
                    if(i >= argc) {
                      cerr << "Need a manifest file to begin update" << endl;
                      break;
                    }

                    // This needs a path to the manifest file
                    i1 = 1; // Manifest on filesystem
                    if(noCopy) i4 = 42; // no copy for convenience sake
                    for(int q = 0; q < (int)strlen(argv[i]); q++) {
                      payload.push_back(argv[i][q]);
                    }
                    payload.push_back('\0');
                  } // end begin

                  else if(strcmp(commands[j].arg, "--usuccess") == 0 ||
                          strcmp(commands[j].arg, "--bsuccess") == 0) {
                    i1 = 1; // current container
                  } // end successes


                  else if(strcmp(commands[j].arg, "--process") == 0) {
                    i += 2;
                    if(i >= argc) {
                      cerr << "Need a chunk file and identifier to begin update" << endl;
                      break;
                    }

                    // This needs a path to the chunk file
                    i1 = 1; //
                    for(int q = 0; q < (int)strlen(argv[i-1]); q++) {
                      payload.push_back(argv[i-1][q]);
                    }
                    payload.push_back('\0');
                    for(int q = 0; q < (int)strlen(argv[i]); q++) {
                      payload.push_back(argv[i][q]);
                    }
                    payload.push_back('\0');

                  } // end process

                }

                cout << IVEIOTA_TEST_CLIENT << "pushing message: " << (int)commands[j].cmd << ":" << (int)commands[j].subCmd <<
                  ":" << i1 << ":" << i2 << ":" << i3 << ":" << i4 << ":" << payload.size() << endl;

                messages.push_back(Message(commands[j].cmd, commands[j].subCmd,
                                           i1, i2, i3, i4,
                                           payload));
            }

            j++;
        }
    }

    cout << IVEIOTA_TEST_CLIENT << "Connecting to server." << endl;
    SocketInterface intf(
      [](const Message &message) {
        cout << IVEIOTA_TEST_CLIENT << "Received message: " << (int)message.header.type << ":" << (int)message.header.subType << endl;
        cout << "\t" << message.header.imm[0] << ":" << message.header.imm[1] << ":" << message.header.imm[2] << ":" << message.header.imm[3] << endl;
        cout << "\t" << message.header.pLen << endl;
        for(unsigned int i = 0; i < message.payload.size(); i++) {
          printf(" %c|%02X ", (char)message.payload[i], message.payload[i]);
          if(((i + 1) % 10) == 0) printf(" -- \n");
        }
        if(message.payload.size() > 0) cout << endl;
    });

    // Sending all messages
    for(auto m : messages) {
      cout << IVEIOTA_TEST_CLIENT << "Sending message: " << (int)m.header.type << ":" << (int)m.header.subType << endl;
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
