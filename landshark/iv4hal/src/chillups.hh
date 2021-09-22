#ifndef __CHILLUPS_HH
#define __CHILLUPS_HH

#include <string>
#include <tuple>
#include <vector>
#include <memory>

#include "socket_interface.hh"
#include "message.hh"

namespace iv4 {

  class RS485Interface;
  
  class ChillUPSInterface {
  public:
    ChillUPSInterface(RS485Interface *serial);
    ~ChillUPSInterface();

    std::unique_ptr<Message> ProcessMessage(const Message &msg);

    bool Initialize(SocketInterface &intf);
    bool ProcessMainLoop(SocketInterface &intf);

  protected:


    // The last time we did an update
    time_t tLastFastUpdate;
    time_t tLastSlowUpdate;


    static const int FAST_UPDATE_FREQ_S = 10;
    static const int SLOW_UPDATE_FREQ_S = (60 * 2);
    time_t chillupsFastUpdateFreq;
    time_t chillupsSlowUpdateFreq;

    bool updateMainStatus(SocketInterface &intf, bool send = true);
    bool updateSlowStatus(SocketInterface &intf, bool send = true);
    
    // RS485 serial interface
    RS485Interface *serial;


    // All the functions to get and set status

    // Keep track of the last time we sent status messages so that we can send them periodically
    static const time_t StatusMessageFreq = 5; // Send status messages every 5 seconds
    time_t lastTORMessage; // The last timne we sent a temp out of range message
    time_t lastBSMessage;  // The last time we sent a Battery state message
    time_t lastACMessage;  // The last time we sent an AC message
    time_t lastCEMessge;   // The last time we sent a compressor error message
    
    // Read from the main status register
    class cupsStatus {
    public:
      // TODO: Clean this up by making everything a function processing _reg?
      //       or maybe a bitfield enum?
      bool acStatus;         // Bit 0
      bool battGood;
      bool battQual;
      bool bootACK;    // Boot status for RS485
      
      bool comprON;      
      bool comprOK;
      bool defrostState;
      bool firmwareState;    // Bit 7

      uint8_t _reg;

      cupsStatus() {
        // Default to firmware state good, sane, everything else off
        // TODO: Think about this more
        set(0x88);
      }

      cupsStatus(uint8_t reg) {
        set(reg);
      }

      void set(uint8_t reg) {
        firmwareState = (reg & 0x80) != 0;
        defrostState = (reg & 0x40) != 0;
        comprOK = (reg & 0x20) != 0;
        comprON = (reg & 0x10) != 0;

        bootACK = (reg & 0x08) != 0;
        battQual = (reg & 0x04) != 0;
        battGood = (reg & 0x02) != 0;
        acStatus = (reg & 0x01) != 0;

        _reg = reg;
      }

      cupsStatus &operator=(const cupsStatus &cups) {
        set(cups._reg);
        return *this;
      }

      bool operator==(const cupsStatus &cups) const {
        return cups._reg == _reg;
      }

      bool operator!=(const cupsStatus &cups) const {
        return !(cups == *this);
      }
    };
    cupsStatus lastMainStatus;
    bool lastMainStatusRead;
    bool getMainStatus(uint8_t &status_reg, bool &);

    float lastCalibratedColdCubeTemp;
    float lastCalibratedAmbientTemp;
    float lastThermistorTemp;
    bool readTemperatures();

    uint8_t lastChargePercent;
    float lastPSU1Voltage;
    float lastPSU2Voltage;
    float lastPSU3Voltage;
    float lastBatteryVoltage;
    float lastChargerVoltage;
    float lastCompressorVoltage;
    bool readVoltages();


    bool readCompressorError(uint8_t &err);  // 0x10
    
    uint8_t lastDefrostLength;
    float lastDefrostTempLimit;
    float lastTempRange;
    float lastSetPoint;
    uint16_t lastDefrostPeriod;
    bool readPersistentSettings();
    bool setTemperature(uint16_t temp, uint8_t range);
    bool setDefrostSettings(uint16_t period, uint8_t length, uint16_t limit);

    bool discover();
    bool acknowledgeBoot(uint8_t &status_byte, bool &);

    uint8_t lastCompressorError;

    struct chillups_version {
      uint8_t major;
      uint8_t minor;
    };
    chillups_version currentVersion;
    
    struct chillups_id {
      uint8_t family;
      uint8_t id[6];
    };
    chillups_id readColdCubeID();
    chillups_id readAmbientID();
    std::string IdToString(chillups_id);

    struct board_config {
      uint8_t id;      // Board id
      bool valid;      // Is this ID valid?
      bool calColdCube; // Calibrated cold cube probe exists
      bool calAmbient; // Calibrated ambient probe exists
    };
    board_config currentBoardConfig;
    bool         readBoardConfig();
    
    std::vector<std::tuple<int, float> > savedTemps;
    bool readRecordedTemps(std::vector<std::tuple<int, float> > &temps);

    class cupsDynamicSettings {
    public:
      bool large_fan;
      bool small_fan;
      bool ambient_fan;
      bool auto_chill;
      bool auto_defrost;
      bool led_backup;
      bool log_enable;

      cupsDynamicSettings(uint8_t b = 0x00) {
        parseByte(b);
      }
      
      void parseByte(uint8_t b) {
        large_fan    = (b&0x80) != 0;
        small_fan    = (b&0x40) != 0;
        ambient_fan  = (b&0x20) != 0;
        auto_chill   = (b&0x08) != 0;
        auto_defrost = (b&0x04) != 0;
        led_backup   = (b&0x02) != 0;
        log_enable   = (b&0x01) != 0;
      }

      uint8_t toByte() {
        uint8_t ret = 0x0;
        if(large_fan)    ret |= 0x80;
        if(small_fan)    ret |= 0x40;
        if(ambient_fan)  ret |= 0x20;
        if(auto_chill)   ret |= 0x08;
        if(auto_defrost) ret |= 0x04;
        if(led_backup)   ret |= 0x02;
        if(log_enable)   ret |= 0x01;

        return ret;
      }
    };
    cupsDynamicSettings currentDynamicSettings;
    bool readDynamicSettings();
    bool setDynamicSettings();

    bool startDefrost();
    bool startBatteryTest();

    bool cupsResetting;
    time_t cupsResettingTime;
    bool globalReset();

    bool getErrorCode(uint8_t &error_code);
    bool getCUPSErrorCodes(std::vector<uint8_t> &);

  private:

  public:
    static const int DEFAULT_TIMEOUT = 100; // 100ms
    
    inline uint8_t Major() const {
      return currentVersion.major;
    }
    inline uint8_t Minor() const {
      return currentVersion.minor;
    }
  };
};
#endif

