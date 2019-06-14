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
#include "config.hh"

namespace iVeiOTA {
  Partition GetPartition(const std::string &name) {
    if(name == "root")           return Partition::Root;
    else if(name == "system")    return Partition::System;
    else if(name == "boot_info") return Partition::BootInfo;
    else if(name == "bi")        return Partition::BootInfo;
    else if(name == "boot")      return Partition::Boot;
    else if(name == "qspi")      return Partition::QSPI;
    else if(name == "data")      return Partition::Data;
    else if(name == "cache")     return Partition::Cache;
    
    // Dont have an entry here for Partition::None.  It shouldn't be able to be
    //  created from a config file.  Just internally
    
    else                         return Partition::Unknown;
  }

  HashAlgorithm GetHashAlgorithm(const std::string &name) {
    if(name == "md5")    return HashAlgorithm::MD5;
    if(name == "sha1")   return HashAlgorithm::SHA1;
    if(name == "sha256") return HashAlgorithm::SHA256;
    if(name == "sha512") return HashAlgorithm::SHA512;
    if(name == "none")   return HashAlgorithm::None;

    else                 return HashAlgorithm::Unknown;
  }
  
  std::string GetHashValue(HashAlgorithm hashType, const std::string &filePath) {
    if(hashType == HashAlgorithm::None || hashType == HashAlgorithm::Unknown) return "";
    
    // First we have to get the command to run
    std::string prog = config.GetHashAlgorithmProgram(hashType);
    std::string ret  = RunCommand(prog + " " + filePath);
    if(ret.find("No such file") != std::string::npos) {
      debug << "File did not exist to run hash program on" << std::endl;
      // Command returned an error
      return "";
    }

    // TODO: Figure out how many tokens the return splits into
    //  double spaces should be collapsed, but need to make sure
    std::vector<std::string> toks = Split(ret, " \t");
    if(toks.size() >= 2 && toks.size() < 4) return toks[0];
    else return "";    
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
  
  bool dirExists(std::string dir_path) {
    struct stat ss;
    
    if (stat(dir_path.c_str(), &ss) == 0 && S_ISDIR(ss.st_mode)) {
      return true;
    } else {
      return false;
    }
  }

  int RemoveAllFiles(const std::string &path, bool recursive) {
    if (path.empty()) return 0;

    DIR *theFolder = opendir(path.c_str());
    struct dirent *next_file;
    char filepath[1024];
    int ret_val;

    if (theFolder == NULL) return errno;

    while ( (next_file = readdir(theFolder)) != NULL ) {
      // build the path for each file in the folder
      sprintf(filepath, "%s/%s", path.c_str(), next_file->d_name);
      
      //we don't want to process the pointer to "this" or "parent" directory
      if ((strcmp(next_file->d_name,"..") == 0) || (strcmp(next_file->d_name,"." ) == 0) ) {
        continue;
      }
      
      //dirExists will check if the "filepath" is a directory
      if (dirExists(filepath)) {
        if (!recursive) {
          //if we aren't recursively deleting in subfolders, skip this dir
          continue;
        }
        
        ret_val = RemoveAllFiles(filepath, recursive);
        
        if (ret_val != 0) {
          closedir(theFolder);
          return ret_val;
        }
      }
      
      ret_val = remove(filepath);
      //ENOENT occurs when i folder is empty, or is a dangling link, in
      //which case we will say it was a success because the file is gone
      if (ret_val != 0 && ret_val != ENOENT) {
        closedir(theFolder);
        return ret_val;
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

  std::string Mount::Mounted(const std::string &name, bool dev) {
    // Get the list of mounted filesystems
    std::stringstream ss;
    {
      std::ifstream input("/proc/mounts");
      ss << input.rdbuf();
    }
    
    // Each system is its own line, so split on new lines
    auto lines = Split(ss.str(), "\r\n");
    for(std::string line : lines) {
      if(line.length() > 0) {
        // Individual tokens are white space seperated
        auto toks = Split(line, " \t");
        if(toks.size() >= 2) {          
          if(dev && toks[0] == name) {
            debug << "Found device " << name << ":" << line << std::endl;
            return toks[1];
          } else if(toks[1] == name) {
            return toks[0];
            debug << "Found " << name << ":" << line << std::endl;
          }
        }
      }
    }
    
    return "";
  }
  
  std::string Mount::DeviceMounted(const std::string &name) {
    return Mounted(name, true);
  }
  std::string Mount::PathMountedOn(const std::string &path) {
    return Mounted(path, false);
  }


  // TODO: Need to implement directory checking / creation
  
  Mount::Mount(const std::string &dev, const std::string &path) : Mount(dev, path, true, "ext4") {}  
  Mount::Mount(const std::string &dev, const std::string &path, bool allowOtherPath, const std::string &type) :
    dev(dev), path(path) {
    std::string mpath = DeviceMounted(dev);
    
    // device already mounted
    if(mpath.length() > 0) {
      debug << "Already mounted: " << dev << ":" << mpath << std::endl;
      isMounted = false;
      wasMounted = true;
      return;
    }
    wasMounted = false;
    
    // Check to make sure we can mount on the supplied path
    mpath = PathMountedOn(path);
    if(mpath.length() <= 0) {
      if(allowOtherPath) {
        //TODO: implement this
        // Generate a new path here to mount on
      }
      isMounted = false;
    }
    
    // Mount the device onto path
    debug << Debug::Mode::Info << "Trying to mount " << dev.c_str() << " onto " << path.c_str() << " with type " << type.c_str() << std::endl;
    int res = mount(dev.c_str(), path.c_str(), type.c_str(), 0, 0);
    if(res != 0) {
      // We failed to mount the filesystem
      debug << Debug::Mode::Err << "Failed to mount: " << res << ":" << strerror(errno) << std::endl << Debug::Mode::Info;
      isMounted = false;
    } else {
      isMounted = true;
    }
    
  }
  
  Mount::~Mount() {            
    std::string mpath = PathMountedOn(path);
    if(mpath.length() <= 0) {
      // No longer mounted...
      return;
    }

    debug << Debug::Mode::Info << "Trying to unmount " << path.c_str() << std::endl;    
    int res = umount(path.c_str());
    if(res != 0) {
      // Failed to unmount - What to do here
    }
  }
};
