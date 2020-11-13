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
    DSBInterface(std::string dev, unsigned int poll_rate_s = 2);
    ~DSBInterface();

    std::unique_ptr<Message>ProcessMessage(const Message &msg);

    bool Initialize(SocketInterface &intf, bool send = true);
    bool ProcessMainLoop(SocketInterface &intf, bool send = true);

    uint32_t GetVersions() const;
    uint32_t Count() const;

  protected:
    int checkCount;
    bool enableCRC; // Send CRCs and check on receipt
    
    std::string _dev;
    int devFD;
    bool send(uint8_t addr, uint8_t type, bool read,
              std::vector<uint8_t> dat);
    bool _recv(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
              int timeoutMS);
    bool recv(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
              int timeoutMS = 100);

    // Class to store information on a single drawer sensor board
    struct DSB {
      uint8_t address;
      uint8_t version;     // major.minor (both are nibbles)
      
      bool bootLoaderMode;
      char temperature; // signed 8-bit temp

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

    bool clearDrawerIndices();
    bool assignDrawerIndex(uint8_t index);

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
    unsigned int dsbUpdateFreq;
    static const uint8_t DSB_UPDATE_FREQ = 1; // Update every 2 seconds
    
  private:
    static const uint8_t BROADCAST_ADDRESS         = 31;

    static const uint8_t DISCOVERY_TYPE            = 0x01;
    static const uint8_t DISCOVERY_RETURN          = 0x81;

    static const uint8_t GLOBAL_LOCK_TYPE          = 0x02;

    static const uint8_t GET_STATUS_TYPE           = 0x03;
    static const uint8_t GET_STATUS_RETURN         = 0x83;
                                                   
    static const uint8_t GET_TEMP_TYPE             = 0x04;
    static const uint8_t GET_TEMP_RETURN           = 0x84;
                                                   
    static const uint8_t GET_ERRORS_TYPE           = 0x05;
    static const uint8_t GET_ERRORS_RETURN         = 0x85;
                                                   
    static const uint8_t GLOBAL_RESET_TYPE         = 0x06;

    static const uint8_t DRAWER_RECALIBRATION_TYPE = 0x07;

    static const uint8_t DRAWER_OVERRIDE_TYPE      = 0x08;
    
    static const uint8_t FACTORY_MODE_TYPE         = 0x20;
    static const uint8_t CLEAR_INDICES_TYPE        = 0x21;
    static const uint8_t ASSIGN_INDEX_TYPE         = 0x22;

    static const uint8_t GET_DEBUG_TYPE            = 0x51;
    static const uint8_t GET_DEBUG_RETURN          = 0xD1;
    
    static const uint8_t BOOTLOADER_MODE_TYPE      = 0x70;

    static const uint8_t DRAWER_STATE_CHANGE_EVENT = 0x99;
    
  }; // end class DSBInterface
  
}; // end namespace iv4

#endif
