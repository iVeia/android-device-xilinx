#ifndef __DRAWER_SENSOR_BOARD_HH
#define __DRAWER_SENSOR_BOARD_HH

#include <vector>
#include <ctime>

#include "message.hh"
#include "hardware.hh"
#include "socket_interface.hh"


namespace iv4 {
  class RS485Interface;

  class DSBInterface {

  public:
    DSBInterface(RS485Interface *serial, unsigned int poll_rate_s = 2);
    ~DSBInterface();

    std::unique_ptr<Message>ProcessMessage(const Message &msg);

    bool Initialize(SocketInterface &intf, bool send = true);
    bool ProcessMainLoop(SocketInterface &intf, bool initialized, bool send = true);

    uint32_t GetVersions() const;
    uint32_t Count() const;

    bool ReceiveDrawerEvent(std::vector<uint8_t> &msg);
    bool SelfAssignEvent();

  protected:
    RS485Interface *serial;
    
    // Class to store information on a single drawer sensor board
    struct DSB {
      uint8_t address;
      uint8_t version;     // major.minor (both are nibbles)
      
      bool bootLoaderMode;
      char temperature; // signed 8-bit temp
      uint8_t voltage;     // unsigned 8-bit voltage (in 0.1V increments)

      uint8_t status_byte;
      bool    errors;
      bool    factoryMode;
      bool    proxStatus;
      uint8_t solenoidStatus;   
      bool    gunlock;          // Global unlock status
      bool    lunlock;          // Local unlock status
      uint8_t proxState;
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
    bool drawerOverride(uint8_t index, bool lock);

    bool drawerRecalibration(bool save);

    bool setBootLoaderMode(bool enable);
    
    bool globalLockState;
    bool solenoidManualState;
    bool setGlobalLockState(bool state, bool solManual);
    
    bool globalReset();

    bool factoryModeState;
    bool setFactoryMode(bool state);
    bool getDebugData(uint8_t dsb_index, std::string &ret);

    bool clearDrawerIndices(uint8_t override_val);
    bool assignDrawerIndex(uint8_t index);

    bool getDrawerStatus();
    bool getDrawerTemps();
    bool getErrors(DSB &dsb, std::vector<uint8_t> &errors);

    time_t discoverCountdown;
    static const uint8_t RESET_DISCOVER_WAIT = 1; // Wait 1 second after the last 9A to re-discover
                                                   //  after a reset    

    bool int_send(uint8_t addr, uint8_t type, bool reading,
                  std::vector<uint8_t> &dat);

    struct DrawerEvent {
      uint8_t index;
      uint8_t solenoid;
      uint8_t position;
      bool open;
      bool event;
    };
    std::vector<DrawerEvent> events;

    bool sendEnumEvent;

    time_t tLastUpdate;
    unsigned int dsbUpdateFreq;
    static const uint8_t DSB_UPDATE_FREQ = 2; // Update every 2 seconds

    static const int DEFAULT_TIMEOUT = 100; // 100ms
    
  private:
  }; // end class DSBInterface
  
}; // end namespace iv4

#endif
