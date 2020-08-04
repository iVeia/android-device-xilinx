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
//#include <stdint.h>
//#include <stdio.h>
//#include <string.h>
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

  Image::Image(int width, int height, ImageType type) :
    _width(width), _height(height), _type(type) {
    _data.clear();
  }

  // The assumption here is that dat is width * height in size
  Image::Image(int width, int height, uint8_t *dat, int datLen, ImageType type) :
    Image(width, height, type) {
    debug << Debug::Mode::Debug << "Making an image from location " << static_cast<void *>(dat) << " with length " << datLen << std::endl;
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
  }

  bool CameraInterface::InitializeV4L2() {
    // open up the video device
    
    _camfd = open(_vdev.c_str(), O_RDWR | O_NONBLOCK);
    
    if(_camfd < 0) {

    } else {
      struct v4l2_requestbuffers req;
      CLEAR(req);

      // TODO: How many buffers?
      req.count = 4;
      req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      req.memory = V4L2_MEMORY_MMAP;
      
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
        buf2.memory = V4L2_MEMORY_MMAP;
        buf2.m.planes = planes;
        buf2.length = VIDEO_MAX_PLANES;
        int err = xioctl(_camfd, VIDIOC_QUERYBUF, &buf2);
        if (err) {
          debug << Debug::Mode::Failure << "Querying planes: " << strerror(errno) << std::endl;
          return false;
        }
        // TODO: Handle multi-planar formats?
        num_planes = buf2.length;
        debug << Debug::Mode::Debug << "Num planes: " << num_planes << std::endl;
      }
      
      for(unsigned int bnum = 0; bnum < req.count; bnum++) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        CLEAR(buf);
        CLEAR(planes);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = bnum;
        buf.m.planes = planes;
        buf.length = VIDEO_MAX_PLANES;
        
        if (-1 == xioctl(_camfd, VIDIOC_QUERYBUF, &buf)) {
          debug << Debug::Mode::Failure << _vdev << ": VIDIOC_QUERYBUF: " << strerror(errno) << std::endl;;
          return false;
        }

        debug << Debug::Mode::Debug << "query buf = len:" <<
          planes[0].length << " offset:" << planes[0].m.mem_offset << ":" << std::endl;
        
        size_t tbuflen = planes[0].length;
        void *tbuf     = mmap(NULL                   /* start anywhere */,
                              planes[0].length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED             /* recommended */,
                              _camfd,
                              planes[0].m.mem_offset);

        // Then add it to our list of buffers
        cambuf push_buf;
        push_buf.addr = static_cast<uint8_t *>(tbuf);
        push_buf.len = tbuflen;
        buffers.push_back(push_buf);
        
        debug << Debug::Mode::Debug << "Buffer " << bnum << " is at offset " << static_cast<void*>(buffers[bnum].addr) <<
          " with length " << buffers[bnum].len << std::endl;
        
        if (MAP_FAILED == buffers[bnum].addr) {
          debug << Debug::Mode::Failure << _vdev << ": mmap: " << strerror(errno) << std::endl;;
        } else {
          
          // Then we queue up the buffer
          if (-1 == xioctl(_camfd, VIDIOC_QBUF, &buf)) {
            debug << Debug::Mode::Failure << _vdev << ": QBUF " << bnum << " " << strerror(errno) << std::endl;;
            return false;
          }
        }
      } // end for over each buffer


      enum v4l2_buf_type type;
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      if (-1 == xioctl(_camfd, VIDIOC_STREAMON, &type)) {
        debug << Debug::Mode::Failure << _vdev << ": StreamOn " << strerror(errno) << std::endl;;
        return false;
      }
    }

    return true;
  } // end CameraInterface::CameraInterface

  CameraInterface::~CameraInterface() {
    // open says it returns a "small non-negative integer" so I guess 0 is valid
    if(_camfd >= 0) close(_camfd);

    // TODO: Need to release v4l2 stuff
  } // end CameraInterface::~CameraInterface
  
  std::unique_ptr<Image> CameraInterface::GetRawImage() {
    if(_camfd < 0) return std::unique_ptr<Image> (nullptr);

    // TODO: For continuous capture, this should go in a loop starting here
    // --------------------------------------------------------------------
    fd_set fds;
    struct timeval tv;
    int r;

    FD_ZERO(&fds);
    FD_SET(_camfd, &fds);
    
    /* Timeout. */
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    debug << Debug::Mode::Debug << "Starting select with " << _camfd << std::endl;
    r = select(_camfd + 1, &fds, NULL, NULL, &tv);
    debug << Debug::Mode::Debug << "select returned " << r << std::endl;

    if (-1 == r) {
      debug << Debug::Mode::Err << "select: " << strerror(errno) << std::endl;
      return std::unique_ptr<Image> (nullptr);
    }

    if (0 == r) {
      debug << Debug::Mode::Err << "select timeout" << std::endl;
      return std::unique_ptr<Image> (nullptr);
    }

    // Read a frame
    struct v4l2_buffer buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    CLEAR(buf);
    CLEAR(planes);
    
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length   = VIDEO_MAX_PLANES;
    
    int tryCount = 0;
    const int numTimesToTry = 20; // 2 seconds
    while(tryCount < numTimesToTry) {
      if (-1 == xioctl(_camfd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
        case EAGAIN:
          debug << Debug::Mode::Warn << "Got EAGAIN on buffer dequeue" << std::endl;
          usleep(10000);
          tryCount++;
          break;
        default:
          debug << Debug::Mode::Failure << "buffer dequeue: " << strerror(errno) << std::endl;
          return std::unique_ptr<Image> (nullptr);
        }
      } else {
        debug << "Got a frame from buffer " << buf.index << std::endl;
        break;
      }
    }

    if(tryCount >= numTimesToTry) {
      debug << Debug::Mode::Failure << "Buffer de-queue tries exceeded: " << std::endl;
      return std::unique_ptr<Image> (nullptr);      
    }

    // Pull the data into an image
    // TODO: This copies the image data twice.  Once to get it into the image and once to get it into a message
    //       We should be able to send directly from a buffer to socket
    int which = buf.index;
    std::unique_ptr<Image> retImage(new Image(_width, _height, buffers[which].addr, buffers[which].len));

    // And requeue the buffer for future use
    if (-1 == xioctl(_camfd, VIDIOC_QBUF, &buf)) {
      debug << Debug::Mode::Err << "Failed to re-queue buffer: " << strerror(errno) << std::endl;
      // TODO: What to do here? Ignoring it probably isn't good, but we still have buffers?
    } else {
      debug << "Queue buffer " << buf.index;
    }

    return retImage;
  }


} // end namespace