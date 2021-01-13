#ifndef __IVEIOTA_MESSAGE_HH
#define __IVEIOTA_MESSAGE_HH

#include <vector>
#include <cstdint>
#include <memory>
#include <sstream>
#include <arpa/inet.h>

#include "iv4hal.hh"

namespace iv4 {

  class Message {
  public:
    
    //! Management Message
    /*!
      Used to pass information about OTA server operation and flow control issues.
    */
    static struct _ManagementMessage {
      //! Management message type
      constexpr operator uint8_t() const {return  0x01;}
      //! Initialize the HAL system
      /*!
        No parameters on send

        On receive:
        Imm[1] - DSB versions (each byte for one DSB - nibbles: major.minor)
        Imm[2] - Chillups version
        Imm[3] - HAL Server version
      */
      constexpr static uint8_t Initialize        = 0x01;
      //! Gets the boot status of the active (current) container
      /*!
        No parametetrs on send

        On receive:
        Imm[0] - 1 if the system is up, 0 otherwise
      */
      constexpr static uint8_t HALStatus        = 0x10;
      //! Sent by the server to acknowledge a command
      /*!
        imm[0] : Type of ACK'ed command
        imm[1] : SubType of ACK'ed command            
      */
      constexpr static uint8_t ACK               = 0xF0;
      //! Sent by the server to indicate command failure
      //! Sent by the server to acknowledge a command
      /*!
        imm[0] : Type of ACK'ed command
        imm[1] : SubType of ACK'ed command  
        imm[2] : Error code (optional)
        Payload: Error message string (optional)          
      */
      constexpr static uint8_t NACK              = 0xFB;
      
      std::string toString(uint8_t sub) {
        switch(sub) {
        case Initialize: return "Management::Initialize";
        case ACK:        return "Management::ACK";
        case NACK:       return "Management::NACK";
        default:         return "Management::Invalid";
        }
      }
    } Management;
    
    //! HAL Image Message
    /*!
      Send messages to the server having to do with image operations
    */
    static struct _ImageMessage {
      //! HAL image message type
      constexpr operator uint8_t() const {return  0x02;}
      //! Request an image capture
      /*!
        imm[0] : Which camera
        imm[1] : Bitmask of image types
      */
      constexpr static uint8_t CaptureImage      = 0x01;
      //! Enable or disable continuous capture
      /*!
        imm[0] : Which camera
        imm[1] : Bitmask of image types
        imm[2] : 1 to enable, 0 to disable
        imm[3] : Messages to skip before sending
      */
      constexpr static uint8_t ContinuousCapture = 0x02;
      //! Image Captured - Sent from the server to client when a new image has been captured
      /*!
        imm[0] : Which camera
        imm[1] : Bitmask of image types captured
      */
      constexpr static uint8_t ImageCaptured     = 0x08;
      //! Get image - request a single image
      /*!
        imm[0] : Which camera
        imm[1] : Single image type.  If more than one bit is set, this is an error
      */        
      constexpr static uint8_t GetImage          = 0x10;
      //! Send image - Message from the server containing image data
      /*!
        imm[0] : Which camera
        imm[1] : Single image type
        imm[2] : Resolution in the byte form [width_0][width_1][height_0][height_1]
        imm[3] : Current message / Total messages
      */
      constexpr static uint8_t SendImage         = 0x18;
      std::string toString(uint8_t sub) {
        switch(sub) {
        case CaptureImage:      return "Image::CaptureImage";
        case ContinuousCapture: return "Image::ContinuousCapture";
        case ImageCaptured:     return "Image::ImageCaptured";
        case GetImage:          return "Image::GetImage";
        case SendImage:         return "Image::SendImage";
        default:                return "Image::Invalid";
        }
      }
    } Image;

