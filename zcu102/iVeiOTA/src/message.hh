#ifndef __IVEIOTA_MESSAGE_HH
#define __IVEIOTA_MESSAGE_HH

#include <vector>
#include <cstdint>
#include <memory>
#include <sstream>
#include <arpa/inet.h>

namespace iVeiOTA {

  class Message {
  public:
    
    //! Management Message
    /*!
      Used to pass information about OTA server operation and flow control issues.
    */
    static struct _ManagementMessage {
      //! Management message type
      constexpr operator uint8_t() const {return  0x01;}
      //! Initialize the OTA system.  Currently this is done on boot and this command does nothing
      constexpr static uint8_t Initialize        = 0x01;
      //! Gets the boot status of the active (current) container
      /*!
        No parametetrs on send

        On receive:
        Imm[0] - 1 if the system was updated, 0 otherwise
      */
      constexpr static uint8_t BootStatus        = 0x10;
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
    
    //! OTA Update Message
    /*!
      Send messages to the server having to do with update operations
    */
    static struct _OTAUpdateMessage {
      //! OTA update message type
      constexpr operator uint8_t() const {return  0x02;}
      //! Begin an update
      /*!
        imm[0] : Where the manifest is stored.  0 - In the payload.  1 - On the filesystem
        Payload: Either the manifest or a filesystem path based on imm[0]
      */
      constexpr static uint8_t BeginUpdate       = 0x01;
      //! Continue an interrupted update
      constexpr static uint8_t ContinueUpdate    = 0x08;
      //! Cancel an update in progress.  A canceled update cannot be continued
      constexpr static uint8_t CancelUpdate      = 0x10;
      //! Process an update chunk
      /*!
        imm[0] : Where the chunk data is stored.  0 - In the payload.  1 - On the filesystem
        imm[1] : If imm[0] is 1, the offset in the payload where the data starts
        imm[2] : If imm[0] is 1, the offset into the chunk where the data starts
        Payload: The chunk identifier as null-terminated string, followed by either
        chunk data or the path to the chunk file
      */
      constexpr static uint8_t ProcessChunk      = 0x20;
      //! Finalize an update.  After this is called a reboot will booth into the updated container
      constexpr static uint8_t Finalize          = 0x30;
      std::string toString(uint8_t sub) {
        switch(sub) {
        case BeginUpdate:    return "OTAUpdate::BeginUpdate";
        case ContinueUpdate: return "OTAUpdate::ContinueUpdate";
        case CancelUpdate:   return "OTAUpdate::CancelUpdate";
        case ProcessChunk:   return "OTAUpdate::ProcessChunk";
        case Finalize:       return "OTAUpdate::Finalize";
        default:             return "OTAUpdate::Invalid";
        }
      }
    } OTAUpdate;
    
    //! OTA status message
    /*!
      Messages to query status of the OTA process
    */
    static struct _OTAStatusMessage {
      //! OTA Status Message type
      constexpr operator uint8_t() const {return  0x03;}
      //! Get status on the OTA sytem itself - Not currently used
      /*!
        imm[0] : How many key/value pairs are in the payload
        Payload: Comma delimited list of key:value pairs consisting of the following information
      */
      constexpr static uint8_t OTAStatus         = 0x01;
      //! Get the status of the current update and the chunks needed for it
      /*!
        imm[0] - Current OTA state
        0 - Idle
        1 - Update not started but can be continued
        2 - Update is being initialized
        3 - Update is being prepared (containers being copied)
        4 - Update is ready
        5 - Update is processing a chunk
        6 - All chunks have been processed
        imm[1] - 1 if all chunks passed successfully, 0 otherwise
        Payload: The chunk that is currently being processed, if status == 5
      */
      constexpr static uint8_t UpdateStatus      = 0x10;
      //! Get status on the processing of the current chunk
      /*!
        imm[0] - The number of chunks needed for this update

        Payload: 
      */       
      constexpr static uint8_t ChunkStatus       = 0x20;
      std::string toString(uint8_t sub) {
        switch(sub) {
        case OTAStatus:    return "OTAStatus::OTAStatus";
        case UpdateStatus: return "OTAStatus::UpdateStatus";
        case ChunkStatus:  return "OTAStatus::ChunkStatus";
        default:           return "OTAStatus::Invalid";
        }
      }
    } OTAStatus;
    
    //! Boot management message
    /*!
      Messages dealing with the boot process
      Container identifier values are:
      1 - Current container
      2 - Alternate container
      3 - Recovery container (not currently updatable)
    */
    static struct _BootManagementMessage {
      //! Boot Management Message type
      constexpr operator uint8_t() const {return  0x04;}
      //! Write the container info to its BootInfo partition
      /*!
        Will likely be deprecated as this should be processed internally
        imm[0] : Container identifier
      */
      constexpr static uint8_t WriteContainerInfo = 0x04;
      //! Modify the BootInfo partition so that the alternate and current containers switch
      /*!
        This is intended more for debugging purposes 
        It will set the alternate container revision to the current container revision + 1
        but it will leave the other flags alone.  So if the alternate container is invalid
        it will not boot
      */
      constexpr static uint8_t SwitchContainer    = 0x10;
      //! Set the validity of the container
      /*!
        imm[0] : Container identifier
        imm[0] : 0 - Invalid, 1 - Valid
      */
      constexpr static uint8_t SetValidity        = 0x20;
      //! Clears the update flag from the BootInfo
      /*!
        imm[0] : Container identifier
      */
      constexpr static uint8_t MarkUpdateSuccess  = 0x24;
      //! Resets the boot flag of the container back to zero
      /*!
        imm[0] : Container identifier
      */
      constexpr static uint8_t ResetBootCount     = 0x28;
      std::string toString(uint8_t sub) {
        switch(sub) {
        case WriteContainerInfo: return "BootManagement::WriteContainerInfo";
        case SwitchContainer:    return "BootManagement::SwitchContainer";
        case SetValidity:        return "BootManagement::SetValidity";
        case MarkUpdateSuccess:  return "BootManagement::MarkUpdateSuccess";
        case ResetBootCount:     return "BootManagement::ResetBootCount";
        default:                 return "BootManagement::Invalid";
        }
      }
    } BootManagement;
    
    //! The message header for OTA messages
    /*!
      The format of the message header is
      03:00 - Sync1 byte consisting of 0x 69 56 65 69 ('i' 'V' 'e' 'i')
      07:04 - Sync2 byte consisting of 0x 4F 54 41 00 ('O' 'T' 'A'  0 )
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
      uint32_t sync1;         // 'i' 'V' 'e' 'i' :: 0x 69 56 65 69
      uint32_t sync2;         // 'O' 'T' 'A'  0  :: 0x 4F 54 41 00
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
    
    const static uint8_t sync[];// = {0x69, 0x56, 0x65, 0x69, 0x4f, 0x54, 0x41, 0x00};
    const static uint32_t sync1;// = sync[0];
    const static uint32_t sync2;// = sync[4];
    const static uint8_t SyncLength = 8;
    
    const static uint16_t DefaultRev = 1;
    const static uint32_t MaxPayload = 1024 * 1024 * 16; // 16M to start

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
