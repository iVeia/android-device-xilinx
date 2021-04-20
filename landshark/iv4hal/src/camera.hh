#ifndef __IV4_CAMERA_HH
#define __IV4_CAMERA_HH

#include <string>
#include <memory>
#include <vector>
#include <tuple>
#include <map>
#include <pthread.h>

#include "message.hh"
#include "socket_interface.hh"
#include "debug.hh"

namespace iv4 {
  // TODO: This is kind of ugly.  Is there a way to handle typed enum's as bitfields?
  typedef struct BufferInfo_t {
    uint8_t *buf;
    uint8_t *dest;
    int width,    height;
    int widthDest,heightDest;
    int destLen;
    int index;
    bool ready;
  } BufferInfo;

  enum class ImageType {
    UYVY = 0x0001,
    RGB  = 0x0002,
    GRAY = 0x0004,
  };
  inline uint32_t ToInt(ImageType t) {
    switch(t) {
    case ImageType::UYVY: return 1;
    case ImageType::RGB:  return 2;
    case ImageType::GRAY: return 4;
    default: return 0;
    }
  }
  inline std::vector<ImageType> GetImageTypes(uint32_t typeMask) {
    std::vector<ImageType> tts;
    if(typeMask & 0x0001) tts.push_back(ImageType::UYVY);
    if(typeMask & 0x0002) tts.push_back(ImageType::RGB);
    if(typeMask & 0x0004) tts.push_back(ImageType::GRAY);

    return tts;
  }
  
  class Image {
  public:
    Image();
    Image(int width, int height, ImageType type = ImageType::UYVY);
    Image(int width, int height, uint8_t *dat, int datlen, ImageType type = ImageType::UYVY);
    
    inline int Width() const {return _width;}
    inline int Height() const {return _height;}
    inline ImageType Type() const {return _type;}
    inline int Size() const {
      return _data.size();
    }
    inline int GetData(std::vector<uint8_t> &dat, int offset, int count) {
      int bSize = dat.size();
      copy(_data.begin() + offset, _data.begin() + (offset + count), back_inserter(dat));
      return dat.size() - bSize;
    }
    inline int GetData(std::vector<uint8_t> &dat) {
      return GetData(dat, 0, _data.size());
    }
    inline const std::vector<uint8_t>& GetData() const {return _data;}

  private:
    unsigned int _width, _height;
    ImageType _type;
    std::vector<uint8_t> _data;
  };
  
  class CameraInterface {
  public:
    // Initialize  Basler I2C MIPI camera, which involves configuring the media pipeline using
    //  v4l2-ctl and media-ctl and sending our playback information to the camera
    //  via I2C.
    // We don't really know how the camera works, so we record I2C traffic from their binary
    //  only library, then play it back to configure the camera
    static std::tuple<int,int> InitializeBaslerCamera(int mediaDevNum);    

    // TODO: Replace all this with an identifier that gets
    //       matched to a camera in a config file
    CameraInterface(const std::string &ovideoDev,
                    int mediaDevNum,
                    int width, int height);
    ~CameraInterface();
    
    // V4l2 controls
    bool InitializeV4L2(); // Initialize v4l2 (query the dev node, get mmap buffers)
    bool StreamOn();       // Send STREAMON to v4l2 system
    bool DeInitializeV4L2();
    bool StreamOff();      // Send STREAMOFF to v4l2 system
          
    
    inline bool IsGood() const { return _camfd >= 0; }
    
    static void PlaybackBaslerFile(std::string playbackFile, std::string mediaDev);
    std::unique_ptr<Message> ProcessMessage(const Message &m);

    inline bool Streaming() const {return streaming;}

    // Called by the event loop to allow the camera to perform its periodic actions
    //  If needed, we can send images over the passed in interface
    bool ProcessMainLoop(SocketInterface &intf);

    inline int ReadySet(fd_set *rset) {
      if(_camfd >= 0) {
        FD_SET(_camfd, rset);
        return _camfd;
      } else return -1;
    }
    
  private:
    void ProcessBuffer(uint8_t *in);
    bool DropFrame();

    // The width and height of our capture device
    //  TODO: We can query the v4l2 dev node to get this information
    int _width, _height;
    int _width_2, _height_2;
    
    // The path to our video device 
    std::string _vdev;
    int devNum;

    // Information for the v4l2 sub-system
    int _camfd;      // The file descriptor to the /dev/videoX device
    int num_planes;  // How many planes our v4l2 capture device has
    size_t plane_len;
    int    num_buffers;

    // True if we have turned v4l2 STREAMON 
    bool streaming;

    // We can set the camera to capture mode, where it will capture all the images
    //  that are available
    bool capturing;                       // Are we capturing images
    //int captureSkip;                      // How many images to skip before sending one
    //int skippingAt;                       // How many images we have skipped so far
    std::vector<ImageType> captureTypes;  // The Types of images we are supposed to be capturing
    
    // We also allow the capture of a single image.  We store that image until a new one is captured
    bool oneshot;                         // True if we want to capture only one image, then stop
    std::vector<ImageType> oneshotTypes;
    std::map<ImageType, Image> oneshotImages;

    // user dma buffers
    uint8_t *udma_addr;
    size_t udma_len;    
    
    // Our v4l2 video buffers
    struct cambuf {
      uint8_t *addr;
      size_t len;
      int fd; // for dma purposes
    };
    std::vector<cambuf> buffers;

    uint8_t *dest;
    uint32_t destLen;
  }; // end CameraInterface
}; // end namespace
#endif
