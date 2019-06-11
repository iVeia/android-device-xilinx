#ifndef __IVEIOTA_UTIL_HH
#define __IVEIOTA_UTIL_HH

#include <sys/mount.h>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>

namespace iVeiOTA {
  enum class Partition {
    Root,
    System,
    BootInfo,
    Boot,
    QSPI,
    Data,
    
    None,
    
    Unknown,
  };
  Partition GetPartition(const std::string &name);
  inline std::string ToString(Partition part) {
    switch(part) {
    case Partition::Root     : return "Root";
    case Partition::System   : return "System";
    case Partition::BootInfo : return "BootInfo";
    case Partition::Boot     : return "Boot";
    case Partition::QSPI     : return "QSPI";
    case Partition::Data     : return "Data";
    case Partition::None     : return "None"; 
    case Partition::Unknown  : return "Unknown";
    default: return "<<Error>>";
    }
  }

  enum class Container {
    Active,
    Alternate,
    Recovery,

    Unknown,
  };
  inline std::string ToString(Container container) {
    switch(container) {
    case Container::Active    : return "Active";
    case Container::Alternate : return "Alternate";
    case Container::Recovery  : return "Recovery";
    case Container::Unknown   : return "Unknown";
    default: return "<<Error>>";
    }
  }
  
  uint64_t CopyFileData(const std::string &dest, const std::string &src, 
                        uint64_t offset, uint64_t len);
  
  //TODO: Consider replacing these with returns of unique_ptr if copying becomes too much
  std::vector<std::string> Split(std::string str, std::string delims);
  
  std::map<std::string,std::string> ToDictionary(std::string param);
  
  class Mount {
  protected:
    bool isMounted;
    bool wasMounted;
    std::string dev, path;
    
    static std::string Mounted(const std::string &name, bool dev);
    
  public:
    static std::string DeviceMounted(const std::string &name);
    static std::string PathMountedOn(const std::string &path);
    
    bool IsMounted() const {return isMounted;}
    const std::string& Path() const {return path;}
    
    explicit Mount(const std::string &dev, const std::string &path);
    
    explicit Mount(const std::string &dev, const std::string &path, bool allowOtherPath, const std::string &type);
    
    ~Mount();
  };
};

#endif