    static struct _CUPSMessage {
      //! HAL cups message type
      constexpr operator uint8_t() const {return  0x03;}
      //! Set temperature set point
      /*!
        imm[0] : Temperature
        imm[1] : Range
      */      
      constexpr static uint8_t SetTemperature       = 0x10;
      //! Get temperature set point
      /*!
        No parameters on send

        On Receive
        imm[0] : Temp set point
        imm[1] : Range
      */
      constexpr static uint8_t GetTemperature       = 0x11;
      //! Get all temperatures
      /*!
        No parameters on send
        
        On Receive
        imm[0] : A count of how many temperatures
        payload: a null-terminated list of temperatures of the form 
          <name>:<value>
      */
      constexpr static uint8_t GetAllTemperatures   = 0x18;
      //! Set defrost settings
      /*!
        imm[0] : defrost period
        imm[1] : defrost length
        imm[2] : defrost limit
      */
      constexpr static uint8_t SetDefrostParams     = 0x20;
      //! Get defrost settings
      /*! No parameters on send

        On receive
        imm[0] : defrost period
        imm[1] : defrost length
        imm[2] : defrost limit
      */
      constexpr static uint8_t GetDefrostParams    = 0x21;
      //! Initiate defrost
      constexpr static uint8_t InitiateDefrost     = 0x22;
      //! Initiate battery test
      constexpr static uint8_t IntiiateBatteryTest = 0x32;
      //! Get all system voltages
      /*!
        No parameters on send

        On receive
        imm[0] : A count of how many voltages
        payload: a null-terminated list of temperatures of the form
          <name>:<value>
      */
      constexpr static uint8_t GetAllVoltages      = 0x38;
      //! Get the battery charge percent
      /*!
        No parameters on send

        On receive
        imm[0] : The current battery percent as an integer
      */
      constexpr static uint8_t GetBatteryPercent   = 0x3A;
      //! Get stored temperatures
      /*!
        No parameters on send
        
        One receive
        imm[0] : How many temperatures were stored
        payload: a null terminated list of temperatures of the form
          <id>:<temp>
      */
      constexpr static uint8_t GetStoredTemperatures = 0x40;
      //! Get probe ids
      /*!
        imm[0] : Number of probes
        
        payload: a null terminated list of ids in the form
                 <probe_name>:<id>
      */
      constexpr static uint8_t GetProbeIDs           = 0x50;
      //! Compressor Error event
      /*!
        imm[0] : Bitfield of errors
      */       
      constexpr static uint8_t CompressorError       = 0xB0;

      //TODO: These
      constexpr static uint8_t Failure               = 0xB2; 
      constexpr static uint8_t BatteryStateChanged   = 0xB4; 
      constexpr static uint8_t ACStateChanged        = 0xB5;
      constexpr static uint8_t TemperatureOutOfRange = 0xB6; 
      
      std::string toString(uint8_t sub) {
        switch(sub) {
        case SetTemperature       : return "CUPS::SetTemperature";
        case GetTemperature       : return "CUPS::GetTemperature";
        case GetAllTemperatures   : return "CUPS::GetAllTemperatures";
        case SetDefrostParams     : return "CUPS::SetDefrostParams";
        case GetDefrostParams     : return "CUPS::GetDefrostParams";
        case InitiateDefrost      : return "CUPS::InitiateDefrost";
        case IntiiateBatteryTest  : return "CUPS::IntiiateBatteryTest";
        case GetAllVoltages       : return "CUPS::GetAllVoltages";
        case GetBatteryPercent    : return "CUPS::GetBatteryPercent";
        case GetStoredTemperatures: return "CUPS::GetStoredTemperatures";
        case GetProbeIDs          : return "CUPS::GetProbeIDs";
        case CompressorError      : return "CUPS::CompressorError";
        case Failure              : return "CUPS::Failure"; 
        case BatteryStateChanged  : return "CUPS::BatteryStateChanged"; 
        case ACStateChanged       : return "CUPS::ACStateChanged";
        case TemperatureOutOfRange: return "CUPS::TemperatureOutOfRange"; 
        default                   : return "CUPS::Invalid";
        }
      }

    } CUPS;

