#include <iostream>
#include <signal.h>

#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <memory>

#include "iv4_hal.hh"

#include "rs485.hh"
#include "debug.hh"
#include "socket_interface.hh"
#include "support.hh"
#include "message.hh"
#include "camera.hh"
#include "chillups.hh"
#include "dsb.hh"
#include "hardware.hh"

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

  debug << Debug::Mode::Warn << " **************** ";
  debug << Debug::Mode::Warn << "Starting iv4 HAL server v" << 
    IV4HAL_MAJOR << "." <<
    IV4HAL_MINOR << "." <<
    IV4HAL_PATCH;
  debug << Debug::Mode::Warn << " **************** " << std::endl;
    
  // Register a signal handler so that we can exit gracefully when ctrl-c is pressed
  signal (SIGINT, signalHandler);
  signal (SIGPIPE, signalHandler);

  // For development convenience, fake things that don't exist on a dev system
  bool simulate = false;
  bool use_cam  = true;
  bool use_cups = true;
  bool use_dsb  = true;
  bool init_on_start = false;
  bool pwm_leds = true;

  unsigned int update_freq = 10;
  bool usbrs485 = false;
  for(int i = 1; i < argc; i++) {
    if(std::string(argv[i]) == "-s") simulate = true;

    if(std::string(argv[i]) == "-i") init_on_start = true;
    
    else if(std::string(argv[i]) == "-d") debug.SetThreshold(Debug::Mode::Debug);
    else if(std::string(argv[i]) == "-q") debug.SetThreshold(Debug::Mode::Warn);
    
    else if(std::string(argv[i]) == "-f") {
      i++;
      if(i >= argc) break;
      update_freq = atoi(argv[i]);
    }

    else if(std::string(argv[i]) == "--cam=0") use_cam = false;
    else if(std::string(argv[i]) == "--cups=0") use_cups = false;
    else if(std::string(argv[i]) == "--dsb=0") use_dsb = false;
    else if(std::string(argv[i]) == "--urs485") usbrs485 = true;

    else if(std::string(argv[i]) == "--pwm-leds=0") pwm_leds = false;
  }

  debug << Debug::Mode::Debug << "Debug statements visible" << std::endl;
  debug << Debug::Mode::Info  << "Info  statements visible" << std::endl;
  debug << Debug::Mode::Warn  << "Warn  statements visible" << std::endl;
  debug << Debug::Mode::Err   << "Error statements visible" << std::endl;

  debug << Debug::Mode::Info << "Update frequency = " << update_freq << std::endl;
  
  RunCommand("echo 420 > /sys/class/gpio/export");
  RunCommand("echo out > /sys/class/gpio/gpio420/direction");
  RunCommand("echo 0 > /sys/class/gpio/gpio420/value");

  if(pwm_leds) {
    RunCommand("echo 0 > /sys/class/pwm/pwmchip0/export");
    RunCommand("echo 46000 > /sys/class/pwm/pwmchip0/pwm0/period");
    
    SetLED(0x00, pwm_leds);

    // Set the analog control all the way up so that pwm is all that controls the LEDs
    SetPot("/dev/i2c-0", 0x2C, 0xFF);

    RunCommand("echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable");
  }
  
  bool initialized = false;

  // Create all our cameras -
  //   Order matters here, as we interact with user applications via an index
  //   TODO: Add a string identifier to each camera?
  //             Not sure if that is really needed as this is a very specific application and doesn't
  //             reeally need to be generic
  std::vector<CameraInterface*> cameras;

  if(use_cam) {
    {
      // Camera0 First
      std::tuple<int,int> res0 = CameraInterface::InitializeBaslerCamera(0);
      cameras.push_back(new CameraInterface("/dev/video0",
                                            0,
                                            std::get<0>(res0),
                                            std::get<1>(res0)));
      cameras.back()->Resettable(true);
      debug << Debug::Mode::Info << "Initialized /dev/video0 " << (void*)cameras.back() << "with resolution " <<
        std::get<0>(res0) << "," <<  std::get<1>(res0) << std::endl;
    }
    {
      // Video1 is the sobel transformed image
      std::tuple<int,int> res1 = CameraInterface::InitializeBaslerCamera(1);
      cameras.push_back(new CameraInterface("/dev/video1",
                                            1,
                                            std::get<0>(res1),
                                            std::get<1>(res1)));    
      debug << Debug::Mode::Info << "Initialized /dev/video1 " << (void*)cameras.back() << "with resolution " <<
        std::get<0>(res1) << "," <<  std::get<1>(res1) << std::endl;
    }
  }
  
  std::string ddev = "/dev/ttyPS1";
  if(usbrs485) ddev = "/dev/ttyUSB0";
  
  debug << "Initializing RS485: " << ddev << std::endl;
  RS485Interface *rs485 = new RS485Interface(ddev);
  
  ChillUPSInterface *cups = nullptr;
  if(use_cups) {
    debug << "Initializing CUPS" << std::endl;
    //cups = new ChillUPSInterface(rs485, "/dev/i2c-2");
    cups = new ChillUPSInterface(rs485, "/dev/i2c-22");
  }

  
  DSBInterface *dsb = nullptr;
  if(use_dsb) {
    debug << "Initializing DSBs: " << rs485 << std::endl;

    dsb = new DSBInterface(rs485, update_freq);
  }

  rs485->SetInterfaces(dsb);

  SocketInterface eventServer([] (const Message &msg) {
    //  What to do?  This interface is for sending async events to the client
    //  We should not be getting messages on it...
  }, true, IV4HAL_EVENT_SOCK_NAME);


  // Hack to know what cameras and stuff to turn on
  bool cam0on=false, cam1on=false,
    image0on=false, edge0on=false,
    image1on=false, edge1on=false;
  
  // Create our listening socket
  SocketInterface server([&server, &cameras, &initialized,
                          cups, dsb, pwm_leds,
                          &cam0on, &cam1on,
                          &image0on, &edge0on,
                          &image1on, &edge1on,
                          &eventServer](const Message &message) {
    //TODO: Refactor some of this into functions
    debug << "Message received: " << message.header.toString() << std::endl;
    
    std::vector<std::unique_ptr<Message>> resp;

    // We handle management messages here ourselves
    if(message.header.type == Message::Management &&
       message.header.subType == Message::Management.Initialize) {
      // Initialize has been called
      if(!initialized) {
        //debug << "Initializing " << cameras.size() << " cameras" << std::endl;
        //for(CameraInterface &cam : cameras) {
        //  cam.InitializeV4L2();
        //  debug << "Initialized camera " << (void*)&cam << std::endl;
        //}
        

        if(cups != nullptr) {
          cups->Initialize(eventServer);
          debug << "Initialized ChillUPS" << std::endl;
        } else debug << "Skipping cups init" << std::endl;

        if(dsb != nullptr) {
          dsb->Initialize(eventServer);
          debug << "Initialized DSBs" << std::endl;
        } else debug << "Skipping dsb init" << std::endl;
      } else {
        // We are trying to initialize when we already are.  No harm there?        
      }

      // Send back our state and revision here
      int crev = 0;
      if(cups != nullptr) {
        crev =
          ((cups->Major() << 4) & 0x000000F0) |
          ((cups->Minor() << 0) & 0x0000000F);
      }
      
      uint32_t rev =
        ((IV4HAL_MAJOR << 16) & 0x00FF0000) |
        ((IV4HAL_MINOR <<  8) & 0x0000FF00) |
        ((IV4HAL_PATCH <<  0) & 0x000000FF);

      uint32_t drev = 0;
      if(dsb != nullptr) {
        uint32_t dcount = dsb->Count();
        debug << "Found " << dcount << " dsbs" << std::endl;
        drev = dsb->GetVersions();
      }

      resp.push_back(std::unique_ptr<Message>(new Message(Message::Management, Message::Management.Initialize,
                                                          0, drev, crev, rev)));

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
          
          if(message.header.subType == Message::Image.ContinuousCapture) {
            int wcam = message.header.imm[0];
            bool enable = (message.header.imm[2] == 1);
            bool imm = ((message.header.imm[1] & 0x0001) != 0) ? (true) : (false);
            bool edge = ((message.header.imm[1] & 0x0004) != 0) ? (true) : (false);
            int capture_skip = message.header.imm[3];


            if((message.header.imm[1] & 0x0001) != 0) {
              if(wcam == 0) image0on = true;
              if(wcam == 1) image1on = true;
            } else {
              if(wcam == 0) image0on = false;
              if(wcam == 1) image1on = false;
            }
            if((message.header.imm[1] & 0x0004) != 0) {
              if(wcam == 0) edge0on = true;
              if(wcam == 1) edge1on = true;
            } else {
              if(wcam == 0) edge0on = false;
              if(wcam == 1) edge1on = false;
            }

            if(enable) {
              if(wcam == 0) {
                cam0on = true;
                cam1on = false;
              }

              if(wcam == 1) {
                cam0on = false;
                cam1on = true;
              }
            }
            
            debug << "Processing Image message -- continuous :: " <<
              wcam << ":" << enable << ":" << imm << ":" << edge << "::" <<
              cam0on << " - " << cam1on << " - " <<
              image0on << ":" << image1on << " <> " << edge0on << ":" << edge1on << ":" <<
              "skip-" << capture_skip <<              
              std::endl;
            
            // First we have to turn everything off
            for(unsigned int i = 0; i < cameras.size(); i++) {
              cameras[i]->SetupStream(false, false, 0, 0);
            }

            usleep(1000);

            // Then we have to switch the mux gpio
            int cam_number = 0;
            if(cam0on) {
              cam_number = 0;
              RunCommand("echo 0 > /sys/class/gpio/gpio420/value");
            } else if(cam1on) {
              cam_number = 1;
              RunCommand("echo 1 > /sys/class/gpio/gpio420/value");
            }
            
            usleep(1000);

            if(cam0on || cam1on) {
              // Then turn streaming back on -- for both cameras for now
              for(unsigned int i = 0; i < cameras.size(); i++) {
                bool tosend = false;
                if(i == 0 && cam0on && image0on) tosend = true;
                if(i == 0 && cam1on && image1on) tosend = true;
                if(i == 1 && cam0on && edge0on) tosend = true;
                if(i == 1 && cam1on && edge1on) tosend = true;

                debug << "Setting up stream " << i << "::" << tosend << std::endl;
                cameras[i]->SetupStream(true, tosend, cam_number, capture_skip);
              }
            }
            resp.push_back(Message::MakeACK(message));
          } else {
            debug << "Processing Image message -- other" << std::endl;
            int wcam = message.header.imm[0];
            if(wcam >= (int)cameras.size()) {              
              resp.push_back(Message::MakeNACK(message, 0, "Invalid camera"));
            } else {
              resp.push_back(cameras[wcam]->ProcessMessage(message));
            }
          }
        } // ------------ End case Message::Image ------------
        break;

      case Message::CUPS:
        {
          debug << "Processing ChillUPS message" << std::endl;
          if(cups != nullptr) {
            resp.push_back(cups->ProcessMessage(message));
          } else {
            resp.push_back(Message::MakeNACK(message, 0, "iv4hal is running without ChillUPS support"));
          }
        }
        break;

      case Message::DSB:
        {
          debug << "Processing DSB message" << std::endl;
          if(dsb != nullptr) {
            resp.push_back(dsb->ProcessMessage(message));
          } else {
            resp.push_back(Message::MakeNACK(message, 0, "iv4hal is running without DSB support"));
          }
        }
        break;

      case Message::Hardware:
        {
          //TODO: Roll this into its own class/handler
          if(message.header.subType == Message::Hardware.SetLights) {
            uint8_t lightsVal = (int)message.header.imm[0];
            SetLED(lightsVal, pwm_leds);
            resp.push_back(Message::MakeACK(message));
          } else if(message.header.subType == Message::Hardware.GetLights) {
            uint8_t val = GetLED(pwm_leds);
            resp.push_back(unique_ptr<Message>(new Message(Message::Hardware, Message::Hardware.GetLights,
                                                           val, 0, 0, 0)));
          } else if(message.header.subType == Message::Hardware.SetBuzzer) {
            debug << "Setting buzzer: " << (int)message.header.imm[0] << std::endl;
            bool set = (message.header.imm[0] == 0) ? (false) : (true);
            // TODO: don't call out to shell just to write values to GPIO
            if(set) {
              RunCommand("echo 1 > /sys/class/gpio/gpio347/value");
            } else {
              RunCommand("echo 0 > /sys/class/gpio/gpio347/value");
            }
          }

        }
        break;
        
      default:
        resp.push_back(Message::MakeNACK(message, 0, "Unsupported message type"));
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

  // This is for debugging the initialization routines only
  //  If an initialization message is sent, it will result in double initialization
  if(init_on_start) {
    if(cups != nullptr) cups->Initialize(eventServer);
    if(dsb != nullptr) dsb->Initialize(eventServer);
  }

  // Our event loop
  bool done = false;
  bool doorSensor = false;
  int doorSensorFailCount = 0;
  debug << "Entered event loop" << std::endl;
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
    //       That involves going and making sure every read/write is safe in that regard
    //         and bubbling the message up.
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
    
    // TODO: Make this number configurable?  Decide on the best value here
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000 * 25; // 25ms

    int sret = select(maxfd + 1, &sockset, NULL, NULL, &timeout);
    if(sret < 0) {
      // Timed out - just keeping going
    } else {
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
        // What to do when the event server fails?
      }
    }

    if(rs485 != nullptr) {
      // Always run this -- even when not initialized
      rs485->ProcessMainLoop();
    }
    
    // Check to see if we have any cameras here that we need to process
    // TODO: Split this off into a new thread, that way we can reduce latency
    //       on the main communication thread
    // Always call the camera stuff regardless of the select return value
    //  If they don't have anything to do they just return as they are non-blocking
    for(CameraInterface *cam : cameras) {
      cam->ProcessMainLoop(eventServer);
    }

    // Process CUPS next
    if(cups != nullptr && initialized) {
      cups->ProcessMainLoop(eventServer);
    }

    if(dsb != nullptr) {
      dsb->ProcessMainLoop(eventServer, initialized);
    }

    // Finally, check the door sensor
    {      
      ifstream dsin;
      dsin.open("/sys/class/gpio/gpio463/value");
      string ds_buf = "";
      dsin >> ds_buf;
      dsin.close();

      if(ds_buf.length() > 0) {
        bool _doorSensor = (ds_buf[0] == '1');
        if(_doorSensor != doorSensor) {
          Message msg(Message::Hardware, Message::Hardware.DoorEvent,
                      _doorSensor?1:0, 0, 0, 0);
          eventServer.Send(msg);

          doorSensor = _doorSensor;
        }
        
      } else {
        if(doorSensorFailCount++ >= 50) {
          debug << "Failed to read ds" << std::endl;
          doorSensorFailCount = 0;
        }
      }
    }
    
    // TODO: Implement a timeout here so that if we are killed but the sockets dont close
    //  we don't stay around anyway
  }

}
