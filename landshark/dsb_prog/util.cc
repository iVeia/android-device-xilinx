
#include "util.hh"

namespace iveia {

uint8_t CalcCRC(const std::vector<uint8_t> &dat) {
  int len = dat.size();

  uint8_t crc = 0;
  
  for (int i = 0; i < len; i++) {
  //get a byte to work with
    uint8_t byte = dat[i];
    
    //roll over its bits making CRC magic
    for (int bit = 0; bit < 8; bit++) {
      uint8_t mix = (crc ^ byte) & 0x01;
      crc >>= 1;
      if(mix) {
        crc ^= 0x8C;
      }
      
      byte >>= 1;
    }
  }
  
  return crc;
}

  char GetNibbleChar(uint8_t u) {
    if(u >= 0 && u <= 9) return u + '0';
    else if(u >= 0x0A && u <= 0x0F) return u + 'A';
    else return 'X';
  }
  
  uint8_t GetNibble(char c) {
    if(c >= '0' && c <= '9') return (c - '0');
    switch(c) {
    case 'a': case 'A': return 0x0A;
    case 'b': case 'B': return 0x0B;
    case 'c': case 'C': return 0x0C;
    case 'd': case 'D': return 0x0D;
    case 'e': case 'E': return 0x0E;
    case 'f': case 'F': return 0x0F;
    default: return 0xF0;
    }
  }
  
}
