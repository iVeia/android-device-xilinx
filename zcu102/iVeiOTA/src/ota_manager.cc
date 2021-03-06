#include <algorithm>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <array>
#include <sys/types.h>
#include <sys/wait.h>

#include "ota_manager.hh"
#include "config.hh"
#include "debug.hh"
#include "support.hh"

namespace iVeiOTA {
  // These threads are targets for pthread functions.  They may not be needed anymore
  void* CopyThreadFunction(void *data) {
    OTAManager *manager = (OTAManager*)data;
    manager->initUpdateFunction();
    manager->joinCopyThread = true;
    return nullptr;
  }
  void* ProcessThreadFunction(void *data) {
    OTAManager *manager = (OTAManager*)data;
    manager->processChunk();
    manager->joinProcessThread = true;
    return nullptr;
  }

  // Extract the chunk type based on the (string) name
  OTAManager::ChunkType OTAManager::GetChunkType(const std::string &name) {
        if(name == "image")        return ChunkType::Image;
        else if(name == "file")    return ChunkType::File;
        else if(name == "script")  return ChunkType::Script;
        else if(name == "archive") return ChunkType::Archive;
        else if(name == "dummy")   return ChunkType::Dummy;

        else                       return ChunkType::Unknown;
    }

  OTAManager::OTAManager(UBootManager &bootMgr) : bootMgr(bootMgr) {
    // Set our internal state to default to no update in progress and not doing anything
    processingChunk = false;
    maxIdentLength = 0;
    chunks.clear();
    state = OTAState::Idle;

    // Our threads are idle
    copyThread = -1;
    processThread = -1;
    joinCopyThread = false;
    joinProcessThread = false;

    // We have no update in progress, so no update to cancel
    cancelUpdate = false;

    // Then we need to look and see if there is an upate currently in progress and,
    //  if so, try and restore it
    bool manifestValid = false;
    try {
      std::ifstream manifest_cache(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
      std::stringstream ss;
      ss << manifest_cache.rdbuf();

      // TODO: Think more about the 10K manifest size limit
      if(ss.str().length() > 0 && ss.str().length() < 1024*10) {
        debug << Debug::Mode::Info << "Cached manifest seems to be valid...  processing" << std::endl;
        manifestValid = processManifest(ss.str());
      }
    } catch(...) {
      // There was no cached manifest, so if there is a journal we should delete it
      manifestValid = false;
      RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
      RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/journal");
      RunCommand("sync"); // We also have to sync the filesystem so we can reboot
    }

    if(manifestValid) {
      state = OTAState::UpdateAvailable;
      // There was, what appears to be, a valid cached manifest from a previous update
      //  So look to see if there is a cached journal so we can try and continue from
      //  where we left off
      try {
        std::ifstream journal(std::string(IVEIOTA_CACHE_LOCATION) + "/journal");
        std::string line;
        debug << Debug::Mode::Info << "Processing cached journal" << std::endl;

        // A line in the journal is <ident:state> for each chunk that is processed
        //  state is:
        //    1 - succeeded
        //    anything else - failed
        //  We don't need to keep not processed information, because things only go
        //  into the journal when they are processed
        while(std::getline(journal, line)) {
          if(line.find("init_success")!= std::string::npos) {
            debug << Debug::Mode::Debug << " c> Cached init successful" << std::endl;
          } else {
            std::vector<std::string> toks = Split(line, ":");
            if(toks.size() < 2) continue;

            // Check to see if this journaled chunk was in the cached manifest
            auto chunk = std::find_if(chunks.begin(), chunks.end(),
                                      [&toks](const ChunkInfo &x) { return x.ident == toks[0];});
            if(chunk != chunks.end()) {
              // It was in the manifest
              if(toks[1] == "1") {
                chunk->processed = true;
                chunk->succeeded = true;
              } else {
                chunk->processed = true;
                chunk->succeeded = false;
              }
            } else {
              // We have a chunk in the journal that isn't in the manifest
              //  This is an error so we cannot continue the previous update
              state = OTAState::Idle;
              chunks.clear();
              RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
              RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/journal");
              RunCommand("sync"); // We also have to sync the filesystem so we can reboot
              break;
            }
          }
        }
      } catch(...) {
        // There was no journal but that's fine - we just start from the beginning
        state = OTAState::Idle;
      }
    }
  }

  // Process a command from the network interface
  std::vector<std::unique_ptr<Message>> OTAManager::ProcessCommand(const Message &message) {
    debug << "Processing command message" << std::endl;
    switch(message.header.type) {
    case Message::OTAStatus:
      debug << " -- status message" << std::endl;
      return processStatusMessage(message);
      break;

    case Message::OTAUpdate:
      debug << " -- update message" << std::endl;
      return processActionMessage(message);
      break;

    default:
      debug << Debug::Mode::Warn << "Unknown message: " << message.header.toString() << std::endl;
      std::vector<std::unique_ptr<Message>> ret;
      ret.push_back(Message::MakeNACK(message, 0, "Unknown message"));
      return ret;
    }
  }

  // Process an action message.  Action messages are those that trigger some sort of
  //  activity in the OTA manager
  // TODO: This should really be split up.  It is a little large for a switch statement
  std::vector<std::unique_ptr<Message>> OTAManager::processActionMessage(const Message &message) {
    std::vector<std::unique_ptr<Message>> ret;

    // Make sure we aren't trying to process a message that isn't actually for us
    if(message.header.type != Message::OTAUpdate) {
      debug << Debug::Mode::Err << "processActionMessage called with the wrong type of message!" << message.header.toString() << std::endl;
      ret.push_back(Message::MakeNACK(message, 0, "processActionMessage called with the wrong type of message"));
      return ret;
    }

    switch(message.header.subType) {

      // ****************************************************************************** //
      // ------------------------- Begin an update ---------------------------------------
      // ****************************************************************************** //
    case Message::OTAUpdate.BeginUpdate:
    {
      debug << "Got begin update message" << std::endl;
      if(state != OTAState::Idle) {
        debug << "Update already in progress" << std::endl;
        ret.push_back(Message::MakeNACK(message, 0, "Update in progress or can be continued"));
      } else {
        debug << "State => initing" << std::endl;
        state = OTAState::Initing;

        // Otherwise, we have to process the provided manifest
        std::string manifest;
        if(message.header.imm[0] == 0) {
          // Manifest is in the payload
          // TODO: This has not been tested
          manifest.assign(message.payload.begin(), message.payload.end());
        } else if(message.header.imm[0] == 1) {
          // Manifest is on the filesystem and payload contains the path
          std::string path(message.payload.begin(), message.payload.end());
          debug << "Manifest path: " << path << std::endl;
          try {
            // Read all the manifest data into a string for processing
            std::ifstream input(path);
            std::stringstream ss;
            ss << input.rdbuf();
            manifest = ss.str();
          } catch(...) {
            debug << "Failed to read manifest data into a string" << std::endl;
            manifest = "";
            ret.push_back(Message::MakeNACK(message, 0, "Invalid Manifest Path"));
          }
        }

        // If we managed to make a manifest out of that, then process it
        // TODO: Think more about this
        // If a manifest is larger than 10K bytes, assume it is not correct
        if(manifest.length() > 0 &&
           manifest.length() < 1024*10) {
          if(processManifest(manifest)) {
            // If we are here, then we have a proper manifest and can continue with the update

            {
              debug << "state -> preparing" << std::endl;
              state = OTAState::Preparing;
              if(prepareForUpdate()) {
                // We succeeded in starting the preparation for an update
                ret.push_back(Message::MakeACK(message));
              } else {
                // Failed to prepare for update
                debug << "Failed to prepare: state -> idle" << std::endl;
                state = OTAState::Idle;
                ret.push_back(Message::MakeNACK(message, 0, "Failed to prepare for update"));
              }
            }
          } else {
            // Failed to process the manifest
            debug << "Manifest invalid: state -> idle" << std::endl;
            state = OTAState::Idle;
            ret.push_back(Message::MakeNACK(message, 0, "Failed to process manifest"));
          }
        } else {
          debug << "Manifest invalid: state -> idle" << std::endl;
          state = OTAState::Idle;
          ret.push_back(Message::MakeNACK(message, 0, "Could not read manifest"));
        }
      } // end if(state != Idle) else
    } // end case BeginUpdate
    break;

    // ****************************************************************************** //
    // ------------------------- Continue an update ---------------------------------
    // ****************************************************************************** //
    case Message::OTAUpdate.ContinueUpdate:
    {
      if(state != OTAState::UpdateAvailable) {
        ret.push_back(Message::MakeNACK(message, 0, "No update to continue"));
      } else {
        // There is an update to continue
        int completed = 0;
        for(auto chunk : chunks) { if(chunk.processed == true) completed++; }
        debug << Debug::Mode::Info << "Continuing an update with " << completed << " chunks completed" << std::endl;

        {
          if(prepareForUpdate(completed > 0)) {
            ret.push_back(Message::MakeACK(message));
          } else {
            ret.push_back(Message::MakeNACK(message, 0, "Failed to continue update"));
          }
        }
      }
    }
    break;

    // ****************************************************************************** //
    // ------------------------- Cancel an update -----------------------------------
    // ****************************************************************************** //
    case Message::OTAUpdate.CancelUpdate:
    {
      if(state == OTAState::Idle) {
        ret.push_back(Message::MakeNACK(message, 0, "Update not in progress"));
      } else {
        // We can't do all this right now, so set a flag so it happens in
        //  processing
        Cancel();
        ret.push_back(Message::MakeACK(message));
      }
    }
    break;

    // ****************************************************************************** //
    // ------------------------- Process a chunk ------------------------------------
    // ****************************************************************************** //
    case Message::OTAUpdate.ProcessChunk:
    {
      debug << "Got process chunk message" << std::endl;
      if(state != OTAState::InitDone) {
        ret.push_back(Message::MakeNACK(message, 0, "Cannot process chunk now"));
      } else {
        // First we have to get the identifier out of the payload
        std::string ident = "";
        unsigned int identEnd = 0;
        debug << " -- processing chunk " << identEnd << ":" << maxIdentLength << std::endl;
        for(; identEnd < message.payload.size() && identEnd < maxIdentLength; identEnd++) {
          if(message.payload[identEnd] == '\0') {
            break;
          }
          ident += (char)message.payload[identEnd];
        }
        debug << " -- chunk ident: " << ident << "  " << identEnd << std::endl;

        // The payload should be at a null-terminator.  If it isn't something went wrong
        if(identEnd >= message.payload.size() || message.payload[identEnd] != '\0') {
          debug << Debug::Mode::Warn << "Malformed chunk message" << std::endl;
          ret.push_back(Message::MakeNACK(message, 0, "Malformed process message"));
        } else {
          // Valid Chunk identifier, check to see if it is in our list
          auto chunk = std::find_if(chunks.begin(), chunks.end(),
                                    [&ident](const ChunkInfo &x) { return x.ident == ident;});
          if(chunk != chunks.end()) {
            // Found it, so indicate that we are processing it
            whichChunk = chunk->ident;
            processingChunk = true;

            // We should process this chunk, so first extract the path to the data
            if(message.header.imm[0] == 0) {
              // TODO: Implement chunk data in message payload
              // payload contains the chunk data
              debug << Debug::Mode::Err << "Chunk data in payload not yet supported" << std::endl;
              ret.push_back(Message::MakeNACK(message, 0, "Chunk data in payload not implemented"));

              // Mark this as failed for now
              chunk->processed = true;
              chunk->succeeded = false;
              whichChunk = "";
              processingChunk = false;
            } else if(message.header.imm[0] == 1) {
              // payload contains the path to the chunk data, but may have null terminators

              // Get past the null terminator
              identEnd++;
              auto itEnd = message.payload.begin() + identEnd;
              while(itEnd != message.payload.end() && *itEnd != 0) itEnd++;

              // Get the string that is the path to the file, then send it for processing
              std::string path(message.payload.begin() + identEnd, itEnd);
              debug << "Chunk path " << path << std::endl;
              intChunkPath = path;

              // TODO: Investigate this
              // We need to increase the stack size for the processing thread.  For some reason the
              //  CopyFileData function (or the function that calls it) overflows the 1M stack
              // Increasing it to 4M seems to be plently, but overflowing a 1M stack is surprising
              //  1M is a lot of stack space, so something seems odd here
              size_t stacksize;
              pthread_attr_t attr;
              pthread_attr_init(&attr);
              pthread_attr_getstacksize(&attr, &stacksize);
              debug << "Setting process stack size: " << pthread_attr_setstacksize(&attr, stacksize * 4) << " ** " << std::endl;
              pthread_attr_getstacksize(&attr, &stacksize);
              debug << "Stack process size is " << stacksize << std::endl;

              // Create the thread and start running it
              if(pthread_create(&processThread, &attr, &ProcessThreadFunction, (void*)this) == 0) {
                ret.push_back(Message::MakeACK(message));
              } else {
                debug << Debug::Mode::Failure << "Could not create copy thread" << std::endl;
                ret.push_back(Message::MakeNACK(message, 0, "Could not create copy thread"));
              }
            } else {
              debug << Debug::Mode::Warn << "Invalid chunk data location" << std::endl;
              ret.push_back(Message::MakeNACK(message, 0, "Invalid chunk data location"));
            }
          } else {
            debug << Debug::Mode::Warn << "Chunk identifier not found" << std::endl;
            ret.push_back(Message::MakeNACK(message, 0, "Chunk identifier not found"));
          } // end if(found chunk) :: else
        } // end if(payload null terminated) :: else
      } // end if(state != InitDone) :: else
    }
    break;

    // ****************************************************************************** //
    // ------------------------- Finalize an update ---------------------------------
    // ****************************************************************************** //
    case Message::OTAUpdate.Finalize:
    {
      // Send status back based on what state we are currently in
      //  We can only finalize when all chunks have been processed successfully
      if(state != OTAState::AllDone) {
        switch(state) {
        case OTAState::Idle:
        case OTAState::UpdateAvailable:
          ret.push_back(Message::MakeNACK(message, 0, "OTABegin has not been called"));
          break;
        case OTAState::Initing:
        case OTAState::Preparing:
          ret.push_back(Message::MakeNACK(message, 0, "OTABegin initialization not complete"));
          break;
        case OTAState::InitDone:
          ret.push_back(Message::MakeNACK(message, 0, "Chunks remaining to be processed"));
          break;
        case OTAState::AllDoneFailed:
          ret.push_back(Message::MakeNACK(message, 0, "Some chunks failed processing"));
          break;
        default:
          ret.push_back(Message::MakeNACK(message, 0, "Internal error"));
        } // end switch(state)
      } else {
        if(singleContainerOnly) {
          debug << Debug::Mode::Info << "Skipping container switching as this is a single partition download" << std::endl;
        } else {
          int currentRev = bootMgr.GetRev(Container::Active);
          debug << "Setting alternate rev to " << currentRev + 1 << std::endl;
          bootMgr.SetAll(Container::Alternate, true, true, 0, currentRev + 1);
        }

        // We have to clear out our list of chunks so that we can do another udpate if we want to
        chunks.clear();

        // Clear out some internal convenience state
        maxIdentLength = 0;

        // Remove the cached manifest and journal so we don't accidentally resume them
        RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
        RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/journal");
        RunCommand("sync"); // We also have to sync the filesystem so we can reboot

        // Move back to the idle state
        state = OTAState::Idle;

        // And send a positive response back
        ret.push_back(Message::MakeACK(message));
      }
    }
    break;

    default:
    {
      debug << Debug::Mode::Warn << "Invalid command sub type: " << message.header.toString() << std::endl;
      ret.push_back(Message::MakeNACK(message, 0, "Invalid command sub type"));
    }
    break;
    } // end switch(subType)

    return ret;
  }

  // Process a status message.  A status message is read-only and just gets information from
  //  the OTA server
  std::vector<std::unique_ptr<Message>> OTAManager::processStatusMessage(const Message &message) {
    std::vector<std::unique_ptr<Message>> ret;

    if(message.header.type != static_cast<uint32_t>(Message::OTAStatus)) {
      debug << "Not the right type in OTAManager::processStatusMessage" << std::endl;
      return ret;
    }

    debug << " subType: " << (int)message.header.subType << std::endl;
    switch(message.header.subType) {
    case Message::OTAStatus.UpdateStatus:
    {
      uint32_t status = 0;
      switch(state) {
      case OTAState::Idle:            status = 0; break;
      case OTAState::UpdateAvailable: status = 1; break;
      case OTAState::Initing:         status = 2; break;
      case OTAState::Preparing:       status = 3; break;
      case OTAState::Canceling:       status = 3; break; // Canceling will count as preparing - TODO: Revisit this...
      case OTAState::InitDone:
        if(!processingChunk) {
          status = 4;
        } else {
          status = 5;
        }
        break;
      case OTAState::AllDone:         status = 6; break;
      case OTAState::AllDoneFailed:   status = 6; break;
      }

      uint32_t allPassed = (state == OTAState::AllDone)?1:0;

      if(status == 5) {
        // Put which chunk we are processing into the payload
        std::vector<uint8_t> payload(whichChunk.begin(), whichChunk.end());
        payload.push_back('\0');
        ret.push_back(std::unique_ptr<Message>(new Message(Message::OTAStatus, Message::OTAStatus.UpdateStatus,
                                                           status, 0, 0, 0, payload)));
      } else {
        // Else the payload is empty
        ret.push_back(std::unique_ptr<Message>(new Message(Message::OTAStatus, Message::OTAStatus.UpdateStatus,
                                                           status, allPassed, 0, 0)));
      }
    }
    break;

    case Message::OTAStatus.ChunkStatus:
    {
      debug << "Chunk status message" << std::endl;
      std::vector<uint8_t> payload;
      for(auto chunk : chunks) {
        // Identifier first
        std::copy(chunk.ident.begin(), chunk.ident.end(), std::back_inserter(payload));
        payload.push_back(':');
        // Then the current status of this chunk
        if(chunk.ident == whichChunk) payload.push_back('1');
        else if(chunk.processed &&  chunk.succeeded) payload.push_back('2');
        else if(chunk.processed && !chunk.succeeded) payload.push_back('3');
        else payload.push_back('0');
        // Then the order matters flag for this chunk
        payload.push_back(':');
        if(chunk.orderMatters) payload.push_back('1');
        else payload.push_back('0');

        if(chunk.type == ChunkType::Script) {
          payload.push_back(':');
          std::string ret_val = std::to_string(chunk.exitCode);
          for(char c : ret_val) payload.push_back(c);
        }
        
        // Null terminate the list
        payload.push_back('\0');
      }

      // Double null terminate the list
      payload.push_back('\0');
      ret.push_back(std::unique_ptr<Message>(new Message(Message::OTAStatus, Message::OTAStatus.ChunkStatus,
                                                         chunks.size(), 0, 0, 0, payload)));
    }
    break;

    default:
      ret.push_back(Message::MakeNACK(message, 0, "Unknown message subtype"));
      debug << "Unknown subtype in processStatusMessage: " << message.header.subType << std::endl;
    }

    return ret;
  }

  void OTAManager::initUpdateFunction() {
    debug << "Download thread starting" << std::endl;
    //TODO: This may be dangerous as it creates a power-cycle race condition
    //      If you power cycle ater copying but before setting validity then you may
    //        power back up into the backup container.
    if(copyBI) {
      debug << "starting to copy BI" << std::endl;
      std::string src = config.GetDevice(Container::Active, Partition::BootInfo);
      std::string dest = config.GetDevice(Container::Alternate, Partition::BootInfo);
      debug << "Copying file " << src << " to " << dest << std::endl;
      CopyFileData(dest, src, 0, 0, &cancelUpdate);

      debug << "Setting alternate validity to false after copying BI partition" << std::endl;
      bootMgr.SetValidity(Container::Alternate, false);
    }

    if(copyRoot) {
      debug << "starting to copy Root" << std::endl;
      std::string src = config.GetDevice(Container::Active, Partition::Root);
      std::string dest = config.GetDevice(Container::Alternate, Partition::Root);
      CopyFileData(dest, src, 0, 0, &cancelUpdate);
    }

    if(copySystem) {
      debug << "starting to copy System" << std::endl;
      std::string src = config.GetDevice(Container::Active, Partition::System);
      std::string dest = config.GetDevice(Container::Alternate, Partition::System);
      CopyFileData(dest, src, 0, 0, &cancelUpdate);
    }

    // Then we have to clear the cache
    if(clearCache) {
      debug << "clearing the cache" << std::endl;
      std::string cache = config.GetDevice(Container::Alternate, Partition::Cache);
      if(cache.length() > 1) {
        // Make sure we have something to try and mount
        // TODO: Should add more checks here.  This can be very destructure
        Mount mount(cache, IVEIOTA_MNT_POINT);
        if(mount.IsMounted()) {
          // TODO: Really need to make sure this always works
          RemoveAllFiles(mount.Path() + "/", true);
        } else {
          debug << Debug::Mode::Err << "Unable to mount cache partition" << std::endl;
        }
      }// unmount
    }

    // Save the fact that we finised initialization off to the journal
    try {
      debug << "Init succeeded: " << std::endl;
      std::ofstream journal(std::string(IVEIOTA_CACHE_LOCATION) + "/journal", std::ios::out | std::ios::app);
      RunCommand("sync"); // We also have to sync the filesystem so we can reboot
      journal << "init_success" << std::endl;;
    } catch(...) {
      //TODO: Implement logging
      // Failed to write to the journal -- can't resume a failed update
    }
    debug << "Thread finished" << std::endl;
  }

  bool OTAManager::prepareForUpdate(bool noCopy) {
    // TODO: A better way --
    //  A config file tells what redundant partition types the system has (from the list in support.hh)
    //  Keep a map/vector of partitions that we will copy an image into and set flag
    //  at the end copy over all partitions that don't have the flag set
    // Need to get to the point of config file processing before that can happen
    debug << Debug::Mode::Info << "Preparing for update" << std::endl;
    copyBI     = true;
    copyRoot   = true;
    copySystem = true;
    clearCache = true;

    // First check to see if all chunks are in a single container
    bool singleOnly = true;
    unsigned int singleCount = 0;
    for(auto chunk : chunks) {
      if(config.IsSinglePartition(chunk.dest)) {
        singleCount++;
      } else {
        singleOnly = false;
      }
    }

    if(singleCount == chunks.size()) {
      debug << "Single count == chunks.size() = " << singleCount << std::endl;
    }

    if(singleOnly) {
      debug << "singleOnly was true" << std::endl;
    }

    if(singleOnly && (singleCount == chunks.size())) {
      debug << Debug::Mode::Info << "All chunks are on single partitions, no need to switch" << std::endl;
      singleContainerOnly = true;
      
      clearCache = false;
      copyBI     = false;
      copyRoot   = false;
      copySystem = false;
    } else {
      singleContainerOnly = false;
      clearCache = true;
    }

    for(auto chunk: chunks) {
      bool copy = true;
      if(chunk.type == ChunkType::Image) {
        copy = false;
      } else if(chunk.type == ChunkType::Archive && chunk.complete == true) {
        copy = false;
      }

      if(!copy || noCopy || singleContainerOnly) {
        if(     chunk.dest == Partition::BootInfo) copyBI     = false;
        else if(chunk.dest == Partition::Root)     copyRoot   = false;
        else if(chunk.dest == Partition::System)   copySystem = false;
      }
    }

    debug << Debug::Mode::Info << (copyBI     ? "" : "Not ") << "Copying BootInfo" << std::endl;
    debug << Debug::Mode::Info << (copyRoot   ? "" : "Not ") << "Copying Root"     << std::endl;
    debug << Debug::Mode::Info << (copySystem ? "" : "Not ") << "Copying System"   << std::endl;
    debug << Debug::Mode::Info << (clearCache ? "" : "Not ") << "Clearing Cache"   << std::endl;
    bootMgr.SetValidity(Container::Alternate, false);

    // -------------------------------------------------------------------------------------------------------
    // We have to make the thread stack size larger.  See the comment for processChunk for more details
    size_t stacksize;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &stacksize);
    debug << "Setting stack size: " << pthread_attr_setstacksize(&attr, stacksize * 4) << " ** " << std::endl;
    pthread_attr_getstacksize(&attr, &stacksize);
    debug << "Stack size is " << stacksize << std::endl;
    int ret = pthread_create(&copyThread, &attr, &CopyThreadFunction, (void*)this);
    if(ret != 0) {
      debug << Debug::Mode::Failure << "Could not create chunk process thread" << std::endl;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    debug << "Exiting after thread created" << std::endl;
    return ret == 0;
  }

  void OTAManager::processChunk() {
    // Find the chunk we are supposed to process first
    std::string ident = this->whichChunk;
    auto chunk = std::find_if(chunks.begin(), chunks.end(), [&ident](const ChunkInfo &x) { return x.ident == ident;});
    if(chunk != chunks.end()) {
      debug << "Processing chunk: " << whichChunk << std::endl;
      // Process the chunk
      bool success = false;
      if(processChunkFile(*chunk, intChunkPath)) {
        chunk->processed = true;
        chunk->succeeded = true;
        success = true;
      } else {
        chunk->processed = true;
        chunk->succeeded = false;
        success = false;
      }

      // THis isn't a real good way to do this.  We need to refactor the processing somewhat
      if(chunk->type == ChunkType::Script) chunk->exitCode = lastExitCode;

      try {
        debug << "Succeeded in processing chunk: " << chunk->ident << std::endl;
        std::ofstream journal(std::string(IVEIOTA_CACHE_LOCATION) + "/journal", std::ios::out | std::ios::app);
        RunCommand("sync"); // We also have to sync the filesystem so we can reboot
        journal << chunk->ident << ":" << (success ? "1" : "2") << std::endl;
      } catch(...) {
        //TODO: Implement logging
        // Failed to write to the journal -- can't resume a failed update
        debug << Debug::Mode::Failure << "Failed to write to the journal" << std::endl;
      }
    } else {
      debug << "Didn't find the chunk: " << whichChunk << std::endl;
    }
  }

  bool OTAManager::processChunkFile(const ChunkInfo &chunk, const std::string &path) {
    bool success = false;

    // First, see if the file exists
    {
      debug << "File " << path << " doesn't exist" << std::endl;
      std::ifstream existTest(path);
      if(!existTest.good()) return false;
    } // end scope to close file

    // Then we need to check the hash
    {
      std::string hashValue = GetHashValue(chunk.hashType, path);
      if(hashValue != chunk.hashValue) {
        debug << "Hashed values differed: " << hashValue << "::" << chunk.hashValue << std::endl;

        if(chunk.type != ChunkType::Dummy) return false;
      }
    }

    debug << "Processing " << ToString(chunk.type) << ":" << path << std::endl;

    // Then process it based on type
    switch(chunk.type) {

      ///////////////////////////////////////////////////////////////////////////
    case ChunkType::Image:
    {
      std::string dest = config.GetDevice(Container::Alternate, chunk.dest);
      uint64_t offset = chunk.pOffset;
      uint64_t size = chunk.size;
      debug << "Writing image " << path << " to " << dest << " offset: " << offset << " size: " << size << std::endl;
      uint64_t written = CopyFileData(dest, path, offset, size, &cancelUpdate);
      if(written != size) {
        debug << "Didn't write proper amount: " << written << ":" << size << std::endl;
        return false;
      }
    }
    success = true;
    break;

    ///////////////////////////////////////////////////////////////////////////
    case ChunkType::Archive:
    {
      std::string dest = config.GetDevice(Container::Alternate, chunk.dest);
      std::string ftype = config.GetFilesystemType(dest);

      {
        Mount mount(dest, IVEIOTA_MNT_POINT, ftype);
        if(!mount.IsMounted()) {
          debug << "Failed to mount device" << std::endl;
          success = false;
          break;
        }

        if(chunk.complete) {
          // We have to clear out the old files first
          debug << "Clearing out old files for complete archive on " << dest << std::endl;
          RemoveAllFiles(mount.Path() + "/", true);
        }
      } // unmount and sync filesystem -- not waiting here caused tar to fail

      // TODO: Verify that the sleep and unmount/mount is needed.  May be an artifact of previous problem
      sleep(2);

      {
        Mount mount(dest, IVEIOTA_MNT_POINT, ftype);
        if(!mount.IsMounted()) {
          debug << "Failed to mount device second time" << std::endl;
          success = false;
          break;
        }

        // Then we have to untar it
        // TODO: Check the output of tar for success/failure
        std::string command = "/system/bin/tar -xvv --overwrite -f " + path + " -C " + mount.Path() + "/";
        std::string output = RunCommand(command);
      } // unmount
    }
    success = true;
    break;

    ///////////////////////////////////////////////////////////////////////////
    case ChunkType::Script:
    {
      // Simply invoke the script if we get this far
      std::string command = path;
      int status;
      int exitCode;
      std::string output = RunCommandWithRet("/system/bin/sh " + command, status);
      exitCode = WEXITSTATUS(status);
      lastExitCode = exitCode;

      if(!WIFEXITED(status)) {
        success = false;
        break;
      }
    }
    success = true;
    break;

    ///////////////////////////////////////////////////////////////////////////
    case ChunkType::File:
    {
      std::string dest = config.GetDevice(Container::Alternate, chunk.dest);
      std::string ftype = config.GetFilesystemType(dest);

      Mount mount(dest, IVEIOTA_MNT_POINT, ftype);
      if(!mount.IsMounted()) {
        debug << "Failed to mount device" << std::endl;
        success = false;
        break;
      }

      //TODO: I would like to use an internal method, not call out to RunCommand
      std::string command = "cp -f " + path + " " + mount.Path() + "/" + chunk.filePath;
      std::string output = RunCommand(command);
    }
    success = true;
    break;

    ///////////////////////////////////////////////////////////////////////////
    case ChunkType::Dummy:
    {
      debug << "Processing dummy chunk" << std::endl;
      success = true;
    }
    break;

    default:
      success = false;
      break;
    }

    return success;
  }

  bool OTAManager::processManifest(const std::string &manifest) {
    // The manifest is a list of chunks in the format
    // ident:type:partition:order:<params_list>:hash_type:hash_value
    // ident is a string identifier
    // type is the type of chunk
    //      image, file, script
    // partition is the destination of the chunk/file
    //      root, system, boot_info, boot, data, qspi
    // order is 0/false or 1/true indicating if this chunk has
    //   has to be processed in the order it appears in the manifest
    //   All order=true chunks must appear at the start of the manifest
    // params_list depends on the chunk type
    //  For images:
    //   pOffset:fOffset:num_bytes
    //   pOffset is the offset in the physical device to transfer chunk data
    //   fOffset is the file offset of the image file that this chunk contains (not needed?)
    //   num_bytes are how many bytes in this chunk (starting at zero) to copy to the device
    //  For Archives:
    //   complete
    //   complete is whether this image will completely overwrite the destination
    //  For files:
    //   dest_path
    //   dest_path is the location the file should be placed at (including path and name)
    //  For dummy:
    //   dest_path - a file to (possibly) test for hash calculations.  If it doesn't exist
    //               this isn't a problem.  It will be ignored.
    // hash_type is the type of the hash value
    // hash_value is the value of the hash for integrity checking
    debug << Debug::Mode::Info << "Processing manifest" << std::endl;
    std::vector<std::string> lines = Split(manifest, "\r\n");
    for(std::string line : lines) {
      // TODO: put these hard coded restrictions someplace more visible
      // ~1500 charactes for a path seem like a reasonable limit
      if(line.length() > 2048) continue;
      std::vector<std::string> toks = Split(line, ":");
      if(toks.size() < 6) {
        continue;
      }
      ChunkInfo chunk;
      chunk.processed = false;

      chunk.ident = toks[0];
      chunk.type = GetChunkType(toks[1]);
      chunk.dest  = GetPartition(toks[2]);

      // Sanity check the type and destination
      if(chunk.type == ChunkType::Unknown || chunk.dest == Partition::Unknown) {
        debug << "Type or destination incorrect: " <<
          static_cast<int>(chunk.type) <<
          static_cast<int>(chunk.dest) << std::endl;
        continue;
      }

      if((chunk.type == ChunkType::Image && toks.size() < 9) ||
         (chunk.type == ChunkType::File && toks.size() < 7) ||
         (chunk.type == ChunkType::Archive && toks.size() < 7)) {
        debug << "Incorrect number of params for " << line << " #" << toks.size() << std::endl;
        continue;
      }

      // Check to see if we have a specific order we have to process this chunk in
      // TODO: proper handling of this needs to be implemented
      chunk.orderMatters = (toks[3].length() > 0 && toks[3][0] == '1') ? true : false;

      // Get the hash information
      chunk.hashType = GetHashAlgorithm(toks[toks.size() - 2]);
      chunk.hashValue = toks[toks.size() - 1];
      // Sanity check it
      if(chunk.hashType == HashAlgorithm::Unknown ||
         (chunk.hashType == HashAlgorithm::None && chunk.type != ChunkType::Dummy)) {
        debug << "Incorrect type of hash type" << std::endl;
        continue;
      }

      // Then get the chunk specific stuff
      switch(chunk.type) {
      case ChunkType::Image:
        chunk.pOffset = strtoll(toks[4].c_str(), 0, 10);
        chunk.fOffset = strtoll(toks[5].c_str(), 0, 10);
        chunk.size    = strtoll(toks[6].c_str(), 0, 10);
        break;

      case ChunkType::Archive:
        chunk.complete = ((toks[4].length() > 0) && (toks[4][0] == '1'));
        break;

      case ChunkType::Dummy:
        chunk.dest = Partition::None;
        // fall through to set filepath
      case ChunkType::File:
        chunk.filePath = toks[4];
        break;

      case ChunkType::Script:
        //nothing special to do for script chunks
        break;

      default:
        // Don't process this
        debug << Debug::Mode::Err << "Unknown chunk type: " << ToString(chunk.type) << std::endl;
        continue;
      }

      // If we made it here, then we have a valid chunk
      debug << "maxIdentLength : " << maxIdentLength << " new ident length: " << chunk.ident.length() << std::endl;
      if(chunk.ident.length() > maxIdentLength) maxIdentLength = chunk.ident.length();

      debug << Debug::Mode::Info << "Found chunk: " << chunk.ident << ":" << iVeiOTA::ToString(chunk.dest) << std::endl;
      chunks.push_back(chunk);
    } // end for(line : lines)

    if(chunks.size() <= 0) {
      // This doesn't seem like a valid manifest since there are no chunks in it
      debug << Debug::Mode::Err << "Invalid manifest" << std::endl;
      return false;
    }

    // This seems to be a valid manifest, so we should save it to the cache
    try {
      std::ofstream manifest_cache(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
      manifest_cache << manifest;
    } catch(...) {
      debug << Debug::Mode::Err << "Failed to cache manifest" << std::endl;
    }
    return true;
  }

  void OTAManager::Cancel() {
    // Setting this flag will cause the processing threads to exit
    cancelUpdate = true;

    // Then we have to clear out our other state
    state = OTAState::Canceling;
  }

  bool OTAManager::Process() {
    // Do anything that we need to check on periodially
    if(joinCopyThread && copyThread != -1) {
      // We need to join the copy thread
      pthread_join(copyThread, NULL);
      copyThread = -1;
      joinCopyThread = false;

      if(!cancelUpdate) {
        bool allDone = true;
        for(auto chunk : chunks) {
          if(chunk.processed == false) allDone = false;
        }
        if(allDone) state = OTAState::AllDone;
        else        state = OTAState::InitDone;
      }
    }

    if(joinProcessThread && processThread != -1) {
      // Need to join the process chunk thread
      pthread_join(processThread, NULL);
      processThread = -1;
      joinProcessThread = false;

      // update our data about this chunk
      processingChunk = false;
      whichChunk = "";
      intChunkPath = "";

      // We should check to see if all chunks have been processed now
      if(!cancelUpdate) {
        bool allProcessed = true;
        bool allSucceeded = true;
        for(auto chunk : chunks) {
          if(!chunk.processed) allProcessed = false;
          if(!chunk.succeeded) allSucceeded = false;
        }

        if(allProcessed && allSucceeded) state = OTAState::AllDone;
        else if(allProcessed) state = OTAState::AllDoneFailed;
      }
    }

    // If all our threads are join()ed, then we can turn off the cancel flag
    //  and move back to the idle state
    if(cancelUpdate &&
       (!joinProcessThread && processThread == -1) &&
       (!joinCopyThread && copyThread == -1)
      ) {
      debug << Debug::Mode::Debug << "Update cancel completed. Updating status to reflect" << std::endl;
      cancelUpdate = false;
      chunks.clear();
      maxIdentLength = 0;

      // We have to remove any cached files too
      RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
      RemoveFile(std::string(IVEIOTA_CACHE_LOCATION) + "/journal");
      RunCommand("sync"); // We also have to sync the filesystem so we can reboot

      state = OTAState::Idle;
    }

    return true;
  }
};
