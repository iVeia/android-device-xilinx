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

  // TODO: It is a known issue that this parser will fail if there are differences
  //        in trailing / characters.  i.e. It thinks /mnt/data and /mnt/data/ are different
  //       They are actually different though, so what should be done about it is questionable
  std::string Mount::Mounted(const std::string &name, bool dev) {
    // Get the list of mounted filesystems
    std::stringstream ss;
    {
      std::ifstream input("/proc/mounts");
      if(!input.good()) {
        debug << Debug::Mode::Failure << "Failed to read mount information" << std::endl;
        return "";
      }
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
            debug << "Found " << name << ":" << line << std::endl;
            return toks[0];
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
  Mount::Mount(const std::string &dev, const std::string &path, const std::string &type) : Mount(dev, path, true, type) {}
  Mount::Mount(const std::string &dev, const std::string &path, bool allowOtherPath, const std::string &type) :
    dev(dev), path(path) {
    std::string mpath = DeviceMounted(dev);
    
    // device already mounted
    // TODO: We can probably continue from this state, but what to do about it?
    if(mpath.length() > 0) {
      debug << "Already mounted: " << dev << ":" << mpath << std::endl;
      isMounted = false;
      wasMounted = true;
      return;
    }
    wasMounted = false;
    
    // Check to make sure we can mount on the supplied path
    mpath = PathMountedOn(path);
    if(mpath.length() > 0) {
      debug << "Path " << path << " already mounted on by " << mpath << std::endl;
      if(allowOtherPath) {
        //TODO: implement this
        // Generate a new path here to mount on
      }
      isMounted = false;
      return;
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
