#ifndef __DRAWER_SENSOR_BOARD_HH
#define __DRAWER_SENSOR_BOARD_HH

#include <vector>
#include <ctime>

#include "message.hh"
#include "hardware.hh"
#include "socket_interface.hh"

namespace iv4 {

  class DSBInterface {

  public:
    DSBInterface(std::string dev);
    ~DSBInterface();

    std::unique_ptr<Message>ProcessMessage(const Message &msg);

    bool Initialize(SocketInterface &intf, bool send = true);
    bool ProcessMainLoop(SocketInterface &intf, bool send = true);

    uint32_t GetVersions() const;
    uint32_t Count() const;

  protected:

    std::string _dev;
    bool send(FDManager &fd, uint8_t addr, uint8_t type, bool read,
              std::vector<uint8_t> dat);
    bool _recv(FDManager &fd,
              uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
              int timeoutMS);
    bool recv(FDManager &fd,
              uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
              int timeoutMS = 100);

    // Class to store information on a single drawer sensor board
    struct DSB {
      uint8_t address;
      uint8_t start;    // The first and last drawer index for this dsb
      uint8_t end;      // These are really a function of the address
                        //  but the dummy dsb doesn't work that way
      uint8_t version;     // major.minor (both are nibbles)
      
      bool bootLoaderMode;
      char temperature; // signed 8-bit temp

      bool errors;
      bool factoryMode;
      bool proxStatus;
      uint8_t solenoidStatus;
      bool gunlock;
      bool lunlock;
      struct drawer {
        uint8_t index;          // drawer index
        uint8_t solenoidState;  // State of the locking solenoid
        bool open;              // Is the drawer open
        uint8_t position;       // From position sensor
      };
      std::vector<drawer> drawers;
    };
    std::vector<DSB> dsbs;
    bool discover();

    bool globalLockState;
    bool setGlobalLockState(bool state);
    bool globalReset();

    bool factoryModeState;
    bool setFactoryMode(bool state);

    bool clearDrawerIndices();
    bool assignDrawerIndex(uint8_t index);

    // TODO: I think this should be part of OTA?
    bool updateFirmware(std::string fw_path);
    
    bool getDrawerStatus();
    bool getDrawerTemps();
    bool getErrors(DSB &dsb, std::vector<uint8_t> &errors);

    time_t discoverCountdown;
    static const uint8_t RESET_DISCOVER_WAIT = 5; // Wait 5 seconds to re-discover
                                                  //  after a reset    

    struct DrawerEvent {
      uint8_t index;
      uint8_t solenoid;
      uint8_t position;
      bool open;
      bool event;
    };
    std::vector<DrawerEvent> events;

    time_t tLastUpdate;
    static const uint8_t DSB_UPDATE_FREQ = 2; // Update every 2 seconds
    
  private:
    static const uint8_t BROADCAST_ADDRESS = 31;

    static const uint8_t GET_STATUS_TYPE   = 0x03;
    static const uint8_t GET_STATUS_RETURN = 0x83;

    static const uint8_t GET_TEMP_TYPE     = 0x04;
    static const uint8_t GET_TEMP_RETURN   = 0x84;
                                           
    static const uint8_t GET_ERRORS_TYPE   = 0x05;
    static const uint8_t GET_ERRORS_RETURN = 0x85;

    static const uint8_t GLOBAL_RESET_TYPE = 0x06;

    static const uint8_t FACTORY_MODE_TYPE = 0x20;

    static const uint8_t DRAWER_STATE_CHANGE_EVENT = 0x99;
    
  }; // end class DSBInterface
  
}; // end namespace iv4

#endif
