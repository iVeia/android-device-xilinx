#ifndef __RS485_INTERFACE_HH
#define __RS485_INTERFACE_HH

#include <string>
#include <vector>

#include "dsb.hh"
#include "chillups.hh"

namespace iv4 {
  
  class RS485Interface {
  public:
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
    static const uint8_t DSB_SELF_ASSIGN_EVENT     = 0x9A;


    static const int DEFAULT_MAINLOOP_TIMEOUT = 5; // 5 ms timeout
    static const int BROADCASTS_PER_LOOP      = 50;
    
    enum class RS485Return {
      Success = 0,

        RecvFailed,
        RecvTimeout,
        RecvTooManyBroadcasts,
        RecvCRCFailure,
        
        SendFailed,
        };
    
    
  public:
    RS485Interface(const std::string &dev);
    ~RS485Interface();

    void SetInterfaces(DSBInterface *dsbs);
    
    bool Send(uint8_t addr,
              uint8_t type,
              bool read,
              std::vector<uint8_t> &dat);
    RS485Return Receive(uint8_t &addr,
                       uint8_t &type,
                       std::vector<uint8_t> &msg,
                       int timeoutMS);
    RS485Return SendAndReceive(uint8_t &addr, uint8_t &type, bool read,
                              std::vector<uint8_t> &msg, int timeoutMS);
    int BytesAvailable();
    bool ProcessMainLoop();

    void DumpSerialPortStats();

    
  protected:
    bool open();
    bool close();
    
    RS485Return ReceiveSingleMessage(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
                                     int timeoutMS);
    
    std::string devFile;
    int fd;

    DSBInterface *dsb_interface;
    
    
    enum class RecvState {
      WaitHeader = 0,
        WaitType,
        ReadPayload,
        WaitCRC,
        };
    
    
  };
  
};
#endif
