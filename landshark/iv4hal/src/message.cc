#include "message.hh"

namespace iv4 {

  // Static message types to help with type safety
  struct Message::_ManagementMessage     iv4::Message::Management;
  struct Message::_ImageMessage          iv4::Message::Image;
  struct Message::_CUPSMessage           iv4::Message::CUPS;
  struct Message::_DSBMessage            iv4::Message::DSB;
  struct Message::_HardwareMessage       iv4::Message::Hardware;
  
  std::string Message::Header::toString() const {
    std::stringstream ss;
    switch(type) {
    case Message::Management:     ss << Message::Management.toString(subType);     break;
    case Message::Image:          ss << Message::Image.toString(subType);          break;
    case Message::CUPS:           ss << Message::CUPS.toString(subType);           break;
    case Message::DSB:            ss << Message::DSB.toString(subType);            break;
    case Message::Hardware:       ss << Message::Hardware.toString(subType);       break;
    default: return "Invalid:Invalid";
    }
    
    // Add in the immediate parameters
    ss << " (" << imm[0] << ")(" << imm[1] << ")(" << imm[2] << ")(" << imm[3] << ") : " << pLen;
    
    // Extract and return the string
    return ss.str();
  }

  // Our sync message
  const uint8_t  Message::sync[] = {'i', 'v', '4', ' ', 'h', 'a', 'l', '\0'};
  const uint32_t Message::sync1 = (sync[0] << 24) | (sync[1] << 16) | (sync[2] << 8) | sync[3];
  const uint32_t Message::sync2 = (sync[4] << 24) | (sync[5] << 16) | (sync[6] << 8) | sync[7];
  
  
  // Convenience constructors
  Message::Header::Header() : Header((uint8_t)0, (uint8_t)0) {}  
  Message::Header::Header(uint8_t type, uint8_t subType) : 
    Header(type, subType, 0, 0, 0, 0, 0) {}  
  Message::Header::Header(uint8_t type, uint8_t subType, uint32_t pLen) : 
    Header(type, subType, 0, 0, 0, 0, pLen) {}
  
  // The main constructor for the Header class
  Message::Header::Header(uint8_t type, uint8_t subType, 
                          uint32_t imm1, uint32_t imm2, uint32_t imm3, uint32_t imm4, 
                          uint32_t pLen) :
    sync1(Message::sync1), sync2(Message::sync2), 
    rev(Message::DefaultRev), type(type), subType(subType),
    imm{imm1, imm2, imm3, imm4}, pLen(pLen) {
      // TODO: Calculate checksum
      checksum = 0;
    }
  
  // Construct a header from a sequence of bytes
  // It is assumed that buffer holds an entire header if sync is true, 
  //  else buf holds an entire header minus the two sync bytes
  Message::Header::Header(const uint8_t *buf, bool sync) {
    int i = 0;
    if(sync) {
      // Parse out the sync bytes
      sync1 = ntohl(*(uint32_t*)(buf + i)); i+=4; 
      sync2 = ntohl(*(uint32_t*)(buf + i)); i+=4;
    } else {
      // It is assumed the parser found the sync bytes, so we can just insert them here
      sync1 = Message::sync1;
      sync2 = Message::sync2;
    }
    rev   = ntohs(*(uint32_t*)(buf + i)); i+=2;
    
    // Sanity check
    if(sync1 != Message::sync1 || sync2 != Message::sync2) throw "Invalid Message Sync";
    if(rev   != Message::DefaultRev)                       throw "Invalid Message Revision";
    
    // Sanity check passed, so parse stuff out
    type     = buf[i++];
    subType  = buf[i++];
    imm[0]   = ntohl(*(uint32_t*)(buf + i)); i+=4;
    imm[1]   = ntohl(*(uint32_t*)(buf + i)); i+=4;
    imm[2]   = ntohl(*(uint32_t*)(buf + i)); i+=4;
    imm[3]   = ntohl(*(uint32_t*)(buf + i)); i+=4;
    pLen     = ntohl(*(uint32_t*)(buf + i)); i+=4;
    checksum = ntohl(*(uint32_t*)(buf + i)); i+=4;
    
    // More sanity checks
    if(pLen > Message::MaxPayload) throw "Payload too large";
  }

  // Convert this header to an array of bytes
  std::unique_ptr<uint8_t[]> Message::Header::ToByteArray() const {
    // Make sure this is a header we can send
    // TODO: Should I check for all valid types here?  Then we have to keep it in
    //       sync every time we add/remove a message
    if(type == 0 || subType == 0) return std::unique_ptr<uint8_t[]>(nullptr);

    // TODO: I cast this to a uint32_t*, so it has to be 4-byte aligned.
    //       This has never been an issue, but it should be checked
    int headerLen = Size(Message::DefaultRev);
    std::unique_ptr<uint8_t[]> ret(new uint8_t[headerLen]);
    uint8_t *_ret = ret.get();
    
    // Now to push everything into the buffer
    int i = 0;
    *(uint32_t*)(_ret + i) = htonl(sync1); i+= 4;
    *(uint32_t*)(_ret + i) = htonl(sync2); i+= 4;
    *(uint16_t*)(_ret + i) = htons(rev);   i+= 2;
    ret[i++] = type;
    ret[i++] = subType;
    for(int j = 0; j < 4; j++) {
      *(uint32_t*)(_ret + i) = htonl(imm[j]); i+= 4;
    }
    *(uint32_t*)(_ret + i) = htonl(pLen);     i+= 4;
    *(uint32_t*)(_ret + i) = htonl(checksum); i+= 4;
    
    return ret;
  }
  
  // Convenience constructors -- These mainly just pass information on to the
  //  header constructor
  Message::Message(uint32_t type, uint32_t subType) : header(type, subType) {}  
  Message::Message(uint8_t type, uint8_t subType, const std::vector<uint8_t> &payload) : 
    header(type, subType, payload.size()), payload(payload) {}  
  Message::Message(uint8_t type, uint8_t subType, 
                   uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4) : 
    header(type, subType, i1, i2, i3, i4, 0) {}  
  Message::Message(const Header &header) : header(header) {}
  Message::Message(const uint8_t *hDat, bool sync) : header(hDat, sync) {}
  Message::Message() : header() {}
  Message::Message(uint8_t type, uint8_t subType, 
                   uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4,
                   const std::vector<uint8_t> &payload) : 
    header(type, subType, i1, i2, i3, i4, payload.size()), payload(payload) {}
  
  // Add len bytes from data to the payload
  void Message::AddPayload(uint8_t *data, int len) {
    for(int i = 0; i < len; i++) {
      payload.push_back(data[i]);
    }
    header.pLen = payload.size();
  }

  // Convert this message to a string
  //  TODO: maybe extract some of the payload for viewing if it looks like a string?
  std::string Message::toString() const {
    std::string ret = header.toString();
    ret += " : + payload";
    
    return ret;
  }
};
