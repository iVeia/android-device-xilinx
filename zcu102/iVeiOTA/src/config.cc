#include <vector>
#include <map>
#include <string>
#include <fstream>

#include "config.hh"
#include "debug.hh"

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

    if(active.length() == 0 || active == "None") {
      debug << Debug::Mode::Failure << "Did not find active identifier! OTA will likely not work" << std::endl << Debug::Mode::Info;
    }

    if(alternate.length() == 0 || alternate == "None") {
      debug << Debug::Mode::Failure << "Did not find alternate identifier! OTA will likely not work" << std::endl << Debug::Mode::Info;
    }

    // Get the config file next
    try {
      debug << "Reading config file" << std::endl;
      std::ifstream conf(configPath);
      std::string line;
      while(std::getline(conf, line)) {
        std::vector<std::string> toks = Split(line, ":");
        if(toks.size() > 0) {
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
              debug << Debug::Mode::Debug <<
                "Inserting: " << ToString(container) << ":" << ToString(part) << ":" << dev << std::endl;

              //partition_info _info;
              //_info.container = container;
              //_info.partition = part;
              //_info.device = dev;
              //partitions.push_back(_info);
              
              partitions[container].insert(std::make_pair(part, dev));
              
            } else {
              debug <<
                Debug::Mode::Err <<
                "Unknown partition in config file: " << which << ":" << name << ":" << dev << std::endl <<
                Debug::Mode::Info;
            }
          }
        }
      }
      
    } catch(...) {
      debug << Debug::Mode::Failure << "Failed to read command line" << std::endl << Debug::Mode::Info;
    }
  }

  std::string GlobalConfig::GetContainerName(Container container) {
    if(container == Container::Active) return active;
    else if(container == Container::Alternate) return alternate;
    else return "";
  }

  std::string GlobalConfig::GetDevice(Container container, Partition part) {
    //for(auto _info : partitions) {
    //  debug << Debug::Mode::Debug << "   *** " << ToString(_info.container) << ":" << ToString(_info.partition) << std::endl;
    //  if(_info.container == container && _info.partition == part) return _info.device;
    //}
    if(partitions.find(container) != partitions.end() &&
       partitions[container].find(part) != partitions[container].end()) {
      return partitions[container][part];
    } else {
      debug << Debug::Mode::Debug << "Did not find partition: " << ToString(container) << ":" << ToString(part) << std::endl << Debug::Mode::Info;
      return "";
    }
  }
};
