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
  protected:
    //struct partition_info {
    //  Container container;
    //  Partition partition;
    //  std::string device;
    //};
    std::string configPath;
    std::string cmdLinePath;
    
    std::map<Container, std::map<Partition, std::string>> partitions;
    //std::vector<partition_info> partitions;
    
    std::string active;
    std::string alternate;

    bool updated;
  };

  extern GlobalConfig config;
};

#endif
