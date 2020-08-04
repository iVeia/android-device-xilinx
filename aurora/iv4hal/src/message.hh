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
      //! Initialize the HAL system.  Currently this is done on boot and this command does nothing
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
      */
      constexpr static uint8_t ContinuousCapture = 0x02;
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
        case GetImage:          return "Image::GetImage";
        case SendImage:         return "Image::SendImage";
        default:                return "Image::Invalid";
        }
      }
    } Image;
    
    
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