    static struct _DSBMessage {
      //! HAL DSB message type
      constexpr operator uint8_t() const {return  0x04;}
      //! Put all DSBs into BootLoader mode
      /*!
        imm[0] : BLM Mode 1 => Enable, 0 => Disable
      */        
      constexpr static uint8_t SetBootLoaderMode   = 0x04;
      //! Reset all DSBs
      constexpr static uint8_t Reset               = 0x10;
      //! Set global lock
      /*!
        imm[0] : Lock state.  0 => Off, anything else is on
        imm[1] : Solenoid state. 0 => Auto, anything else is manual
      */      
      constexpr static uint8_t SetGlobalLock       = 0x12;
      //! Drawer override
      /*!
        imm[0] : Index of drawer
        imm[1] : 1 - Unlock, Lock otherwise
      */
      constexpr static uint8_t DrawerOverride      = 0x13;
      //! Set factory mode
      /*!
        imm[0] : Factory mode state
      */
      constexpr static uint8_t SetFactoryMode      = 0x14;
      //! Clear drawer indices
      constexpr static uint8_t ClearDrawerIndices  = 0x16;
      //! Assign drawer index
      /*! 
        imm[0] : Index
      */
      constexpr static uint8_t AssignDrawerIndex   = 0x17;
      //! Request calibration
      /*!
        imm[0] : 1 - Save, Anything else - don't save
      */
      constexpr static uint8_t DrawerRecalibration = 0x1A;
      //! Get drawer states
      /*!
        from client: no parameters

        from server:
        imm[0]: Number of drawer states

        payload: a null-terminator delimeted list of:
            <drawer_num>:<solenoid>:<open>:<position>:<temp>:<status_byte>
        where
            drawer_num is the assigned drawer number
            solenoid is 0 => locked, 1 => unlocked, 2 => unlocking (picked), 3 => failed
            open is 1 => opened, 2 => closed
            position is the distance of the drawer from the sensor in mm
                  The maximum is 15mm, so a value of 15 indicates >= 15mm
            temp is the drawer temperature.  
                  Note: This is at the DSB level, so multiple drawers will have the same temp
            status_byte is the byte containing the global/local lock, etc
      */      
      constexpr static uint8_t GetDrawerStates    = 0x20;
      //! Drawer state changed event
      /*!
        imm[0] : Drawer index
        imm[1] : solenoid state (as per get drawer state)
        imm[2] : open state (as per get drawer state)
        imm[3] : position (as per get drawer state)
      */
      constexpr static uint8_t DrawerStateChanged = 0x22;
      //! Get debug data from DSB
      /*!
        imm[0] : Index of DSB to get data from
                  They will be indexed into the DSB array - not the address or drawer index
        
        payload: String of collected debug values
      */
      constexpr static uint8_t GetDebugData       = 0x70;
      //! Get debug data -- TODO
      constexpr static uint8_t GetDebugDataRaw    = 0x7C;
      //! Errors were recorded by a drawer
      /*!
        Sent from the server as an event when an error is detected
        imm[0] - Address or DSB reporting errors
        imm[1] - number of errors
        
        payload: Null-terminator delineated list of error codes
      */
      constexpr static uint8_t DrawerErrors       = 0xA0;

      std::string toString(uint8_t sub) {
        switch(sub) {
        case SetBootLoaderMode  : return "DSB::SetBootLoaderMode";
        case Reset              : return "DSB::Reset";
        case SetGlobalLock      : return "DSB::SetGlobalLock";
        case DrawerOverride     : return "DSB::DrawerOverride";
        case SetFactoryMode     : return "DSB::SetFactoryMode";
        case ClearDrawerIndices : return "DSB::ClearDrawerIndices";
        case AssignDrawerIndex  : return "DSB::AssignDrawerIndex";
        case DrawerRecalibration: return "DSB::DrawerRecalibration";
        case GetDrawerStates    : return "DSB::GetDrawerStates";
        case DrawerStateChanged : return "DSB::DrawerStateChanged";
        case GetDebugData       : return "DSB::GetDebugData";
        default                 : return "DSB::Invalid";
        }
      }
      
    } DSB;

