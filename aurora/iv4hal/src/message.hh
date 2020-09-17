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
        imm[1] : defrost limit
      */
      constexpr static uint8_t SetDefrostParams     = 0x20;
      //! Get defrost settings
      /*! No parameters on send

        On receive
        imm[0] : defrost period
        imm[1] : defrost limit
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
      //! Get stored temperatures
      /*!
        No parameters on send
        
        One receive
        imm[0] : How many temperatures were stored
        payload: a null terminated list of temperatures of the form
          <id>:<temp>
      */
      constexpr static uint8_t GetStoredTemperatures = 0x40;
      //! Compressor Error event
      /*!
        imm[0] : Bitfield of errors
      */
      constexpr static uint8_t CompressorError      = 0xB0;

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
        case GetStoredTemperatures: return "CUPS::GetStoredTemperatures";
        case CompressorError      : return "CUPS::CompressorError";
        default                   : return "CUPS::Invalid";
        }
      }

    } CUPS;
    
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
