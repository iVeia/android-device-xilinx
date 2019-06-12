#include <algorithm>
#include <thread>
#include <unistd.h>

#include "ota_manager.hh"
#include "config.hh"
#include "debug.hh"

namespace iVeiOTA {
  OTAManager::ChunkType OTAManager::GetChunkType(const std::string &name) {
        if(name == "image")        return ChunkType::Image;
        else if(name == "file")    return ChunkType::File;
        else if(name == "script")  return ChunkType::Script;
        else if(name == "archive") return ChunkType::Archive;
        else if(name == "dummy")   return ChunkType::Dummy;

        else                       return ChunkType::Unknown;
    }

  OTAManager::OTAManager(UBootManager &bootMgr, std::string configFile) : bootMgr(bootMgr) {
    // Open and process the config file
    processingChunk = false;
    maxIdentLength = 0;
    chunks.clear();
    state = OTAState::Idle;
    
    // Then we need to look and see if there is an upate currently in progress and,
    //  if so, try and restore it
    bool manifestValid = false;
    try {
      std::ifstream manifest_cache(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
      std::stringstream ss;
      ss << manifest_cache.rdbuf();
      
      std::vector<std::unique_ptr<Message>> dummy;
      
      debug << "Manifest seems to be valid...  processing" << std::endl;
      manifestValid = processManifest(ss.str(), dummy);
    } catch(...) {
      // There was no cached manifest, so if there is a journal we should delete it
      //  but that will happen after this
      manifestValid = false;
    }
    
    //TODO: Delete the journal if it is there
    if(manifestValid) {
      // There was a cached manifest, so process it and the journal
      //TODO: set the state to indicate there is a cached update available
      //  when that is working
      try {
        std::ifstream journal(std::string(IVEIOTA_CACHE_LOCATION) + "/journal");
        std::string line;
        debug << "Processing journal" << std::endl;
        while(std::getline(journal, line)) {
          std::vector<std::string> toks = Split(line, ":");
          if(toks.size() < 2) continue;
          for(auto chunk : chunks) {
            if(chunk.ident == toks[0]) {
              if(toks[1] == "1") {
                chunk.processed = true;
                chunk.succeeded = true;
              } else {
                chunk.processed = true;
                chunk.succeeded = false;
              }
            }
          }
        }
      } catch(...) {
        // There was no journal but that's fine - we just start from the beginning
      }
    }
  }
  
  std::vector<std::unique_ptr<Message>> OTAManager::ProcessCommand(const Message &message) {
    debug << "processing command message" << std::endl;
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
      debug << "unknown message" << std::endl;
      std::vector<std::unique_ptr<Message>> ret;
      ret.push_back(Message::MakeNACK(message, 0, "Unknown message"));
      return ret;
    }
  }
  
