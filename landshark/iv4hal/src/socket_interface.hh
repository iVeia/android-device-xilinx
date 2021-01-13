#ifndef __SOCKET_INTERFACE_HH
#define __SOCKET_INTERFACE_HH

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
#include "message.hh"

#include "iv4hal.hh"

namespace iv4 {

class SocketInterface {
    public:
    typedef std::function< void (const Message &message) > IV4MessageCallback;

    SocketInterface(IV4MessageCallback callback, bool server = false, 
                    const std::string &name = IV4HAL_DEFAULT_SOCK_NAME);

  bool Process(fd_set *rset = nullptr);

  void ProcessData(uint8_t *data, int dataLen);
  
  bool Send(const Message &m);
  bool Send(const Message::Header &hdr, uint8_t *data, int dataLen);

  void Stop();

  ~SocketInterface();

  bool IsOpen() const;

  bool Listening() const;
  
  bool ClientConnected() const;
  void CloseConnection();

  inline int ReadySet(fd_set *rset) {
    if(clientSocket >= 0) {
      // There is an active client, so check to see if it has data to read
      FD_SET(clientSocket, rset);
      return clientSocket;
    } else if(serverSocket >= 0) {
      // Check to see if there is an incoming connection
      FD_SET(serverSocket, rset);
      return serverSocket;
    } else return -1;
  }
  

protected:
  bool               server;          // Is this instance a server
  int                serverSocket;    // Socket for listening server
  int                clientSocket;    // Socket for client communication 
  IV4MessageCallback callback;        // Function to call when a message is received
  
private:
  enum class messageState {
    WaitingSync,
    ReadingHeader,
    ReadingPayload,
  };
  messageState state;
  
  // Where are we in looking for the sync bytes
  uint32_t syncAt;
  
  // TODO: maybe need to change this
  constexpr static int rdbufLen = 1024*1024;
  uint8_t              rdbuf[rdbufLen]; // Read up to 1M at a time from the socket
  
  uint16_t             hbufPos;   // Where we are in trying to read a full header
  uint8_t              hbuf[128]; // large enough to accomadate a header message
  
  Message message;    // Message we construct as we are reading from the socket
};
};

#endif
