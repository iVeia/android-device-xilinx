#ifndef __RS485_INTERFACE_HH
#define __RS485_INTERFACE_HH

#include <string>
#include <vector>
#include <chrono>

#include "dsb.hh"
#include "chillups.hh"

namespace iv4 {
  
  class RS485Interface {
  public:
    static const uint8_t BROADCAST_ADDRESS            = 31;
    static const uint8_t CUPS_ADDRESS                 = 14;
                                                      
    static const uint8_t DISCOVERY                    = 0x01;
    static const uint8_t DISCOVERY_RETURN             = 0x81;
    static const uint8_t GLOBAL_RESET                 = 0x06;
    static const uint8_t FACTORY_MODE                 = 0x20;
                                                      
    static const uint8_t DSB_GLOBAL_LOCK              = 0x02;
    static const uint8_t DSB_GET_STATUS               = 0x03;
    static const uint8_t DSB_GET_STATUS_RETURN        = 0x83;
    static const uint8_t DSB_GET_TEMP                 = 0x04;
    static const uint8_t DSB_GET_TEMP_RETURN          = 0x84;
    static const uint8_t DSB_GET_ERRORS               = 0x05;
    static const uint8_t DSB_GET_ERRORS_RETURN        = 0x85;
    static const uint8_t DSB_DRAWER_RECALIBRATION     = 0x07;
    static const uint8_t DSB_DRAWER_OVERRIDE          = 0x08;
    static const uint8_t DSB_CLEAR_INDICES            = 0x21;
    static const uint8_t DSB_ASSIGN_INDEX             = 0x22;
    static const uint8_t DSB_GET_DEBUG                = 0x51;
    static const uint8_t DSB_GET_DEBUG_RETURN         = 0xD1;

    static const uint8_t CUPS_GET_STATUS              = 0x60;
    static const uint8_t CUPS_GET_PSETTINGS           = 0x61;
    static const uint8_t CUPS_GET_DSETTINGS           = 0x62;
    static const uint8_t CUPS_GET_TEMPERATURE         = 0x63;
    static const uint8_t CUPS_GET_VOLTAGE             = 0x64;
    static const uint8_t CUPS_GET_CAL_PROBE_ID        = 0x65;
    static const uint8_t CUPS_GET_LOGGED_TEMP         = 0x66;
    static const uint8_t CUPS_GET_COMPR_ERROR         = 0x67;
    static const uint8_t CUPS_RESET                   = 0x6C;
    
    static const uint8_t CUPS_GET_STATUS_RETURN       = 0xE0;
    static const uint8_t CUPS_GET_PSETTINGS_RETURN    = 0xE1;
    static const uint8_t CUPS_GET_DSETTINGS_RETURN    = 0xE2;
    static const uint8_t CUPS_GET_TEMPERATURE_RETURN  = 0xE3;
    static const uint8_t CUPS_GET_VOLTAGE_RETURN      = 0xE4;
    static const uint8_t CUPS_GET_CAL_PROBE_ID_RETURN = 0xE5;
    static const uint8_t CUPS_GET_LOGGED_TEMP_RETURN  = 0xE6;
    static const uint8_t CUPS_GET_COMPR_ERROR_RETURN  = 0xE7;

    
    static const uint8_t CUPS_SET_DSETTINGS           = 0x68;
    static const uint8_t CUPS_SET_TEMPERATURE         = 0x69;
    static const uint8_t CUPS_SET_DEFROST             = 0x6A;
    static const uint8_t CUPS_INITIATE_OPERATION      = 0x6B;
                                                      
    static const uint8_t BOOTLOADER_MODE              = 0x70;
                                                      
    static const uint8_t DRAWER_STATE_CHANGE_EVENT    = 0x99;
    static const uint8_t DSB_SELF_ASSIGN_EVENT        = 0x9A;


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
