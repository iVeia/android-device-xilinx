#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <array>

#include "support.hh"
#include "debug.hh"
#include "iv4_hal.hh"

namespace iv4 {
  std::string RunCommandWithRet(std::string command, int &ret) {
    int retVal = -1;
    
    // Where to keep the response from the program we are running
    std::array<char, 256> buffer;
    std::string result;
    debug << "Running command with return value: " << command << std::endl;

    // Run the program and open a pipe to it to get the output
    debug << "Command string is " << command.c_str() << std::endl;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
      debug << Debug::Mode::Err << "Failed to open pipe to run command " << command << std::endl << Debug::Mode::Info;
      return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    retVal = pclose(pipe);
    debug << "pclose returned " << retVal;
    
    if(retVal == -1) {
      debug << Debug::Mode::Err << "Command returned -1.  Unsure if that is return value or error." << std::endl;
    }
    ret = retVal;
    
    debug << Debug::Mode::Info << "Run command: " << command << " exited with " << ret << " with output " << result << std::endl;
    return result;
  }
  
  std::string RunCommand(std::string command) {
    // Where to keep the response from the program we are running
    std::array<char, 256> buffer;
    std::string result;
    debug << "Running command: " << command << std::endl;

    // Run the program and open a pipe to it to get the output
    debug << "Command string is " << command.c_str() << std::endl;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
      debug << Debug::Mode::Err << "Failed to open pipe to run command " << command << std::endl << Debug::Mode::Info;
      return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }
    debug << Debug::Mode::Info << "Run command: " << command << " with output " << result << std::endl;
    return result;
  }
  
  uint64_t CopyFileData(const std::string &dest, const std::string &src,
                        uint64_t offset, uint64_t len,
                        volatile bool *cancel) {
    debug << Debug::Mode::Debug << "Made it here" << std::endl;
    uint64_t totalWritten = 0;
    bool copyAll = (len == 0);
    try {
      debug << Debug::Mode::Debug << "Copying from " << src << " to " << dest << std::endl;
      // TODO: This seems too easy.  Go back and double check all this
      int inf = open(src.c_str(), O_RDONLY);
      int otf = open(dest.c_str(), O_WRONLY);
      debug << "Seeking" << std::endl;
      int res = lseek(otf, offset, SEEK_SET);

      debug << "Starting: " << inf << ":" << otf << ":" << res << std::endl;
      if(inf < 0 || otf < 0 || res < 0) return 0;
      
      char buf[1024*1024];
      uint64_t remaining = len;
      int printCount = 0;
      while((copyAll || remaining > 0) && 
            (cancel != nullptr && !(*cancel))) {
        uint64_t toRead = (uint64_t)1024*1024;
        if(!copyAll) toRead = std::min(toRead, remaining);
        
        size_t bread = read(inf, buf, toRead);
        size_t wrote = write(otf, buf, bread);

        if(wrote != bread) {
          debug << "Wrote different value than read" << std::endl;
          break;
        }
        
        totalWritten += bread;
        if(!copyAll) remaining -= bread;

        if(bread != toRead || bread == 0) break;
        if((printCount++ % 100) == 0) {
          debug << "Copying " << totalWritten << std::endl;
          printCount = 1;
        }
      } // end while
      close(inf);
      close(otf);
    } catch(...) {
      
    }
    debug << "After: " << totalWritten << std::endl;
    return totalWritten;
  }

  bool IsDir(std::string dir_path) {
    struct stat ss;

    // stat returns info about the endpoint of a symlink
    //  use lstat here so that we don't follow symlinks
    if (lstat(dir_path.c_str(), &ss) == 0 && S_ISDIR(ss.st_mode)) {
      return true;
    } else {
      return false;
    }
  }

  int RemoveFile(const std::string &path) {
    debug << Debug::Mode::Debug << "Trying to remove " << path << std::endl;
    // Just try and remove it
    if(remove(path.c_str()) < 0) return errno;
    else return 0;
  }
  
  int RemoveAllFiles(const std::string &path, bool recursive) {
    if (path.empty()) return 0;

    debug << Debug::Mode::Debug << "Trying to remove all files in " << path << std::endl;
        
    DIR *theFolder = opendir(path.c_str());
    struct dirent *next_file;

    // TODO: Go through and replace this with std::string so there isn't a limit on
    //       path length
    char filepath[1024];
    int ret_val;

    if (theFolder == NULL) return errno;

    while ( (next_file = readdir(theFolder)) != NULL ) {
      // build the path for each file in the folder
      sprintf(filepath, "%s/%s", path.c_str(), next_file->d_name);
      
      //we don't want to process the pointer to "this" or "parent" directory
      if ((strcmp(next_file->d_name,"..") == 0) ||
          (strcmp(next_file->d_name,"." ) == 0) ) {
        continue;
      }
      
      // IsDir will check if the "filepath" is a directory
      if (IsDir(filepath)) {
        if (!recursive) {
          //if we aren't recursively deleting in subfolders, skip this dir
          continue;
        }
        
        ret_val = RemoveAllFiles(filepath, recursive);
        
        if (ret_val != 0) {
          // Don't stop if we can't delete a folder.  Just keep deleting as
          //  much as we can
          //closedir(theFolder);
          //return ret_val;
        }
      }

      // remove the file
      // TODO: Double check this w.r.t return value and errno - it doesn't look right
      ret_val = remove(filepath);

      // If we can't remove a file, just continue and hope it won't mess anything else up
      //  try to remove all we can
      if (ret_val != 0 && ret_val != ENOENT) {
        debug << Debug::Mode::Err << "Could not remove " << filepath << ".  Continuing" << std::endl;
      }      
    }
    
    closedir(theFolder);
    
    return 0;
  }
  
  //TODO: Consider replacing these with returns of unique_ptr if copying becomes too much
  std::vector<std::string> Split(std::string str, std::string delims) {
    std::vector<std::string> ret;
    unsigned long start = 0, end = 0;
    unsigned long len = str.length();
        
    while(start < len) {
      start = str.find_first_not_of(delims, start);
      if(start == std::string::npos) break;
          
      end = str.find_first_of(delims, start + 1);
      if(end == std::string::npos) end = str.length();
          
      // Everything between the two is a token
      ret.push_back(str.substr(start, end-start));
          
      // Then start looking for the next token
      start = end + 1;
    }
        
    return ret;
  }

  std::map<std::string,std::string> ToDictionary(std::string param) {
    std::map<std::string, std::string> ret;

    // First, split the string by whitespace and iterate over each token
    std::vector<std::string> toks = Split(param, " \t\r\n");
    for(const auto& tok : toks) {
      // Try to extract a key=value from each token
      std::vector<std::string> elems = Split(tok, "=");

      // Store off the key/value pairs int a dictionary
      if(elems.size() == 0) {/* error */}
      else if(elems.size() == 1) ret[elems[0]] = "";
      else if(elems.size() == 2) ret[elems[0]] = elems[1];
      else if(elems.size() > 2) {
        // This may be an error?
        ret[elems[0]] = elems[1];
      }
    }

    return ret;
  }

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

};
