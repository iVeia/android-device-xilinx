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

// TODO: This class has gotten too large.  Just for maintence purposes
//       I should look into splitting off some functionality, like chunk
//       processing maybe

namespace iVeiOTA {
  class OTAManager {
  protected:
    // The types of chunks the system supports
    enum class ChunkType {
      Image,    // A full filesystem image
      Archive,  // A zipped archive (only what tar -xf supports)
      File,     // A single file copied to a destination
      Script,   // A script to execute (not implemented yet)
      Dummy,    // A dummy chunk that does nothing
      
      Unknown,  // An error
    };    
    static ChunkType GetChunkType(const std::string &name);

    // The states the OTA system can be in
    enum class OTAState {
      Idle,             // Idle, not doing anything
      Canceling,        // We are trying to cancel an update
      UpdateAvailable,  // A download was attempted and can be continued
      Initing,          // Initialization occurring
      Preparing,        // Preparing an update
      InitDone,         // Initialization and preparation is done
      AllDone,          // All chunks have been processed
      AllDoneFailed     // All chunks have been processed but some failed
    };
    OTAState state; // Should only get written in the main thread
    inline std::string ToString(ChunkType type) {
      switch(type) {
      case ChunkType::Image:   return "Image";   break;
      case ChunkType::Archive: return "Archive"; break;
      case ChunkType::File:    return "File";    break;
      case ChunkType::Script:  return "Script";  break;
      case ChunkType::Dummy:   return "Dummy";   break;
      default:                 return "Unknown"; break;
      }
    }

    // Information stored about each type of chunk
    struct ChunkInfo {
      std::string ident;       // Identifier for the chunk
      
      HashAlgorithm hashType;  // How to calculate the hash value
      std::string hashValue;   // MD5 hash for the chunk
      
      ChunkType type;          // Type of this chunk for processing
      Partition dest;          // Which partition this goes chunk goes into
      
      bool orderMatters;       // If order matters, this chunk MUST be processed
                               // before any chunks that come after it
      
      bool processed;          // If this chunk has been processed
      bool succeeded;          // If this chunks succeeded processing

      // TODO: maybe make this a union?
      // -------------- For image chunk types
      uint64_t pOffset;        // Physical offset (on the device) for Image chunks
      uint64_t fOffset;        // File offset for Image chunks
      uint64_t size;           // How many bytes in the image to write

      // -------------- For archive chunk types
      // TODO: Maybe add a destination for archive chunks so that we can
      //       untar many files to a subdirectory for some reason
      bool complete;           // Is this is a complete filesystem archive
                               // If true, we will delete all files on the destination
                               //  filesystem before unpacking

      // -------------- For File chunk types
      std::string filePath;    // Destination path for file chunks      
    };

    // For handling the processing of chunks
    pthread_t processThread;  // The thread that does the processing
    bool processingChunk;     // True if we are currently processing a chunk
    bool joinProcessThread;   // True if the processing thread has completed and needs to be join()ed
    std::string whichChunk;   // Which chunk we are processing
    std::string intChunkPath; // The path to the chunk file, for internal use
    
    std::vector<ChunkInfo> chunks; // A list of chunks we need for an update
    unsigned int maxIdentLength;   // The max identifier encountered in the manifest

    // For the handling of update initialization
    pthread_t copyThread; // The thread that does the initialization
    bool joinCopyThread;  // True if the initialization thread needs to be join()ed
    bool copyBI;          // True if we need to copy the BootInfo partition during initialization
    bool copyRoot;        // True if we need to copy the Root partition during initialization
    bool copySystem;      // True if we need to copy the System partition during initialization

    // For handling the canceling of an update
    bool cancelUpdate;    // True if we are trying to cancel the update

    bool cachedInitCompleted; // True if the last update we attempted completed initialization
    
  public:

    explicit OTAManager(UBootManager &bootMgr);

    // Called to process an incoming command
    //  We can only handle commands of type
    //  OTAUpdate
    //  OTAStatus
    std::vector<std::unique_ptr<Message>> ProcessCommand(const Message &message);

    // Called to cancel an update
    void Cancel();

    // Must be called periodically to handle internal bookeeping, such as thread join()ing
    bool Process();
    
  protected:
    UBootManager &bootMgr; // A handle to our boot manager, for setting container validity

    // Our message handlers
    std::vector<std::unique_ptr<Message>> processActionMessage(const Message &message);
    std::vector<std::unique_ptr<Message>> processStatusMessage(const Message &message);

    // To prepare for an update we potentially need to copy the current container over to
    //  the alternate, or backup container.  We also have to clear the cache on Android
    // If noCopy == true, we will skip the copy step, but the alternate container will probalby
    //  not be bootable
    bool prepareForUpdate(bool noCopy = false);

    // Called to process a chunk, and to process a chunk file
    void processChunk();
    bool processChunkFile(const ChunkInfo &chunk, const std::string &path);

    // Process a manifest file.  This will extract all the chunks needed for the
    //  update, and call the prepareForUpdate function to start initialization
    bool processManifest(const std::string &manifest);
    void initUpdateFunction();

    // Targets for pthread's
    friend void* CopyThreadFunction(void *data);
    friend void* ProcessThreadFunction(void *data);
  };  
};

#endif

