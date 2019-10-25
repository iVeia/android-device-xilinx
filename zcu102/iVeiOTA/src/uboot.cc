#include <vector>
#include <memory>
#include <fstream>

#include "uboot.hh"
#include "message.hh"
#include "support.hh"
#include "debug.hh"
#include "config.hh"

namespace iVeiOTA {
  UBootManager::UBootManager() {
    // Read the information from both the containers and print it out
    readContainerInfo(Container::Active);
    readContainerInfo(Container::Alternate);

    debug << Debug::Mode::Info <<
      "Active container info -- " << std::endl << Debug::Mode::Info << 
      " \tTries: "   << containerInfo[Container::Active].tries <<
      " \tRev: "     << containerInfo[Container::Active].rev <<
      " \tValid: "   << containerInfo[Container::Active].valid <<
      " \tUpdated: " << containerInfo[Container::Active].updated << std::endl;
    debug << Debug::Mode::Info <<
      "Alternate container info -- " << std::endl << Debug::Mode::Info <<
      " \tTries: "   << containerInfo[Container::Alternate].tries <<
      " \tRev: "     << containerInfo[Container::Alternate].rev <<
      " \tValid: "   << containerInfo[Container::Alternate].valid <<
      " \tUpdated: " << containerInfo[Container::Alternate].updated << std::endl;
  }

  // Process a command from the network server
  std::vector<std::unique_ptr<Message>> UBootManager::ProcessCommand(const Message &message) {
    std::vector<std::unique_ptr<Message>> ret;    
    if(message.header.type != Message::BootManagement) return ret;

    // The boot management only operates on containers, as we are essentially
    //  setting the information 
    Container container = 
      (message.header.imm[0] == 1) ? (Container::Active) : 
      (message.header.imm[0] == 2) ? (Container::Alternate) :
      (message.header.imm[0] == 3) ? (Container::Recovery) : Container::Unknown;
    switch(message.header.subType) {
      //TODO: I think this should be handled internally
    case Message::BootManagement.WriteContainerInfo:
      if(writeContainerInfo(container)) {
        ret.push_back(Message::MakeACK(message));
      } else {
        ret.push_back(Message::MakeNACK(message));
      }
      break;
      
    case Message::BootManagement.SwitchContainer:
    {
      auto curr = containerInfo.find(Container::Active);
      auto alt  = containerInfo.find(Container::Alternate);
      if(curr == containerInfo.end() || alt == containerInfo.end()) {
        ret.push_back(Message::MakeNACK(message, 0, "Cannot find active and alternate containers"));
      } else {
        alt->second.rev = curr->second.rev + 1;
        if(writeContainerInfo(Container::Alternate)) {
          ret.push_back(Message::MakeACK(message));
        } else {
          ret.push_back(Message::MakeNACK(message, 0, "Unable to write alternate container meta-data"));
        }
      }
    }
    break;
    
    case Message::BootManagement.SetValidity:
      // Sets or resets the valid flag
      if(containerInfo.find(container) != containerInfo.end()) {
        containerInfo[container].valid = message.header.imm[1] == 0 ? false : true;
        if(writeContainerInfo(container)) {
          ret.push_back(Message::MakeACK(message));
        } else {
          ret.push_back(Message::MakeNACK(message, 0, "Could not write container info"));
        }
      } else {
        ret.push_back(Message::MakeNACK(message));
      }
      break;
      
    case Message::BootManagement.MarkUpdateSuccess:
      // This resets the update flag
      if(containerInfo.find(container) != containerInfo.end()) {
        containerInfo[container].updated = message.header.imm[1] == 0 ? false : true;
        debug << "Updating container " << ToString(container) <<
          " update success: " << containerInfo[container].updated;
        if(writeContainerInfo(container)) {
          debug << "Succeeded" << std::endl;
          ret.push_back(Message::MakeACK(message));
        } else {
          debug << "Failed" << std::endl;
          ret.push_back(Message::MakeNACK(message, 0, "Could not write container info"));
        }
      } else {
        debug << "Could not find container" << std::endl;
        ret.push_back(Message::MakeNACK(message, 0, "Invalid message"));
      }
      break;
      
    case Message::BootManagement.ResetBootCount:
      // Sets the boot count back to zero
      if(containerInfo.find(container) != containerInfo.end()) {
        containerInfo[container].tries = 0;
        debug << "Updating container " << ToString(container) <<
          " tries: " << containerInfo[container].tries;
        if(writeContainerInfo(container)) {
          debug << "Succeeded" << std::endl;
          ret.push_back(Message::MakeACK(message));          
        } else {
          debug << "Failed" << std::endl;
          ret.push_back(Message::MakeNACK(message, 0, "Could not write container info"));
        }
      } else {
        debug << "Failed to find container" << std::endl;
        ret.push_back(Message::MakeNACK(message, 0, "Invalid message"));
      }
      break;
      
    default: ret.push_back(Message::MakeNACK(message, 0, "Invalid Command"));
    }
    
    return ret;
  }
  
