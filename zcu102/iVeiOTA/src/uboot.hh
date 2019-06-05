#ifndef __IVEIOTA_UBOOT_HH
#define __IVEIOTA_UBOOT_HH

#include <vector>
#include <memory>

#include "support.hh"
#include "iveiota.hh"
#include "message.hh"


namespace iVeiOTA {
  class UBootManager {
  public:
    
    struct ContainerInfo {
      int tries, rev;
      bool valid, updated;
    };
    
    UBootManager();    
    std::vector<std::unique_ptr<Message>> ProcessCommand(const Message &message);

    void SetValidity(Container container, bool valid);
    void SetUpdated(Container container, bool updated);
    void SetTries(Container container, int tries);
    void SetRev(Container container, int rev);

    int GetRev(Container container);
    
  protected:
    std::map<Container, ContainerInfo> containerInfo;
    
    bool readContainerInfo(Container container);
    bool writeContainerInfo(Container container);
  };
};

#endif