  std::vector<std::unique_ptr<Message>> OTAManager::processActionMessage(const Message &message) {
    std::vector<std::unique_ptr<Message>> ret;
    
    if(message.header.type != Message::OTAUpdate) return ret;
    
    switch(message.header.subType) {
    case Message::OTAUpdate.BeginUpdate:
    {
      debug << "Got begin update message" << std::endl;
      if(state != OTAState::Idle) {
        debug << "update in progress" << std::endl;
        ret.push_back(Message::MakeNACK(message, 0, "Update in progress or available, cancel first"));
      } else {
        debug << "State => initing" << std::endl;
        state = OTAState::Initing;

        // Otherwise, we have to process the provided manifest
        std::string manifest;
        if(message.header.imm[0] == 0) {
          // Manifest is in the payload
          manifest.assign(message.payload.begin(), message.payload.end());
        } else if(message.header.imm[0] == 1) {
          // Manifest is on the filesystem and payload contains the path
          // TODO: Error checking
          std::string path(message.payload.begin(), message.payload.end());
          debug << "Manifest path: " << path << std::endl;
          try {
            std::ifstream input(path);
            std::stringstream ss;
            ss << input.rdbuf();
            manifest = ss.str();
          } catch(...) {            
            manifest = "";
            ret.push_back(Message::MakeNACK(message, 0, "Invalid Manifest Path"));
          }
        }
        
        // If we managed to make a manifest out of that, then process it
        if(manifest.length() > 0) {
          if(processManifest(manifest, ret)) {
            // If we are here, then we have a proper manifest and can continue with the
            //  update
          
            //TODO: This needs to be on a thread
            debug << "state -> preparing" << std::endl;
            state = OTAState::Preparing;
            if(prepareForUpdate(message.header.imm[3] == 42)) {
              ret.push_back(Message::MakeACK(message));
            } else {
              // Failed to prepare for update
              debug << "state -> idle" << std::endl;
              state = OTAState::Idle;
              ret.push_back(Message::MakeNACK(message, 0, "Failed to prepare for update"));
            }
          } else {
            // Failed to process the manifest
            debug << "state -> idle" << std::endl;
            state = OTAState::Idle;
            ret.push_back(Message::MakeNACK(message, 0, "Failed to process manifest"));
          } 
        } else {
          debug << "state -> idle" << std::endl;
          state = OTAState::Idle;
          ret.push_back(Message::MakeNACK(message, 0, "Invalid manifest"));
        }
      } // end if(state != Idle) else
    } // end case BeginUpdate
    break;
    
    case Message::OTAUpdate.ContinueUpdate:
    {
      ret.push_back(Message::MakeNACK(message, 0, "Continue not yet supported"));
    }
    break;
    
    case Message::OTAUpdate.CancelUpdate:
    {
      if(state == OTAState::Idle) {
        ret.push_back(Message::MakeNACK(message, 0, "Update not in progress"));
      } else {
        // Set our state to not active and clear out the chunks
        chunks.clear();
        //TODO: delete the journal and the cached manifest
        state = OTAState::Idle;
        ret.push_back(Message::MakeACK(message));
      }
    }
    break;
    
    case Message::OTAUpdate.ProcessChunk:
    {
      debug << "Got process chunk message" << std::endl;
      if(state != OTAState::InitDone) {
        ret.push_back(Message::MakeNACK(message, 0, "OTA Begin not yet called"));
      } else {
        // Just pass the whole message, don't bother processing anything here
        if(processChunk(message, ret)) {
          ret.push_back(Message::MakeACK(message));
        } else {
          ret.push_back(Message::MakeNACK(message, 0, "Failed to process chunk"));
        }
      }      
    }
    break;
    
    case Message::OTAUpdate.Finalize:
    {
      bool all_processed = true;
      bool all_succeeded = true;
      for(auto chunk : chunks) {
        if(!chunk.processed) all_processed = false;
        if(!chunk.succeeded) all_succeeded = false;
      }
      
      if(state != OTAState::InitDone) {
        ret.push_back(Message::MakeNACK(message, 0, "OTA Begin not yet called"));
      } else if(!all_processed) {
        ret.push_back(Message::MakeNACK(message, 0, "Not all chunks have been processed"));
      } else if(!all_succeeded) {
        ret.push_back(Message::MakeNACK(message, 0, "Not all chunks succeeded"));
      } else {
        // TODO: This mounts/remount 4 times -- need to do better at some point
        int currentRev = bootMgr.GetRev(Container::Active);
        debug << "Setting alternate rev to " << currentRev + 1 << std::endl;
        bootMgr.SetRev(Container::Alternate, currentRev + 1);
        bootMgr.SetTries(Container::Alternate, 0);
        bootMgr.SetValidity(Container::Alternate, true);
        bootMgr.SetUpdated(Container::Alternate, true);
        
        // TODO: delete the journal and cached manifest
        
        ret.push_back(Message::MakeACK(message));
      }
    }
    break;
    
    default:
    { 
      ret.push_back(Message::MakeNACK(message, 0, "Invalid Command"));
    }
    break;            
    } // end switch(subType)
    
    return ret;
  }
  
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
      case OTAState::InitDone:
        if(!processingChunk) {
          status = 4;
        } else {
          status = 5;
        }
        break;
      }
      
      if(status == 5) {
        // Put which chunk we are processing into the payload
        std::vector<uint8_t> payload(whichChunk.begin(), whichChunk.end());
        payload.push_back('\0');
        ret.push_back(std::unique_ptr<Message>(new Message(Message::OTAStatus, Message::OTAStatus.UpdateStatus,
                                                           status, 0, 0, 0, payload)));
      } else {
        // Else the payload is empty
        ret.push_back(std::unique_ptr<Message>(new Message(Message::OTAStatus, Message::OTAStatus.UpdateStatus,
                                                           status, 0, 0, 0)));
      }
    }
    break;
    
    case Message::OTAStatus.ChunkStatus:
    {
      debug << "Chunk status message" << std::endl;
      std::vector<uint8_t> payload;
      for(auto chunk : chunks) {
        std::copy(chunk.ident.begin(), chunk.ident.end(), std::back_inserter(payload));
        payload.push_back(':');
        if(chunk.ident == whichChunk) payload.push_back('1');
        else if(chunk.processed &&  chunk.succeeded) payload.push_back('2');
        else if(chunk.processed && !chunk.succeeded) payload.push_back('3');
        else payload.push_back('0');
        payload.push_back(',');
      }
      payload.push_back('\0');
      ret.push_back(std::unique_ptr<Message>(new Message(Message::OTAStatus, Message::OTAStatus.ChunkStatus,
                                                         chunks.size(), 0, 0, 0, payload)));
    }
    break;
    
    default:
      ret.push_back(Message::MakeNACK(message, 0, "Unknown message subtype"));
      debug << "Unknown subtype in processStatusMessage" << std::endl;
    }
    
    return ret;
  }
  
  void OTAManager::initUpdateFunction(bool copyBI, bool copyRoot, bool copySystem) {
    debug << "Download thread starting" << std::endl;
    //TODO: This won't really work as it creates a power-cycle race condition
    if(copyBI) {
      debug << "starting to copy BI" << std::endl;
      std::string src = config.GetDevice(Container::Active, Partition::BootInfo);
      std::string dest = config.GetDevice(Container::Alternate, Partition::BootInfo);
      debug << "Copying file " << src << " to " << dest << std::endl;
      CopyFileData(dest, src, 0, 0);
      
      sleep(1);
      debug << "Setting alternate validity to false after copying BI partition" << std::endl;
      bootMgr.SetValidity(Container::Alternate, false);
    }
    
    if(copyRoot) {
      debug << "starting to copy Root" << std::endl;
      std::string src = config.GetDevice(Container::Active, Partition::Root);
      std::string dest = config.GetDevice(Container::Alternate, Partition::Root);
      CopyFileData(dest, src, 0, 0);
    }
    
    if(copySystem) {
      debug << "starting to copy System" << std::endl;
      std::string src = config.GetDevice(Container::Active, Partition::System);
      std::string dest = config.GetDevice(Container::Alternate, Partition::System);
      CopyFileData(dest, src, 0, 0);
    }
    
    // Then we have to clear the cache
    {
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
          debug << Debug::Mode::Err << "Unable to mount cache partition" << std::endl << Debug::Mode::Info;
        }
      }// unmount
    }
    
    debug << "Thread finished" << std::endl;
    state = OTAState::InitDone;
  }
  
  bool OTAManager::prepareForUpdate(bool noCopy) {
    // TODO: A better way --
    //  A config file tells what redundant partition types the system has (from the list in support.hh)
    //  Keep a map/vector of partitions that we will copy an image into and set flag
    //  at the end copy over all partitions that don't have the flag set
    // Need to get to the point of config file processing before that can happen
    debug << Debug::Mode::Info << "Preparing for update" << std::endl;
    bool copyBI=true, copyRoot=true, copySystem=true;
    for(auto chunk: chunks) {
      bool copy = true;
      if(chunk.type == ChunkType::Image) {
        copy = false;
      } else if(chunk.type == ChunkType::Archive && chunk.complete == true) {
        copy = false;
      }

      if(!copy) {
        if(     chunk.dest == Partition::BootInfo) copyBI     = false;
        else if(chunk.dest == Partition::Root)     copyRoot   = false;
        else if(chunk.dest == Partition::System)   copySystem = false;
      }
    }
    
    debug << Debug::Mode::Info << (copyBI     ? "" : "Not ") << "Copying BootInfo" << std::endl;
    debug << Debug::Mode::Info << (copyRoot   ? "" : "Not ") << "Copying Root"     << std::endl;
    debug << Debug::Mode::Info << (copySystem ? "" : "Not ") << "Copying System"   << std::endl;    
    bootMgr.SetValidity(Container::Alternate, false);

    //std::thread copyThread([=]{this->initDownloadFunction(copyBI && !noCopy, copyRoot && !noCopy, copySystem && !noCopy);});
    initUpdateFunction(copyBI && !noCopy, copyRoot && !noCopy, copySystem && !noCopy);
    
    debug << "Exiting after thread created" << std::endl;
    return true;
  }
  
  bool OTAManager::processChunk(const Message &message, std::vector<std::unique_ptr<Message>> &ret) {
    // first we have to get the identifier out of the payload
    std::string ident = "";
    unsigned int identEnd = 0;
    debug << "processing chunk " << identEnd << ":" << maxIdentLength << std::endl;
    for(; identEnd < message.payload.size() && identEnd < maxIdentLength; identEnd++) {
      if(message.payload[identEnd] == '\0') {
        break;
      }
      ident += (char)message.payload[identEnd];
    }
    
    debug << "Chunk ident: " << ident << "  " << identEnd << std::endl;
    
    // The payload should be at a null-terminator.  If it isn't something went wrong
    if(identEnd >= message.payload.size() || message.payload[identEnd] != '\0') {
      debug << "Malformed chunk message" << std::endl;
      ret.push_back(Message::MakeNACK(message, 0, "Process Chunk message malformed"));
      return false;
    }
    identEnd++;
    
    // Then see if we have that chunk in our list
    bool success = false;
    auto chunk = std::find_if(chunks.begin(), chunks.end(), [&ident](const ChunkInfo &x) { return x.ident == ident;});
    if(chunk != chunks.end()) {
      whichChunk = chunk->ident;
      processingChunk = true;
      // We should process this chunk
      if(message.header.imm[0] == 0) {
        // payload contains the chunk data
        debug << "Chunk data in payload not yet supported" << std::endl;
        ret.push_back(Message::MakeNACK(message, 0, "Chunk data in payload not yet supported"));
      } else if(message.header.imm[0] == 1) {
        // payload contains the path to the chunk data, but may have null terminators
        auto itEnd = message.payload.begin() + identEnd;
        while(itEnd != message.payload.end() && *itEnd != 0) itEnd++;

        // Get the string that is the path to the file, then send it for processing
        std::string path(message.payload.begin() + identEnd, itEnd);
        debug << "Chunk path " << path << std::endl;
        success = processChunkFile(*chunk, path);
      }

      // Keep track of our chunk meta dataOB
      chunk->processed = true;
      chunk->succeeded = success;
    }
    
    // We processed the chunk file, so add it to the journal
    if(success) {
      try {
        debug << "Succeeded in processing chunk" << std::endl;
        std::ofstream journal(std::string(IVEIOTA_CACHE_LOCATION) + "/journal", std::ios::out | std::ios::app);
        journal << chunk->ident << ":" << (success ? "1" : "2");
      } catch(...) {
        //TODO: Implement logging
        // Failed to write to the journal -- can't resume a failed update
      }
    }
    
    processingChunk = false;
    whichChunk = "";
    
    return success;
  }
  
  bool OTAManager::processChunkFile(const ChunkInfo &chunk, const std::string &path) {
    bool success = false;
    
    //First we need to check the hash
    {
      std::string hashValue = GetHashValue(chunk.hashType, path);
      if(hashValue != chunk.hashValue) {
        debug << "Hashed values differed: " << hashValue << "::" << chunk.hashValue << std::endl;

        if(chunk.type != ChunkType::Dummy) return false;
      }
    }
    
    switch(chunk.type) {
      
      ///////////////////////////////////////////////////////////////////////////
    case ChunkType::Image:
    {      
      std::string dest = config.GetDevice(Container::Alternate, chunk.dest);
      uint64_t offset = chunk.pOffset;
      uint64_t size = chunk.size;
      debug << "Writing image " << path << " to " << dest << " offset: " << offset << " size: " << size << std::endl;
      uint64_t written = CopyFileData(dest, path, offset, size);
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

      {
        Mount mount(dest, IVEIOTA_MNT_POINT);
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
      sleep(10);
      {
        Mount mount(dest, IVEIOTA_MNT_POINT);
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
    }
    success = false;
    break;
    
    ///////////////////////////////////////////////////////////////////////////
    case ChunkType::File:
    {
      std::string dest = config.GetDevice(Container::Alternate, chunk.dest);
      Mount mount(dest, IVEIOTA_MNT_POINT);
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
      debug << "Processing dummy chunk" << std::endl;
      success = true;
      break;
      
    default:
      success = false;
      break;
    }
    
    return success;
  }
  
  bool OTAManager::processManifest(const std::string &manifest, std::vector<std::unique_ptr<Message>> &ret) {
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
      std::vector<std::string> toks = Split(line, ":");
      if(toks.size() < 6) {
        debug << Debug::Mode::Info << "  > " << line << std::endl;
        // This isn't a valid chunk
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
      chunk.orderMatters = (toks[3][0] == '1' || toks[3] == "true" || toks[3] == "True" || toks[3] == "TRUE") ? true : false;
      
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
        chunk.complete = toks[4][0] == '1';
        break;
        
      case ChunkType::Dummy:
        chunk.dest = Partition::None;
        // fall through to set filepath
      case ChunkType::File:
        chunk.filePath = toks[4];
        break;
        
      case ChunkType::Script:
        // TODO: Add a generic response message for non-failure info cases
        continue; // not handled yet
        
      default:
        // TODO: Add a generic response message for non-failure info cases
        continue;
      }
      
      // If we made it here, then we have a valid chunk
      if(chunk.ident.length() > maxIdentLength) maxIdentLength = chunk.ident.length();
      
      debug << Debug::Mode::Info << "Found chunk: " << chunk.ident << ":" << ToString(chunk.dest) << std::endl;
      chunks.push_back(chunk);
    } // end for(line : lines)
    
    // This seem to be a valid manifest, so we should save it to the cache
    try {
      std::ofstream manifest_cache(std::string(IVEIOTA_CACHE_LOCATION) + "/manifest");
      manifest_cache << manifest;
    } catch(...) {
      debug << Debug::Mode::Err << "Failed to cache manifest" << std::endl << Debug::Mode::Info;
      // Can't write to the cache, so we can't resume this download
      // TODO: Add a generic response message for non-failure info cases
    }
    return true;
  }
};