  void UBootManager::SetValidity(Container container, bool valid) {
    if(containerInfo.find(container) != containerInfo.end()) {
      containerInfo[container].valid = valid;
      writeContainerInfo(container);
    }
  }
  
  void UBootManager::SetUpdated(Container container, bool updated) {
    if(containerInfo.find(container) != containerInfo.end()) {
      containerInfo[container].updated = updated;
      writeContainerInfo(container);
    }
  }
  
  void UBootManager::SetTries(Container container, int tries) {
    if(containerInfo.find(container) != containerInfo.end()) {
      containerInfo[container].tries = tries;
      writeContainerInfo(container);
    }
  }
  
  void UBootManager::SetRev(Container container, int rev) {
    if(containerInfo.find(container) != containerInfo.end()) {
      containerInfo[container].rev = rev;
      writeContainerInfo(container);
    }
  }

  void UBootManager::SetAll(Container container, bool valid, bool updated, int tries, int rev) {
    if(containerInfo.find(container) != containerInfo.end()) {
      containerInfo[container].valid   = valid;
      containerInfo[container].updated = updated;
      containerInfo[container].tries   = tries;
      containerInfo[container].rev     = rev;
      writeContainerInfo(container);
    }
  }

  int UBootManager::GetRev(Container container) {
    if(containerInfo.find(container) != containerInfo.end()) {
      return containerInfo[container].rev;
    }
    return -1;
  }

  bool UBootManager::GetUpdated(Container container) {
    if(containerInfo.find(container) != containerInfo.end()) {
      return containerInfo[container].updated;
    }
    return false;
  }

  bool UBootManager::readContainerInfo(Container container) {
    std::string dev = config.GetDevice(container, Partition::BootInfo);
    
    ContainerInfo bi;
    Mount mount(dev, IVEIOTA_MNT_POINT);
    std::string fName = mount.Path() + "/" + IVEIOTA_UBOOT_CONF_NAME;

    if(!mount.IsMounted()) {
      debug << Debug::Mode::Err << "Failed to mount: " << dev << " on " << IVEIOTA_MNT_POINT << std::endl << Debug::Mode::Info;
      return false;
    } else {
      try {
        // TODO: Need proper path handling here
        // TODO! : opening an non-existent file doesn't cause a problem...
        debug << "Trying to read " << fName << std::endl;
        std::ifstream input(fName);
        std::string line;
        while(std::getline(input, line)) {
          debug << "   line: " << line << std::endl;
          std::vector<std::string> toks = Split(line, "=");
          if(toks.size() > 1) {
            debug << " > " << toks[0] << ":" << toks[1] << std::endl;
            if(toks[0] == "BOOT_UPDATED")    bi.updated = (toks[1][0] == '1');
            else if(toks[0] == "BOOT_VALID") bi.valid   = (toks[1][0] == '1');
            else if(toks[0] == "BOOT_COUNT") bi.tries   = strtol(toks[1].c_str(), 0, 10);
            else if(toks[0] == "BOOT_REV")   bi.rev     = strtol(toks[1].c_str(), 0, 10);
          }
        }
        
        containerInfo[container] = bi;
        return true;
      } catch(...) {
        debug << Debug::Mode::Err << "Failed to write to info file: " << fName << std::endl << Debug::Mode::Info;
        return false;
      }
    }
  }    

  bool UBootManager::writeContainerInfo(Container container) {
    // First we have to mount the container
    ContainerInfo info = containerInfo[container];
    std::string dev = config.GetDevice(container, Partition::BootInfo);
    
    Mount mount(dev, IVEIOTA_MNT_POINT);
    if(!mount.IsMounted()) {
      debug << Debug::Mode::Err <<  "Failed to mount BootInfo partition: " << dev << " for writing" << std::endl;
      return false;
    } else {
      debug << "Mounted " << dev << " to write boot info" << std::endl;
      // TODO: Need proper path handling here
      std::ofstream output(mount.Path() + "/" + IVEIOTA_UBOOT_CONF_NAME);
      output << "BOOT_UPDATED=" << (info.updated?"1":"0") << std::endl;
      output << "BOOT_VALID=" << (info.valid?"1":"0") << std::endl;
      output << "BOOT_COUNT=" << info.tries << std::endl;
      output << "BOOT_REV=" << info.rev << std::endl;
      return true;
    }
  }
};
