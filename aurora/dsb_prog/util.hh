#ifndef __IV_UTIL_HH
#define __IV_UTIL_HH

#pragma once

#include <vector>
#include <stdint.h>


namespace iveia {
  uint8_t CalcCRC(const std::vector<uint8_t> &dat);
  uint8_t GetNibble(char c);
  char    GetNibbleChar(uint8_t u);
}
#endif
