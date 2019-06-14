#ifndef __OTA_MANAGER_HH
#define __OTA_MANAGER_HH

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <pthread.h>

#include "iveiota.hh"
#include "message.hh"
#include "support.hh"
#include "uboot.hh"

namespace iVeiOTA {
  struct OTAStatus {
    
  };
  
  class OTAManager {
  protected:
    enum class ChunkType {
      Image,
      Archive,
      File,
      Script,
      Dummy,
      
      Unknown,
    };    
    static ChunkType GetChunkType(const std::string &name);

    enum class OTAState {
      Idle,
      UpdateAvailable,
      Initing,
      Preparing,
      InitDone,
    };
    OTAState state;            // TODO: Should mutex protect this
    
    struct ChunkInfo {
      std::string ident;       // Identifier for the chunk
      
      HashAlgorithm hashType;  // How to calculate the hash value
      std::string hashValue;   // MD5 hash for the chunk
      
      ChunkType type;          // Type of this chunk for processing
      Partition dest;          // Which partition this goes chunk goes into
      
      bool orderMatters;       //
      
      // TODO: maybe make this a union?
      uint64_t pOffset;        // Physical offset (on the device) for Image chunks
      uint64_t fOffset;        // File offset for Image chunks
      uint64_t size;           // How many bytes in the image to write

      bool complete;
      
      std::string filePath;    // Destination path for file chunks
      
      bool processed;
      bool succeeded;
    };
    
    bool processingChunk;
    std::string whichChunk;
    std::string intChunkPath;
    pthread_t copyThread;
    
    std::vector<ChunkInfo> chunks;
    unsigned int maxIdentLength;

    pthread_t processThread;
    bool copyBI;
    bool copyRoot;
    bool copySystem;
    
  public:
    
    explicit OTAManager(UBootManager &bootMgr, std::string configFile = IVEIOTA_DEFAULT_CONFIG);
    std::vector<std::unique_ptr<Message>> ProcessCommand(const Message &message);
    
  protected:
    UBootManager &bootMgr;
    std::vector<std::unique_ptr<Message>> processActionMessage(const Message &message);
    std::vector<std::unique_ptr<Message>> processStatusMessage(const Message &message);
    bool prepareForUpdate(bool noCopy = false);
    void processChunk();
    bool processChunkFile(const ChunkInfo &chunk, const std::string &path);
    bool processManifest(const std::string &manifest, std::vector<std::unique_ptr<Message>> &ret);
    void initUpdateFunction();
    uint64_t copyFileData(const std::string &dest, const std::string &src,
                          uint64_t offset, uint64_t len);

    friend void* CopyThreadFunction(void *data);
    friend void* ProcessThreadFunction(void *data);
  };  
};

#endif

