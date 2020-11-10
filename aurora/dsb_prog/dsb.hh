#ifndef __IV_DSB_HH
#define __IV_DSB_HH

#pragma once

#include <vector>

namespace iveia {
  
  class DSB {
  public:
    DSB(const std::string &devFile, const std::string &speed, int delay);
    ~DSB();
    
    bool Program(const std::string &hexFile);
    bool IsOpen() const {return (fake || opened);}
    bool DiscoverLight();

    void Verbose(bool v) {verbose = v;}
    void Debug(bool d) {debug = d;}
    
  protected:
    bool opened;
    bool fake;
    bool open();
    std::string device;
    int fd;
    std::string speed;
    int delayMs;
    bool verbose;
    bool debug;
    
    bool dsbs[32];
    
    bool send(uint8_t addr, uint8_t type,
              bool read, std::vector<uint8_t> &dat);
    bool recv(uint8_t &addr, uint8_t &type, std::vector<uint8_t> &msg,
              int timeoutMS, std::vector<uint8_t> &dat);
    bool discoverLight(bool inBLM);
    std::vector<uint8_t> checkErrors();
    
    static const uint8_t MSG_GLOBAL_LOCK_TYPE      = 0x02;

    static const uint8_t MSG_DISCOVER_LIGHT        = 0x09;
    static const uint8_t MSG_DISCOVER_LIGHT_RETURN = 0x89;

    static const uint8_t MSG_GET_STATUS_TYPE       = 0x03;
    static const uint8_t MSG_GET_STATUS_RETURN     = 0x83;
                                                   
    static const uint8_t MSG_GET_ERRORS            = 0x05;
    static const uint8_t MSG_GET_ERRORS_RETURN     = 0x85;
                                                   
    static const uint8_t MSG_SETBLM_TYPE           = 0x70;
    static const uint8_t MSG_HEX_TYPE              = 0x77;

    static const uint8_t ADDR_BROADCAST  = 31;
    static const uint8_t ADDR_DOWNLOAD   = 30;

  };
}
#endif
