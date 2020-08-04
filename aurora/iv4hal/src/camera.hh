#ifndef __IV4_CAMERA_HH
#define __IV4_CAMERA_HH

#include <string>
#include <memory>
#include <vector>
#include <tuple>

namespace iv4 {
  // TODO: This is kind of ugly.  Is there a way to handle typed enum's as bitfields?
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

  private:
    unsigned int _width, _height;
    ImageType _type;
    std::vector<uint8_t> _data;
  };
  
  class CameraInterface {
  public:
    // TODO: Replace all this with an identifier that gets
    //       matched to a camera in a config file
    CameraInterface(const std::string &ovideoDev,
                    int width, int height);
    bool InitializeV4L2();
    
    ~CameraInterface();
    
    inline bool IsGood() const { return _camfd >= 0; }
    
    std::unique_ptr<Image> GetRawImage();

    
    static std::tuple<int,int> InitializeBaslerCamera(int mediaDevNum);
    static void PlaybackBaslerFile(std::string playbackFile, std::string mediaDev);
    
  private:
    int _width, _height;
    std::string _vdev;
    
    int _camfd;
    int num_planes;

    struct cambuf {
      uint8_t *addr;
      size_t len;
    };
    std::vector<cambuf> buffers;
    
    //uint8_t **_cambuf;
    //size_t  *_cambuflen;
    //unsigned int _num_cambufs;
    
  }; // end CameraInterface
}; // end namespace
#endif
