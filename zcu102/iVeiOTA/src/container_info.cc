#include "container_info.hh"

#include "debug.hh"

namespace iVeiOTA {
  ContainerInfo::ContainerInfo() : revision(0), bootCount(100), updated(0), altDev("invalid"), valid(false) {}
  ContainerInfo::ContainerInfo(int rev, int count, bool updated, std::string alt) :
    revision(rev), bootCount(count), updated(updated), altDev(alt), valid(true) {}
  
  // Read the kernel command line and parse out container information
  ContainerInfo ContainerInfo::FromCmdLine(const std::string &cmdLine) {
    std::stringstream ss;
    
    // Open up the command line file and read it all into the string stream buffer
    debug << Debug::Mode::Info << "Opening cmdLine" << std::endl;
    try {
      std::ifstream input(cmdLine);
      ss << input.rdbuf();
    } catch(...) {
      return ContainerInfo();
    }

    debug << "Kernel command line: " << ss.str() << std::endl;
    
    // Keep a mapping of key value pairs so we can extract what we want
    std::map<std::string, std::string> tokens = ToDictionary(ss.str());
    
    int rev = -1;
    int count = 99; // Not currently in the boot information
    bool upd = false;
    std::string alt = "";
    
    // Parse the dictionary for the values we want
    for(const auto& kv : tokens) {
      debug << " << " << kv.first << "," << kv.second << ">> " << std::endl;
        
      if(kv.first == "iveia.boot.rev") {
        rev = atoi(kv.second.c_str());
      } else if(kv.first == "iveia.boot.other") {
        alt = kv.second;
      }else if(kv.first == "iveia.boot.updated") {
        upd = (kv.second[0] == '1') ? true : false;
      }
    }
    
    if(rev != -1) {
      // Assume we found it all
      debug << "ContainerInfo: " << rev << ":" << count << ":" << upd << ":" << alt << std::endl;
      return ContainerInfo(rev, count, upd, alt);
    } else {
      debug << Debug::Mode::Err << "Did not find ContainerInfo on the command line" << std::endl;
      // Didn't find it, so return an invalid default
      return ContainerInfo();
    }
  }
  
  int ContainerInfo::Revision() const {return revision;}
  int ContainerInfo::BootCount() const {return bootCount;}
  bool ContainerInfo::Update() const {return updated;}
  std::string ContainerInfo::AlternateContainerPath() const {return altDev;}
  bool ContainerInfo::IsValid() const {return valid;}
  
  void ContainerInfo::FakeIt() {
    revision = 17;
    bootCount = 3;
    updated = 0;
    altDev = "/dev/mmcblk92p78";  // Should be fake
    valid = true;
  }
};
