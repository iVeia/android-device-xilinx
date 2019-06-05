#ifndef __IVEIOTA_DEBUG_HH
#define __IVEIOTA_DEBUG_HH

#include <iostream>
#include <string>

namespace iVeiOTA {
  class Debug {
  public:
    enum class Mode {
      Info = 0,
      Debug,
      Warn,
      Err,
      Failure,
    };

    // default to quiet
    Debug() {
      currMode = static_cast<int>(Mode::Info);
      threshMode = static_cast<int>(Mode::Failure);
    }

    Debug& SetThreshold(const Mode &m) {threshMode = static_cast<int>(m); return *this;}    
    Debug& operator<<(const Mode &m) {currMode = static_cast<int>(m); return *this;}


    template <class T>
    Debug& operator<<(const T &s) {
      if(currMode >= threshMode) {
        if(currMode <= static_cast<int>(Mode::Warn)) std::cout << s;
        else                                         std::cerr << s;
      }
      return *this;
    }
    
    typedef std::basic_ostream<char, std::char_traits<char> > ct;
    typedef ct& (*_endl)(ct&);
    Debug& operator<<(_endl func) {
      if(currMode >= threshMode) {
        if(currMode <= static_cast<int>(Mode::Warn)) func(std::cout);
        else                                         func(std::cerr);
      }
      
      return *this;
    }
    
  protected:
    int currMode;
    int threshMode;
  };

  extern Debug debug;
};

#endif
