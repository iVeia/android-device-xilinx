#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <asm/types.h>		/* for videodev2.h */
#include <linux/videodev2.h>

#include <fstream>
#include <tuple>

#include "camera.hh"
#include "debug.hh"
#include "support.hh"

namespace iv4 {

  Image::Image() : Image(0,0,ImageType::UYVY) {}
  Image::Image(int width, int height, ImageType type) :
    _width(width), _height(height), _type(type) {
    _data.clear();
  }

  // The assumption here is that dat is width * height in size
  Image::Image(int width, int height, uint8_t *dat, int datLen, ImageType type) :
    Image(width, height, type) {
    debug << Debug::Mode::Debug << "Making an image from location " << static_cast<void *>(dat) <<
      " with length " << datLen << std::endl;
    switch(type) {
    case ImageType::UYVY:
      for(int i = 0; i < datLen; i++) _data.push_back(dat[i]);
      break;
    default:
      debug << Debug::Mode::Failure << "Type not yet supported" << std::endl;
    }    
  }
  
  // The format of the ioctl for the basler cameras
  struct register_access {
    uint16_t address;
    uint8_t  data[256];
    uint16_t data_size;
    uint8_t  command;
  };
#define GENCP_STRING_BUFFER_SIZE        (64)
#define STRING_TERMINATION              (1)
#define BDI_MAGIC                       (84513200)
  struct basler_device_information {
    uint32_t _magic;
    uint32_t gencpVersion;
    uint8_t  manufacturerName[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
    uint8_t  modelName[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
    uint8_t  familyName[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
    uint8_t  deviceVersion[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
    uint8_t  manufacturerInfo[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
    uint8_t  serialNumber[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
    uint8_t  userDefinedName[GENCP_STRING_BUFFER_SIZE + STRING_TERMINATION];
  };

  // TODO: C++-ism desired here
#define CLEAR(x) memset(&(x), 0, sizeof(x))  

  static int xioctl(int fd, int request, void *arg) {
    int r;

    do {
      r = ioctl (fd, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
  }

  static int basler_get_info(const std::string &subdev, struct basler_device_information *info) {
    int media_fd = open(subdev.c_str(), O_RDWR);
    struct v4l2_ext_controls ext_controls;
    struct v4l2_ext_control  ext_control;

    ext_control.id = 0x981003;   // Basler device info
    ext_control.size = sizeof(struct basler_device_information);
    ext_control.string = (char*)info;//
    ext_controls.ctrl_class = 0; // User class
    ext_controls.count = 1;      // Always just one control at a time
    ext_controls.controls = &ext_control;
    int ret2 = ioctl(media_fd, VIDIOC_G_EXT_CTRLS, &ext_controls);
  
    close(media_fd);

    return ret2;
  }
  
  static int basler_read(int fd, uint16_t addr, uint8_t *dat, uint16_t dat_size) {
    struct v4l2_ext_controls ext_controls;
    struct v4l2_ext_control  ext_control;

    struct register_access rega;
    rega.address = addr;
    rega.data_size = dat_size;
    rega.command = 1; // 1 is read, 2 is write

    int ret1, ret2;

    ext_control.id = 0x981002;   // Basler get register
    ext_control.size = sizeof(struct register_access);//
    ext_control.string = (char*)&rega;//
    ext_controls.ctrl_class = 0; // User class
    ext_controls.count = 1;      // Always just one control at a time
    ext_controls.controls = &ext_control;
  
    // First we set the control to save the address, 
    ret1 = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_controls);

    // Then we get the control to get the data out of it
    ret2 = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_controls);

    return ret1 * 1000 + ret2;
  }

  static int basler_write(int fd, uint16_t addr, uint8_t *dat, uint16_t dat_size) {
    struct v4l2_ext_controls ext_controls;
    struct v4l2_ext_control  ext_control;

    struct register_access rega;
    rega.address = addr;
    for(int i = 0; i < 256; i++) {
      if(i < dat_size) rega.data[i] = dat[i];
      else rega.data[i] = 0;
    }
    rega.data_size = dat_size;
    rega.command = 2; // 1 is read, 2 is write

    int ret;

    ext_control.id = 0x981002;   // Basler get register
    ext_control.size = sizeof(struct register_access);//
    ext_control.string = (char*)&rega;//
    ext_controls.ctrl_class = 0; // User class
    ext_controls.count = 1;      // Always just one control at a time
    ext_controls.controls = &ext_control;

    // All we do is set the control 
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_controls);

    return ret;
  }

  std::tuple<int,int> CameraInterface::InitializeBaslerCamera(int mediaDevNum) {
    std::string mediaSubDev = "";
    if(mediaDevNum == 0) {
      mediaSubDev = "/dev/v4l-subdev0";      
    }  else if(mediaDevNum == 1) {
      debug << Debug::Mode::Failure << "Media device " << mediaDevNum << " not yet supported" << std::endl;
      return std::tuple<int,int>(0,0);
    } else {
      debug << Debug::Mode::Failure << "Media device " << mediaDevNum << " not known" << std::endl;
      return std::tuple<int,int>(0,0);
    }

    // First we have to get the type of camera that is plugged in
    struct basler_device_information binfo;
    int iret = basler_get_info(mediaSubDev, &binfo);
    std::string camModel = std::string( (char*)binfo.modelName);
    debug << "Got basler info: " << iret << std::endl <<
      "\t model:  " << (char*)binfo.modelName << std::endl <<
      "\t family: " << (char*)binfo.familyName << std::endl;

    int cameraType = 0;
    if(camModel.find("daA2500-60") != std::string::npos) cameraType = 1;
    else if(camModel.find("daA4200-30") != std::string::npos) cameraType = 2;
    else {
      debug << Debug::Mode::Failure << "Unrecognized camera type" << std::endl;
      return std::tuple<int,int>(0,0);
    }
       
    // Then we have to setup the resolution and format of the pipeline
    // TODO: This is hardcoded.  That is TERRIBLE.  This needs to be more
    //       generic / configurable when we have time.  If the cameras change locations, or the device
    //       tree moves dev nodes around this will fail horribly.
    std::string format = "";
    std::string vformat = "";
    std::string width = "";
   std::string height = "";
    switch(cameraType) {
    case 1:
      format = "UYVY8_1X16";
      vformat = "UYVY";
      width = "2592";
      height = "1944";
      break;
    case 2:
      format = "UYVY8_1X16";
      vformat = "UYVY";
      width = "4208";
      height = "3120";
      break;
    default:
      debug << Debug::Mode::Failure << "Unknown camera type! " << cameraType << std::endl;
      return std::tuple<int,int>(0,0);
    }

    if(mediaDevNum == 0) {
      RunCommand("/system/bin/media-ctl -d /dev/media0 -V '\"a0000000.mipi_csi2_rx_subsystem\":0 [fmt:"+format+"/"+width+"x"+height+" field:none]'");
      RunCommand("/system/bin/media-ctl -d /dev/media0 -V '\"a0000000.mipi_csi2_rx_subsystem\":1 [fmt:"+format+"/"+width+"x"+height+" field:none]'");
      RunCommand("/system/bin/media-ctl -d /dev/media0 -V '\"iveia-basler-mipi 7-0036\":0 [fmt:"+format+"/"+width+"x"+height+" field:none]'");
      RunCommand("/system/bin/v4l2-ctl -d /dev/video0 --set-fmt-video=width="+width+",height="+height+",pixelformat='"+vformat+"'");
    }

    // 
    
    switch(cameraType) {
    case 1:
      PlaybackBaslerFile("/system/etc/basler_5MP.playback", mediaSubDev);
      return  std::tuple<int,int>(2592,1944);
      break;
    case 2:
      PlaybackBaslerFile("/system/etc/basler_14MP.playback", mediaSubDev);
      return  std::tuple<int,int>(4208, 3120);
      break;
    default:
      // This should have been handled already
      break;
    }

    return std::tuple<int,int>(0,0);
  }


  //TODO: Check to make sure these files exist
  void CameraInterface::PlaybackBaslerFile(std::string playbackFile, std::string mediaDev) {
    // Next we have to open our playback file and sent it to the camera
    std::ifstream playback(playbackFile.c_str());

    std::string line;
    int media_fd = open(mediaDev.c_str(), O_RDWR);
    while(getline(playback, line)) {

      bool isWrite = !(line[0] == ',');

      uint8_t dat[512];
      uint8_t dat2[512];

      // Get the individual tokens - be careful, split doesn't insert blank strings into list
      auto toks = Split(line, ",");

      int len = toks.size();
      if(len <= 2) {
        debug << Debug::Mode::Warn << " ******************** Length is too short: " <<
          len << "(" << line << ")" << std::endl;
      } else {
        uint8_t addr[2];

        addr[1] = strtol(toks[0].c_str(), NULL, 16);
        addr[0] = strtol(toks[1].c_str(), NULL, 16);

        int dat_at = 0;
        for(int i = 2; i < len; i++) {
          if(toks[i][0] == '=') continue;
        
          dat[dat_at++] = strtol(toks[i].c_str(), NULL, 16);
        }

        if(isWrite) {
          basler_write(media_fd, *((uint16_t*)addr), dat, len-2);
        } else {
          basler_read(media_fd, *((uint16_t*)addr), dat2, len-2);
        }
      }
    } // end while(getline)

    close(media_fd);
    playback.close(); // This will happen when scope closes, but do it anyway
    
  } // end Initialize

  
  CameraInterface::CameraInterface(const std::string &videoDev,
                                   int width, int height) :
    _width(width), _height(height), _vdev(videoDev), _camfd(-1)  {    
    streaming = false;
  }

  std::unique_ptr<Message> CameraInterface::ProcessMessage(const Message &m) {
    if(m.header.type != Message::Image) return Message::MakeNACK(m, 0, "Invalid message passed to CameraInterface");

    switch(m.header.subType) {
    case Message::Image.ContinuousCapture:
      {
        if(m.header.imm[2] == 1) {
          // Enable capturing
          captureSkip = m.header.imm[3];
          if(captureSkip < 0) captureSkip = 0;
          captureTypes = GetImageTypes(m.header.imm[1]);
          if(!streaming) StreamOn();
          capturing = true;
        } else {
          // disable capturing
          capturing = false;
          StreamOff();
          captureTypes.clear();
        }
      }
      break;
      
    case Message::Image.CaptureImage:
      {
        oneshotTypes = GetImageTypes(m.header.imm[1]);
        oneshotImages.clear();
        oneshot = true;
      }
      break;
    case Message::Image.GetImage:
      {
        // In order to get an image, we need:
        // 1) An image type (and only one image type)
        // 2) The image type must be in the capture list
        // 3) The image type must have been captured
        std::vector<ImageType> imgType = GetImageTypes(m.header.imm[1]);
        if(imgType.size() != 1) {
          return Message::MakeNACK(m, 0, "GetImage can only return one image type");
        } else if(std::find(oneshotTypes.begin(), oneshotTypes.end(), (imgType[0])) == oneshotTypes.end()) {
          return Message::MakeNACK(m, 0, "Image type not in capture list");
        } else if(oneshotImages.find(imgType[0]) == oneshotImages.end()) {
          return Message::MakeNACK(m, 0, "Image type not captured");          
        } else {
          // Get the captured (oneshot) image and send it.  If we don't have a oneshot image, it is an error
          Image &activeImage0 = oneshotImages[imgType[0]];
          int imageSize = activeImage0.Size();
          int w = activeImage0.Width();
          int h = activeImage0.Height();
          debug << "Getting image: " << w << ":" << h << "(" << imageSize << ")" << std::endl;
          
          uint32_t res = ((w<<16) & 0xFFFF0000) | (h & 0x0000FFFF);
          uint32_t msgs = 0x00010001; // At the moment we only send one message / image
          uint32_t itype = ToInt(activeImage0.Type());
          
          return std::unique_ptr<Message>(new Message(Message::Image,
                                                      Message::Image.SendImage,
                                                      0, // camera number
                                                      itype,
                                                      res, msgs,
                                                      activeImage0.GetData()));
        }
      }      
      break;      
    default:
      return Message::MakeNACK(m, 0, "Unknown message subtype");
      break;
    }

    debug << "Process Image message: streaming=" << streaming <<
      " capturing=" << capturing <<
      " oneshot=" << oneshot << std::endl;
    
    return Message::MakeACK(m);
  }

  bool CameraInterface::StreamOn() {
    enum v4l2_buf_type type;
    if(streaming) return true;
    debug << "Turning streaming on" << std::endl;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (-1 == xioctl(_camfd, VIDIOC_STREAMON, &type)) {
      debug << Debug::Mode::Failure << _vdev << ": StreamOn " << strerror(errno) << std::endl;;
      return false;
    }

    streaming = true;
    return streaming;
  }

  bool CameraInterface::StreamOff() {
    // TODO: We can't turn the camera off at the moment, so we can't disable streaming
    //        When we have control over the camera, we have to turn it off first, then send STREAMOFF
    streaming = false;
    return !streaming;
    /*
    enum v4l2_buf_type type;
    if(!streaming) return true;
    debug << "Turning streaming off" << std::endl;
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (-1 == xioctl(_camfd, VIDIOC_STREAMOFF, &type)) {
      debug << Debug::Mode::Failure << _vdev << ": StreamOff " << strerror(errno) << std::endl;;
      return false;
    }

    streaming = false;
    return !streaming;
    */
  }

  bool CameraInterface::InitializeV4L2() {
    // open up the video device
    
    _camfd = open(_vdev.c_str(), O_RDWR | O_NONBLOCK);
    debug << "Opened camera device: " << _camfd << " for " << (void*)this << std::endl;
    
    if(_camfd < 0) {

    } else {
      struct v4l2_requestbuffers req;
      CLEAR(req);

      // TODO: How many buffers?
      req.count = 4;
      req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      //req.memory = V4L2_MEMORY_MMAP;
      //req.memory = V4L2_MEMORY_DMABUF;
      req.memory = V4L2_MEMORY_USERPTR;
      
      if (-1 == xioctl(_camfd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
          debug << Debug::Mode::Failure << _vdev << " does not support memory mapping" << std::endl;;
          return false;
        } else {
          debug << Debug::Mode::Failure << _vdev << " reqbuf" << strerror(errno) << std::endl;;
          return false;
        }
      }

      if (req.count < 2) {
        debug << Debug::Mode::Failure <<  _vdev << ": Insufficient buffer memory" << std::endl;;
        return false;
      }

      {
        // Now we have to query the planar information
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        struct v4l2_buffer buf2;
        
        CLEAR(buf2);
        CLEAR(planes);
        buf2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        //buf2.memory = V4L2_MEMORY_MMAP;
        //buf2.memory = V4L2_MEMORY_DMABUF;
        buf2.memory = V4L2_MEMORY_USERPTR;
        buf2.m.planes = planes;
        buf2.length = VIDEO_MAX_PLANES;
        int err = xioctl(_camfd, VIDIOC_QUERYBUF, &buf2);
        if (err) {
          debug << Debug::Mode::Failure << "Querying planes: " << strerror(errno) << std::endl;
          return false;
        }
        // TODO: Handle multi-planar formats?
        num_planes = buf2.length;
        plane_len = planes[0].length;
        num_buffers = req.count;
        
        debug << Debug::Mode::Debug << "Num planes: " << num_planes <<
          " plane size: " << plane_len << std::endl;
        if(num_planes != 1) {
          debug << Debug::Mode::Warn << "Number of planes is not 1.  Not supported: " << num_planes << std::endl;
        }
      }

      {
        // We need to get our udma buffer now
        int ufd = open("/dev/udmabuf0", O_RDWR);
        if(ufd < 0) {
          debug << Debug::Mode::Warn << "Opening " << "/dev/udmabuf0" << " failed." << std::endl;
        } else {
          // Map in the user dma buffer and close the backing file
          udma_len = num_buffers * plane_len;
          udma_addr = static_cast<uint8_t*>(mmap(NULL, udma_len, PROT_READ | PROT_WRITE, MAP_SHARED, ufd, 0));
          close(ufd);
          
          debug << "Opened " << "/dev/udmabuf0" << " and mapped " << udma_len << " bytes to " <<
            static_cast<void*>(udma_addr) << std::endl;
          
          // I need to touch every page in the mmap'd region to page them in
          uint8_t *buf_ptr = udma_addr;
          uint8_t qq_tmp = 77;
          while(buf_ptr < (udma_addr + udma_len)) {
            buf_ptr[0] = qq_tmp;
            if(buf_ptr[0] != qq_tmp) {
              debug << Debug::Mode::Failure << "Buffer read/write verification failed: " <<
                buf_ptr[0] << ":" << qq_tmp << std::endl;
              //return false;
            }
            buf_ptr++;
            qq_tmp = qq_tmp * 2;
          }
        }
      }
            
      for(unsigned int bnum = 0; bnum < req.count; bnum++) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        
        CLEAR(buf);
        CLEAR(planes);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        //buf.memory      = V4L2_MEMORY_MMAP;
        //buf.memory      = V4L2_MEMORY_DMABUF;
        buf.memory      = V4L2_MEMORY_USERPTR;
        buf.index       = bnum;
        buf.m.planes    = planes;
        buf.length      = VIDEO_MAX_PLANES;
        
        if (-1 == xioctl(_camfd, VIDIOC_QUERYBUF, &buf)) {
          debug << Debug::Mode::Failure << _vdev << ": VIDIOC_QUERYBUF: " << strerror(errno) << std::endl;;
          return false;
        }

        debug << Debug::Mode::Debug << "query buf = len:" <<
          planes[0].length << " offset:" << planes[0].m.mem_offset << ":" << std::endl;
        
        size_t tbuflen = planes[0].length;        
        // for mmap memory
        //void *tbuf     = mmap(NULL                   /* start anywhere */,
        //                      planes[0].length,
        //                      PROT_READ | PROT_WRITE /* required */,
        //                      MAP_SHARED             /* recommended */,
        //                      _camfd,
        //                      planes[0].m.mem_offset);
        //void *tbuf = mmap(NULL,
        //                  planes[0].length,
        //                  PROT_READ | PROT_WRITE,
        //                  MAP_SHARED,
        //                  fd,
        //                  0);
        
        cambuf push_buf;        
        push_buf.len = tbuflen;
        push_buf.addr = udma_addr + (tbuflen * bnum);
        push_buf.fd = -1;
        buffers.push_back(push_buf);
        
        debug << Debug::Mode::Debug << "Buffer " << bnum << "(" << buffers[bnum].fd << ")" << 
          " is at offset " << static_cast<void*>(buffers[bnum].addr) <<
          " with length " << buffers[bnum].len << std::endl;
        
        //if (MAP_FAILED == buffers[bnum].addr) {
        //  debug << Debug::Mode::Failure << _vdev << ": mmap: " << strerror(errno) << std::endl;;
        //  return false;
        //}
        
        // Then we queue up the buffer
        planes[0].m.userptr = reinterpret_cast<unsigned long>(buffers[bnum].addr);
        debug << "buffer " << bnum << ":" << 0 << " addr is: " <<
          reinterpret_cast<void*>(buf.m.planes[0].m.userptr) << std::endl;
        if (-1 == xioctl(_camfd, VIDIOC_QBUF, &buf)) {
          debug << Debug::Mode::Failure << _vdev << ": QBUF failed " << bnum << " " << strerror(errno) << std::endl;
          return false;
        } else {
          debug << "queueing buffer " << buf.index << " for " << _camfd << std::endl;
        }
      } // end for over each buffer
    }

    return true;
  } // end CameraInterface::CameraInterface

  CameraInterface::~CameraInterface() {
    // open says it returns a "small non-negative integer" so I guess 0 is valid
    debug << "Closing " << _camfd << std::endl;
    if(_camfd >= 0) close(_camfd);

    for(auto cbuf : buffers) {
      if(cbuf.len > 0) munmap(cbuf.addr, cbuf.len);
    }
    
  } // end CameraInterface::~CameraInterface

  bool CameraInterface::ProcessMainLoop(SocketInterface &intf) {
    if(!IsGood()) {
      //debug << "Camera is not good in ProcessMainLoop: " << _camfd << ":" <<
      //(void*)this << std::endl;
      return true; // Not nescessarilly a problem, but might be
    }
    
    // If we are not streaming, we may need to turn that on
    if(!streaming && capturing) {
      // This is an error, but don't exit because of it
      debug << Debug::Mode::Err << "Streaming is off but we are capturing" << std::endl;
      return true;
    }

    if(!streaming && oneshot) {
      // We need to turn streaming on so that we can capture
      if(!StreamOn()) {
        debug << Debug::Mode::Failure <<
          "Turning streaming on in ProcessMainLoop failed" << std::endl;
      }
    }

    if(!streaming) {
      // Now, if we are not streaming, there is no reason to continue as an image can't be available
      //debug << "Not streaming" << std::endl;

      //TODO: Once we can turn the camera off, we can short circuit out here,
      //      but without the ability to do that, we are technically always streaming
      //return true;
    }

    // Now check to see if a frame is available
    struct v4l2_buffer buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    CLEAR(buf);
    CLEAR(planes);
    
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    //buf.memory   = V4L2_MEMORY_MMAP;
    //buf.memory   = V4L2_MEMORY_DMABUF;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.m.planes = planes;
    buf.length   = VIDEO_MAX_PLANES;

    // NOTE: This event loop relies on the videodev being open in NONBLOCK mode
    if (-1 == xioctl(_camfd, VIDIOC_DQBUF, &buf)) {
      switch (errno) {
      case EAGAIN:
        // There is no frame yet, so continue on and try again next time
        debug << "Got EAGAIN getting a frame" << std::endl;
        return true;
      default:
        // There is an error in the v4l2 pipeline
        debug << Debug::Mode::Failure <<
          "buffer dequeue: " << _camfd << ":" << strerror(errno) << std::endl;
        return false;
      }
    } else {
      debug << "Got a frame from buffer " << buf.index << " on " << _camfd << std::endl;
    }

    // This is the buffer we are processing this time
    struct cambuf &imgBuf = buffers[buf.index];

    // TODO: Support more image types here?
    uint32_t wcam = 0; // TODO: At the moment we only have one camera, need to know the number if there are >1
    uint32_t res = ((_width<<16) & 0xFFFF0000) | (_height & 0x0000FFFF);
    uint32_t msgs = 0x00010001; // At the moment we only send one message / image
    uint32_t itype = ToInt(ImageType::UYVY);

    if(capturing) {
      if(skippingAt == captureSkip) {
        // We need to send this frame
        Message::Header hdr(Message::Image, Message::Image.SendImage,
                            wcam, itype, res, msgs,
                            imgBuf.len);

        debug << "Sending an image as an event " << std::endl;
        intf.Send(hdr, imgBuf.addr, imgBuf.len);
      }

      if(++skippingAt > captureSkip) skippingAt = 0;
    }

    // See if we have a one shot we need to capture
    if(oneshot) {
      // TODO: itype in the event message below is supposed to be a bitmask of image types we have captured
      //       At the moment we only support raw UYVY, but we need to support more at some point
      oneshotImages[ImageType::UYVY] = Image(_width, _height, imgBuf.addr, imgBuf.len, ImageType::UYVY);
      intf.Send(Message(Message::Image, Message::Image.ImageCaptured,
                        wcam, itype, 0, 0));      
      oneshot = false;
      
      if(!capturing) {
        // We have to turn streaming off if this was just a oneshot
        StreamOff();
      }
    }

    // Now we have to requeue the buffer
    if (-1 == xioctl(_camfd, VIDIOC_QBUF, &buf)) {
      debug << "Failed to requeue buffer: " << strerror(errno) << std::endl;
      return false;
    }

    return true;
  }

} // end namespace
