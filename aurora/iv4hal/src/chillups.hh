#ifndef __CHILLUPS_HH
#define __CHILLUPS_HH

#include <string>
#include <tuple>
#include <vector>
#include <memory>

#include "socket_interface.hh"
#include "message.hh"

namespace iv4 {
  typedef std::tuple<bool, uint16_t> i2c_u16;
  typedef std::tuple<bool, int16_t>  i2c_i16;
  typedef std::tuple<bool, uint8_t>  i2c_u8;
  typedef std::tuple<bool, float>    i2c_f;
  typedef std::tuple<bool, bool>     i2c_b;
  
  
  class ChillUPSInterface {
  public:
    ChillUPSInterface(std::string dev);
    ~ChillUPSInterface();

    std::unique_ptr<Message> ProcessMessage(const Message &msg);

    bool Initialize(SocketInterface &intf);
    bool ProcessMainLoop(SocketInterface &intf);

  protected:


    // The last time we did an update
    time_t tLastFastUpdate;
    time_t tLastSlowUpdate;

    time_t chillupsFastUpdateFreq;
    time_t chillupsSlowUpdateFreq;

    bool updateMainStatus(SocketInterface &intf, bool send = true);
    bool updateSlowStatus(SocketInterface &intf, bool send = true);
    
    // Keep track of whether the i2c device node is opened or not
    //  we don't want to keep it open all the time becuase it may
    //  cause issues talking to the camera
    std::string _dev;
    int i2cfd;
    bool opened() const;
    bool open();
    bool close();


    // All the functions to get and set status

    // Read from the main status register (i2c:0x60)
    class cupsStatus {
    public:
      // TODO: Clean this up by making everything a function processing _reg?
      bool acStatus;         // Bit 0
      bool battQual;
      bool battLow;
      bool sane;
      
      bool tempOutRange;      
      bool comprError;
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
        comprError = (reg & 0x20) != 0;
        tempOutRange = (reg & 0x10) != 0;

        sane = (reg & 0x08) != 0;
        battLow = (reg & 0x04) != 0;
        battQual = (reg & 0x02) != 0;
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
    bool auto_chill = true;
    i2c_u8 getMainStatus();

    // Read from the individual status registers (i2c:0x64)
    //                             // REgister number
    float lastThermistorTemp;
    bool   readThermistorTemp();   // 0x00

    uint16_t lastDefrostPeriod;
    bool     readDefrostPeriod();    // 0x02
    bool     setDefrostPeriod(uint16_t);

    uint8_t lastChargePercent;
    bool    readChargePercent();    // 0x04

    float lastSupplyVoltage;
    bool  readSupplyVoltage();    // 0x05

    float lastBatteryVoltage;
    bool  readBatteryVoltage();   // 0x06

    float lastBackplaneVoltage;
    bool  readBackplaneVoltage(); // 0x07

    float lastOtherVoltage;
    bool  readOtherVoltage();     // 0x08 TODO: Do I need to care which this is?

    float lastTempRange;
    bool  readTempRange();        // 0x09
    bool  setTempRange(float);

    float lastSetPoint;
    bool  readSetPoint();         // 0x0E
    bool  setSetPoint(float);

    uint8_t lastCompressorError;
    i2c_u8  readCompressorError();  // 0x10

    struct chillups_version {
      uint8_t major;
      uint8_t minor;
    };
    chillups_version currentVersion;
    bool readVersion();      // 0x11

    uint8_t lastDefrostLength;
    bool    readDefrostLength();          // 0x12
    bool    setDefrostLength(uint8_t);

    bool lastCompressorBackupState;
    bool readCompressorBackupState();  // 0x13
    bool setCompressorBackupState(bool state);

    bool lastTempLoggingState;
    bool readTempLoggingState();       // 0x14
    bool setTempLoggingState(bool state);

    float lastDefrostTempLimit;
    bool  readDefrostTempLimit();       // 0x16
    bool  setDefrostTempLimit(float);

    float lastCalibratedColdCubeTemp;
    bool  readCalibratedColdCubeTemp(); // 0x18

    float lastCalibratedAmbientTemp;
    bool  readCalibratedAmbientTemp();  // 0x21

    struct chillups_id {
      uint8_t family;
      uint8_t id[6];
    };
    chillups_id readColdCubeID();  // 0x1A
    chillups_id readAmbientID();   // 0x23

    struct board_config {
      uint8_t id;      // Board id
      bool valid;      // Is this ID valid?
      bool calColdCube; // Calibrated cold cube probe exists
      bool calAmbient; // Calibrated ambient probe exists
    };
    board_config currentBoardConfig;
    bool         readBoardConfig();  // 0x2A
    
    // 0x0A - holds the temp index number
    // 0x0C - holds the temp value
    std::vector<std::tuple<int, float> > savedTemps;
    bool readRecordedTemps(std::vector<std::tuple<int, float> > &temps);
  
  private:

  public:
    
    inline uint8_t Major() const {
      return currentVersion.major;
    }
    inline uint8_t Minor() const {
      return currentVersion.minor;
    }
  };
};
#endif

