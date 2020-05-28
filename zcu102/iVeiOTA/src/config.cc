#include <vector>
#include <map>
#include <string>
#include <fstream>

#include "config.hh"
#include "debug.hh"
#include "support.hh"

namespace iVeiOTA {
  GlobalConfig config;

  GlobalConfig::GlobalConfig(const std::string &configPath,
                             const std::string &cmdLine) :
    configPath(configPath), cmdLinePath(cmdLine) {
    active = "";
    alternate = "";
  }

  void GlobalConfig::Init() {
    active = "None";
    alternate = "None";

    updated = false;
    
    // Get the kernel command line args first
    try {
      debug << Debug::Mode::Info << "Trying to parse command line" << std::endl;
      std::ifstream conf(cmdLinePath);
      std::string line;
      while(std::getline(conf, line)) {
        // Keep a mapping of key value pairs so we can extract what we want
        std::map<std::string, std::string> tokens = ToDictionary(line);
        
        // Parse the dictionary for the values we want
        for(const auto& kv : tokens) {
          debug << "     << " << kv.first << "," << kv.second << ">> " << std::endl;
          
          if(kv.first == "iveia.boot.rev") {
            debug << Debug::Mode::Info << "iveia.boot.rev = " << kv.second << std::endl;
            // Don't care
          } else if(kv.first == "iveia.boot.active") {
            debug << Debug::Mode::Info << "iveia.boot.active = " << kv.second << std::endl;
            active = kv.second;
          } else if(kv.first == "iveia.boot.alternate") {
            debug << Debug::Mode::Info << "iveia.boot.alternate = " << kv.second << std::endl;
            alternate = kv.second;
          }else if(kv.first == "iveia.boot.updated") {
            debug << Debug::Mode::Info << "iveia.boot.updated = " << kv.second << std::endl;
            updated = (kv.second[0] == '1') ? true : false;
          }
        }        
      }
    } catch(...) {
      debug << Debug::Mode::Failure << "Failed to parse command line" << std::endl;
    }

    // TODO: We can't do a download with no current - need to have a flag that says that
    //       
    // TODO: If we ever want to support non-OTA downloads (i.e. just overwrite the active system)
    //       we can revisit this
    if(active.length() == 0 || active == "None") {
      active = "";
      debug << Debug::Mode::Failure << "Did not find active identifier! OTA will likely not work" << std::endl;
    }

    // TODO: We can still download with no alternate - need to implement that
    if(alternate.length() == 0 || alternate == "None") {
      alternate = "";
      debug << Debug::Mode::Failure << "Did not find alternate identifier! OTA will likely not work" << std::endl;
    }

    // Get the config file next
    try {
      debug << Debug::Mode::Info << "Reading config file: " << configPath << std::endl;
      std::ifstream conf(configPath);
      std::string line;

      // The config file is a simeple token:value1:value2:.. sequence of lines
      //  Not parameter may cross a line, and each parameter is a full line
      while(std::getline(conf, line)) {
        std::vector<std::string> toks = Split(line, ":");
        if(toks.size() > 0) {

          // partition:type:name:device_file
          //  type is determined by the bootloader (currently a or b), which is obtained from the kernel command line
          //  alternatively, type can be "single" which means there is only one of it
          //    getting the alternate or current will result in the same device
          //
          if(toks[0] == "partition") {
            if(toks.size() < 4) continue; // Invalid
            std::string which = toks[1];
            std::string name  = toks[2];
            std::string dev   = toks[3];


            // Try and get the filesystem type if it is in there
            std::string ftype = "ext4";
            if(toks.size() > 4) {
              // Sanity check first
              for (auto ftype_tmp : std::vector<std::string> {"ext2", "ext3", "ext4", "fat32", "vfat"} ) {
                if(toks[4] == ftype_tmp) ftype = ftype_tmp;
              }
            }
            
            // Then get the type of the container
            Container container;
            if(which == "single")                                 container = Container::Single;
            else if(active.length() > 0 && which == active)       container = Container::Active;
            else if(alternate.length() > 0 && which == alternate) container = Container::Alternate;
            else                                                  container = Container::Unknown;
            debug << Debug::Mode::Info << "    Partition: " << which << ":" << name << ":" << dev << std::endl;

            // Which type of partition this is
            Partition part = GetPartition(name);

            // TODO: Need to check the device file existence too?
            if(container == Container::Single) {
              // A single container (no alternate) will get inserted as active, alternate, and single
              //  so that every GetContainer call will find it
              // TODO: Maybe put that logic into GetContainer to make it more obvious what is going on
              debug << Debug::Mode::Info << "Inserting single container " << ToString(part) << " : " << dev << " for both active and alternate" << std::endl;
              partitions[Container::Active].insert(std::make_pair(part, dev));
              partitions[Container::Alternate].insert(std::make_pair(part, dev));
              partitions[Container::Single].insert(std::make_pair(part, dev));

              debug << Debug::Mode::Info << "Setting device " << dev << " filesystem type to " << ftype << std::endl;
              deviceTypes[dev] = ftype;
            } else if(container != Container::Unknown && part != Partition::Unknown) {
              debug << Debug::Mode::Info << "Inserting: " << ToString(container) << ":" << ToString(part) << ":" << dev << std::endl;
              partitions[container].insert(std::make_pair(part, dev));

              debug << Debug::Mode::Info << "Setting device " << dev << " filesystem type to " << ftype << std::endl;
              deviceTypes[dev] = ftype;
            } else {
              debug <<
                Debug::Mode::Warn <<
                "Unknown partition in config file: " << which << ":" << name << ":" << dev << std::endl;
            }
          } // end if(toks[0] == "partition"

          // hash_prog:type:path_to_executable
          else if(toks[0] == "hash_prog") {
            if(toks.size() < 3) continue; // Invalid
            std::string name = toks[1];
            std::string path = toks[2];

            debug << Debug::Mode::Info << "Hash Algorithm: " << name << ":" << path << std::endl;
            
            HashAlgorithm algo = GetHashAlgorithm(name);
            if(algo != HashAlgorithm::Unknown) {
              // TODO: CHeck if this path exists
              hashAlgorithms[algo] = path;
            }
          } // end if(toks[0] == "hash_prog")
          
        }
      }      
    } catch(...) {
      debug << Debug::Mode::Failure << "Failed to read config file" << std::endl;
    }
  }

