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
    
    // Get the command line args first
    try {
      debug << "Trying to parse command line" << std::endl;
      std::ifstream conf(cmdLinePath);
      std::string line;
      while(std::getline(conf, line)) {
        // Keep a mapping of key value pairs so we can extract what we want
        std::map<std::string, std::string> tokens = ToDictionary(line);
        
        // Parse the dictionary for the values we want
        for(const auto& kv : tokens) {
          debug << " << " << kv.first << "," << kv.second << ">> " << std::endl;
          
          if(kv.first == "iveia.boot.rev") {
            // Don't care
          } else if(kv.first == "iveia.boot.active") {
            active = kv.second;
          } else if(kv.first == "iveia.boot.alternate") {
            alternate = kv.second;
          }else if(kv.first == "iveia.boot.updated") {
            updated = (kv.second[0] == '1') ? true : false;
          }
        }        
      }
    } catch(...) {
      debug << Debug::Mode::Failure << "Failed to parse command line" << std::endl << Debug::Mode::Info;
    }

    // TODO: We can't do a download with no current - need to have a flag that says that
    if(active.length() == 0 || active == "None") {
      active = "";
      debug << Debug::Mode::Failure << "Did not find active identifier! OTA will likely not work" << std::endl << Debug::Mode::Info;
    }

    // TODO: We can still download with no alternate - need to implement that
    if(alternate.length() == 0 || alternate == "None") {
      alternate = "";
      debug << Debug::Mode::Failure << "Did not find alternate identifier! OTA will likely not work" << std::endl << Debug::Mode::Info;
    }

    // Get the config file next
    try {
      debug << "Reading config file: " << configPath << std::endl;
      std::ifstream conf(configPath);
      std::string line;

      // The config file is a simeple token:value1:value2:.. sequence of lines
      //  Not parameter may cross a line, and each parameter is a full line
      while(std::getline(conf, line)) {
        std::vector<std::string> toks = Split(line, ":");
        if(toks.size() > 0) {

          // partition:type:name:device_file
          //  type is determined by the bootload (currently a or b), which is obtained from the kernel command line
          //
          if(toks[0] == "partition") {
            if(toks.size() < 4) continue; // Invalid
            std::string which = toks[1];
            std::string name  = toks[2];
            std::string dev   = toks[3];

            Container container;
            if(active.length() > 0 && which == active)            container = Container::Active;
            else if(alternate.length() > 0 && which == alternate) container = Container::Alternate;
            else                                                  container = Container::Unknown;

            debug << "Partition: " << which << ":" << name << ":" << dev << std::endl;
            Partition part = GetPartition(name);

            // TODO: Need to check the device file existence too?
            if(container != Container::Unknown && part != Partition::Unknown) {
              debug << Debug::Mode::Info <<
                "Inserting: " << ToString(container) << ":" << ToString(part) << ":" << dev << std::endl;
              partitions[container].insert(std::make_pair(part, dev));
              
            } else {
              debug <<
                Debug::Mode::Err <<
                "Unknown partition in config file: " << which << ":" << name << ":" << dev << std::endl <<
                Debug::Mode::Info;
            }
          } // end if(toks[0] == "partition"

          // hash_prog:type:path_to_executable
          else if(toks[0] == "hash_prog") {
            if(toks.size() < 3) continue; // Invalid
            std::string name = toks[1];
            std::string path = toks[2];

            debug << "Hash Algorithm: " << name << ":" << path << std::endl;
            
            HashAlgorithm algo = GetHashAlgorithm(name);
            if(algo != HashAlgorithm::Unknown) {
              // TODO: CHeck if this path exists
              hashAlgorithms[algo] = path;
            }
          } // end if(toks[0] == "hash_prog")
          
        }
      }
      
    } catch(...) {
      debug << Debug::Mode::Failure << "Failed to read config file" << std::endl << Debug::Mode::Info;
    }
  }

  bool GlobalConfig::Valid() const {
    if(active.length() > 0 && active != "None" &&
       alternate.length() > 0 && alternate != "None") {
      return true;
    } else {
      return false;
    }
  }

  std::string GlobalConfig::GetHashAlgorithmProgram(HashAlgorithm algo) {
    if(hashAlgorithms.find(algo) != hashAlgorithms.end()) {
      return hashAlgorithms[algo];
    } else {
      return "";
    }
  }

  std::string GlobalConfig::GetContainerName(Container container) {
    if(container == Container::Active) return active;
    else if(container == Container::Alternate) return alternate;
    else return "";
  }

  std::string GlobalConfig::GetDevice(Container container, Partition part) {
    if(partitions.find(container) != partitions.end() &&
       partitions[container].find(part) != partitions[container].end()) {
      return partitions[container][part];
    } else {
      debug << Debug::Mode::Debug << "Did not find partition: " << ToString(container) << ":" << ToString(part) << std::endl << Debug::Mode::Info;
      return "";
    }
  }
};
