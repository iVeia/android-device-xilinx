#ifndef __IVEIOTA_INFO_HH
#define __IVEIOTA_INFO_HH

#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#include "support.hh"

namespace iVeiOTA {
    struct ContainerInfo {
      public:
      explicit ContainerInfo();
      explicit ContainerInfo(int rev, int count, bool updated, std::string alt);

        // Read the kernel command line and parse out container information
      static ContainerInfo FromCmdLine(const std::string &cmdLine = "/proc/cmdline");
      
      int Revision() const;
      int BootCount() const;
      bool Update() const;
      std::string AlternateContainerPath() const;
      bool IsValid() const;

      void FakeIt();
      protected:
        // Revision of the currently executing container
        int revision;

        // Number of attempts to boot the current container
        int bootCount;

        // Whether this boot is an update or not
        bool updated;

        // The path of the alternate container to update into
        std::string altDev;

        // A flag to indicate if this container information seems to be good
        bool valid;
    };

};

#endif
