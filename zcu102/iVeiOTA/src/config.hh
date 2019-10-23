#ifndef __IVEIOTA_CONFIG_HH
#define __IVEIOTA_CONFIG_HH

#include <string>
#include <vector>

#include "iveiota.hh"
#include "support.hh"

namespace iVeiOTA {
  class GlobalConfig {
  public:
    explicit GlobalConfig(const std::string &configPath = IVEIOTA_DEFAULT_CONFIG,
                          const std::string &cmdLine    = IVEIOTA_KERNEL_CMDLINE);

    void Init();
    std::string GetDevice(Container container, Partition part);
    std::string GetContainerName(Container container);
    bool        IsSinglePartition(Partition part);
    std::string GetHashAlgorithmProgram(HashAlgorithm algo);

    bool Valid() const;
  protected:
    std::string configPath;  // Path to our configuration file
    std::string cmdLinePath; // Path to the kernel command line

    // Mapping from our containers to our partition device strings
    std::map<Container, std::map<Partition, std::string>> partitions;

    // Mapping from hash algorithm names to program locations
    std::map<HashAlgorithm, std::string> hashAlgorithms;
    
    std::string active;    // Name of the active container
    std::string alternate; // Name of the alternate container

    bool updated;
  };

  // Keep a global configuration so that everyone has access
  //  It should be read-only after it is created so there shouldn't be an issue
  //  with thread safety
  extern GlobalConfig config;
};

#endif