  bool GlobalConfig::Valid() const {
    if(
       // We will consider a system valid if we have any targets to download to
       (partitions.at(Container::Single).size() > 0) ||
       // We consider an OTA system valid if it has an active and an alternate container
       (active.length() > 0 && active != "None" &&
        alternate.length() > 0 && alternate != "None")
       )
      {
        return true;
      }
    else
      {
        return false;
      }
  }
  
  std::string GlobalConfig::GetHashAlgorithmProgram(HashAlgorithm algo) {
    if(hashAlgorithms.find(algo) != hashAlgorithms.end()) {
      return hashAlgorithms[algo];
    } else {
      debug << Debug::Mode::Err << "Did not find hash algorithm : " << ToString(algo) << std::endl;
      return "";
    }
  }

  // Get the string name of a container, passed to us by the kernel
  std::string GlobalConfig::GetContainerName(Container container) {
    if(container == Container::Active) return active;
    else if(container == Container::Alternate) return alternate;
    else {
      debug << Debug::Mode::Err << "Did not find container : " << ToString(container) << std::endl;
      return "";
    }
  }

  bool GlobalConfig::IsSinglePartition(Partition part) {
    if(partitions.find(Container::Single) != partitions.end() &&
       partitions[Container::Single].find(part) != partitions[Container::Single].end()) {
      debug << Debug::Mode::Debug << "Partition is single: " << ToString(part) << std::endl;
      return true;
    } else {
      debug << Debug::Mode::Debug << "Partition is not single: " << ToString(part) << std::endl;
      return false;
    }
  }

  // Get the device name of a container/partition combo
  // This comes from the command line and the config file together
  std::string GlobalConfig::GetDevice(Container container, Partition part) {
    if(partitions.find(container) != partitions.end() &&
       partitions[container].find(part) != partitions[container].end()) {
      return partitions[container][part];
    } else {
      debug << Debug::Mode::Err << "Did not find partition: " << ToString(container) << ":" << ToString(part) << std::endl;
      return "";
    }
  }

  std::string GlobalConfig::GetFilesystemType(std::string dev) {
    if(deviceTypes.find(dev) != deviceTypes.end()) {      
      return deviceTypes[dev];
    } else {
      debug << Debug::Mode::Err << "Did not find filesystem " << dev << " so cannot return type" << std::endl;
      return "";
    }
  }
  
};
