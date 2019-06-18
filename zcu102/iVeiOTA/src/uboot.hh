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

    // Information we need to keep about a container
    struct ContainerInfo {
      int tries, rev;
      bool valid, updated;
    };
    
    UBootManager();    
    std::vector<std::unique_ptr<Message>> ProcessCommand(const Message &message);

    // Used to set the fields of a container.  These also write to backing store
    void SetValidity(Container container, bool valid);
    void SetUpdated(Container container, bool updated);
    void SetTries(Container container, int tries);
    void SetRev(Container container, int rev);

    // Get the current settings for the container.  These read cached values
    int GetRev(Container container);
    bool GetUpdated(Container container);
    
  protected:
    std::map<Container, ContainerInfo> containerInfo;
    
    bool readContainerInfo(Container container);
    bool writeContainerInfo(Container container);
  };
};

#endif