    static struct _LightsMessage {
      //! HAL message to interact with lights
      constexpr operator uint8_t() const {return 0x05;}
      //! Set the state of the lights
      /*!
        imm[0] : The value to set the pot to
      */
      constexpr static uint8_t SetLights    = 0x10;
      //! Get the state of the lights
      /*!
        imm[0] : The current value of the lights
      */
      constexpr static uint8_t GetLights    = 0x20;
      std::string toString(uint8_t sub) {
        switch(sub) {
        case SetLights: return "Lights::SetLights";
        case GetLights: return "Lights::GetLights";
        default       : return "Lights::Invalid";
        }
      }
    } Lights;
    
    //! The message header for HAL messages
    /*!
      The format of the message header is
      03:00 - Sync1 byte consisting of 0x 69 76 34 20 ('i' 'v' '4' ' ')
      07:04 - Sync2 byte consisting of 0x 68 61 6c 00 ('h' 'a' 'l'  0 )
      09:08 - Message protocol revision.  Currently 1
      10:10 - Message type
      11:11 - Message sub type
      15:12 - Immediate 1 value
      19:16 - Immediate 2 value
      23:20 - Immediate 3 value
      27:24 - Immediate 4 value
      31:28 - Payload Length
      35:32 - Checksum (not currently used)
      
      A message will be a header followed by <Payload Length> bytes of payload.
    */
    struct Header {
      uint32_t sync1;         // 'i' 'v' '4' ' ' :: 0x 69 76 34 20
      uint32_t sync2;         // 'h' 'a' 'l'  0  :: 0x 68 61 6c 00
      uint16_t rev;           // Message protocol revision
      uint8_t  type;          // Message type
      uint8_t  subType;       // Message subtype
      uint32_t imm[4];        // 16 bytes (4 integers) of immediate data
      uint32_t pLen;          // Length of payload (in bytes)
      uint32_t checksum;      // Checksum of header
      
      static uint32_t Size(uint16_t rev) {return 36;} // Header is 36 bytes large for rev 1
      uint32_t Size() const { return 36;} // Need to base this on the header rev
      
      explicit Header();
      
      explicit Header(uint8_t type, uint8_t subType);
      
      explicit Header(uint8_t type, uint8_t subType, uint32_t pLen);
      explicit Header(uint8_t type, uint8_t subType, 
                      uint32_t imm1, uint32_t imm2, uint32_t imm3, uint32_t imm4, 
                      uint32_t pLen);
      
      // It is assumed that buffer holds an entire header if sync is true, 
      //  else buf holds an entire header minus the two sync bytes
      explicit Header(const uint8_t *buf, bool sync = true);
      
      std::unique_ptr<uint8_t[]> ToByteArray() const;
      
      std::string toString() const;      
    }; // End Header class
    
    Header        header;
    std::vector<uint8_t> payload;
    
    const static uint8_t sync[];
    const static uint32_t sync1;// = sync[0];
    const static uint32_t sync2;// = sync[4];
    const static uint8_t SyncLength = 8;
    
    const static uint16_t DefaultRev = 1;
    const static uint32_t MaxPayload = IV4HAL_MAX_PACKET_LEN; // 32M to start

    static inline std::unique_ptr<Message> MakeACK(const Message &m) {
        return std::unique_ptr<Message>(new Message(Management, Management.ACK, 
                       m.header.type, m.header.subType, 0, 0));
    }

    static inline std::unique_ptr<Message> MakeNACK(const Message& m, uint32_t errorCode = 0, std::string msg = "") {
        std::vector<uint8_t> payload;
        if(msg.length() > 0) {
            for(char c : msg) payload.push_back((uint8_t)c);
        }
        return std::unique_ptr<Message>(new Message(Management, Management.NACK, 
                       m.header.type, m.header.subType,
                       errorCode, 0, payload));
    }


    Message(uint32_t type, uint32_t subType);
    
    Message(uint8_t type, uint8_t subType, const std::vector<uint8_t> &payload);
    Message(uint8_t type, uint8_t subType, 
            uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4,
            const std::vector<uint8_t> &payload);
    
    Message(uint8_t type, uint8_t subType, 
            uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4);
    
    Message(const Header &header);
    
    Message(const uint8_t *hDat, bool sync = true);
    
    Message();
    
    void AddPayload(uint8_t *data, int len);
    
    std::string toString() const;
    
  };

};

#endif
