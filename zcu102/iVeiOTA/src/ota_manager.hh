#ifndef __OTA_MANAGER_HH
#define __OTA_MANAGER_HH

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <thread>

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
        File,
        Script,
        Dummy,

        Unknown,
    };    
  static ChunkType GetChunkType(const std::string &name);

    struct ChunkInfo {
        std::string ident;    // Identifier for the chunk
        std::string hash;     // MD5 hash for the chunk

        ChunkType type;       // Type of this chunk for processing
        Partition dest;       // Which partition this goes chunk goes into

        bool orderMatters;    //

        // TODO: maybe make this a union?
        uint64_t pOffset;     // Physical offset (on the device) for Image chunks
        uint64_t fOffset;     // File offset for Image chunks
        uint64_t size;        // How many bytes in the image to write

        std::string filePath; // Destination path for file chunks

        bool processed;
        bool succeeded;
    };
    
    int         initStatus;
    bool        initCancel;

    bool processingChunk;
    std::string whichChunk;

    bool updateActive;
    bool updateAvailable;
    std::vector<ChunkInfo> chunks;
    unsigned int maxIdentLength;


    public:

  explicit OTAManager(UBootManager &bootMgr, std::string configFile = IVEIOTA_DEFAULT_CONFIG);
  std::vector<std::unique_ptr<Message>> ProcessCommand(const Message &message);
  
    protected:
  UBootManager &bootMgr;
  std::vector<std::unique_ptr<Message>> processActionMessage(const Message &message);
  std::vector<std::unique_ptr<Message>> processStatusMessage(const Message &message);
  void prepareForUpdate(bool noCopy = false);
  bool processChunk(const Message &message, std::vector<std::unique_ptr<Message>> &ret);
  bool processChunkFile(const ChunkInfo &chunk, const std::string &path);
  bool processManifest(const std::string &manifest, std::vector<std::unique_ptr<Message>> &ret);
};
};

#endif

