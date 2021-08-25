#ifndef __IVEIOTA_DEBUG_HH
#define __IVEIOTA_DEBUG_HH

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

namespace iv4 {
  class Debug {
  public:
    enum class Mode {
      Debug = 0, // Debug information
      Info,      // Normal program flow information
      Warn,      // Things that can happen but are bad
      Err,       // Things that should NOT happen
      Failure,   // Things that put the system into a broken state 
    };

    enum class Info {
      Time = 0, // Print time stamp
    };

    // default to (relatively) quiet
    Debug() {
      currMode   = static_cast<int>(Mode::Info);
      defMode    = static_cast<int>(Mode::Debug);
      threshMode = static_cast<int>(Mode::Failure);
    }

    // The threshold is where we start printing messages.  Any message below the
    //  threshold will not be printed
    Debug& SetThreshold(const Mode &m) {threshMode = static_cast<int>(m); return *this;}

    // The default is what messages with no explicit mode will be printed as
    Debug& SetDefault(const Mode &m) {defMode = static_cast<int>(m); return *this;}

    //  The printing operators
    Debug& operator<<(const Mode &m) {currMode = static_cast<int>(m); return *this;}
    Debug& operator<<(const Info &info) {
      std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
      time_t now_time = std::chrono::system_clock::to_time_t(now);

      auto gmt_time = gmtime(&now_time);
      auto timestamp = std::put_time(gmt_time, "%Y-%m-%d %H:%M:%S");

      (*this) << timestamp << ": ";

      return *this;
    }

    template <class T>
    Debug& operator<<(const T &s) {
      // Only print if this message is at or above the current theshold
      // Messages at Warn or higher go to cerr
      if(currMode >= threshMode) {
        if(currMode <= static_cast<int>(Mode::Warn)) std::cout << s;
        else                                         std::cerr << s;
      }
      return *this;
    }    
    typedef std::basic_ostream<char, std::char_traits<char> > ct;
    typedef ct& (*_endl)(ct&);
    Debug& operator<<(_endl func) {
      // Only print if this message is at or above the current theshold
      // Messages at Warn or higher go to cerr
      if(currMode >= threshMode) {
        if(currMode <= static_cast<int>(Mode::Warn)) func(std::cout);
        else                                         func(std::cerr);
      }

      // Reset ourselves back to default on a std::endl
      currMode = defMode;
      return *this;
    }
    
  protected:
    int currMode;
    int defMode;
    int threshMode;    
  };

  extern Debug debug;
};

#endif
