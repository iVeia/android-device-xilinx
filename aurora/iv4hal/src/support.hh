#ifndef __IV4_UTIL_HH
#define __IV4_UTIL_HH

#include <sys/mount.h>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>

namespace iv4 {
  std::string RunCommand(std::string command);
  std::string RunCommandWithRet(std::string command, int &ret);

  int RemoveFile(const std::string &path);
  int RemoveAllFiles(const std::string &path, bool recursive);

  uint64_t CopyFileData(const std::string &dest, const std::string &src, uint64_t off, uint64_t size,
                        volatile bool *cancel = 0);
  
  //TODO: Consider replacing these with returns of unique_ptr if copying becomes too much
  std::vector<std::string> Split(std::string str, std::string delims);

  uint8_t CalcCRC(const std::vector<uint8_t> &dat);
  
  std::map<std::string,std::string> ToDictionary(std::string param);  
} // end namespace iv4;

#endif
