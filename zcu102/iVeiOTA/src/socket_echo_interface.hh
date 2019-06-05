#ifndef __SOCKET_ECHO_INTERFACE_HH
#define __SOCKET_ECHO_INTERFACE_HH

#include <string>
#include <cstdlib>
#include <functional>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/select.h>
#include <stdio.h>
#include <algorithm>

#include "socket_interface.hh"
#include "iveiota.hh"

namespace iVeiOTA {

  class SocketEchoInterface {
  public:
    typedef std::function< void (uint8_t *data, int len) > EchoCallback;
    
    SocketEchoInterface(EchoCallback callback, bool server = false, 
                    const std::string &name = IVEIOTA_DEFAULT_SOCK_NAME);
    
    bool Process();
    
    bool Send(uint8_t *dat, int len);
    
    void Stop();
    
    ~SocketEchoInterface();
    
    bool IsOpen() const;
    
    bool Listening() const;
    
    bool ClientConnected() const;
  
protected:
  bool               server;          // Is this instance a server
  int                serverSocket;    // Socket for listening server
  int                clientSocket;    // Socket for client communication 
  EchoCallback callback;        // Function to call when a message is received
  
private:
  // TODO: maybe need to change this
  constexpr static int rdbufLen = 1024*1024;
  uint8_t              rdbuf[rdbufLen]; // Read up to 1M at a time from the socket  
};
};

#endif
