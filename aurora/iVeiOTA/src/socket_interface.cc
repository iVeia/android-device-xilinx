
#include <string.h>
#include <errno.h>

#include "socket_interface.hh"
#include "debug.hh"
namespace iVeiOTA {

  SocketInterface::SocketInterface(OTAMessageCallback callback, bool server, const std::string &name) :
        server(server), callback(callback), state(messageState::WaitingSync), syncAt(0), hbufPos(0) {
        clientSocket = -2;
        serverSocket = -2;
        int tempSocket = -2;
        struct sockaddr_un server_address;
        socklen_t address_length = offsetof(struct sockaddr_un, sun_path) + strlen(name.c_str());

        if ((tempSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            // Failed to create the server socket
            serverSocket = -1;
            return;
        }

        memset(&server_address, 0, sizeof(server_address));
        server_address.sun_family = AF_UNIX;
        // sun_path should not be null terminated, and the first character should be \0 to use 
        //  the abstract namespace for the socket
        strcpy(server_address.sun_path, name.c_str()); 
        server_address.sun_path[0] = '\0';

        if(server) {
            serverSocket = tempSocket;

            // Bind to the socket so we can accept connections
            if (bind(serverSocket, (const struct sockaddr *) &server_address, address_length) < 0) {
                // Failed to bind the socket
              debug << Debug::Mode::Failure << "Failed to bind socket: " << strerror(errno) << std::endl << Debug::Mode::Info;
                close(serverSocket);
                serverSocket = -1;
                return;
            }

            // Then start listening for incoming connections
            if(listen(serverSocket, 2) < 0) {
                // Failed to listen on socket
              debug << Debug::Mode::Failure << "Failed to listen on socket: " << strerror(errno) << std::endl << Debug::Mode::Info;
                close(serverSocket);
                serverSocket = -1;
                return;
            }
        } else {
            // Client code
            clientSocket = tempSocket;
            if (connect(clientSocket, (struct sockaddr*)&server_address, address_length) < 0) {
                clientSocket = -1;
                return;
            }
        }
        return;
    }

    bool SocketInterface::Process() {
        fd_set rset;
        int maxfd = 0;

        if(clientSocket < 0 && serverSocket < 0) {
            return false;
        }

        // Set up our sockets for the select call so that we dont block
        FD_ZERO(&rset);
        if(clientSocket >= 0) {
            // There is an active client, so check to see if it has data to read
            FD_SET(clientSocket, &rset);
            maxfd = clientSocket + 1;
        } else if(serverSocket >= 0) {
            // Check to see if there is an incoming connection
            FD_SET(serverSocket, &rset);
            maxfd = serverSocket + 1;
        } else {
          // We have no valid sockets
          return false;
        }

        // TODO: Make this number configurable?  Decide on the best value here
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if(select(maxfd, &rset, NULL, NULL, &timeout) < 0) {
            return false;
        }

        if(serverSocket >= 0 && FD_ISSET(serverSocket, &rset)) {
            // Something to accept()
            struct sockaddr_un client_addr;
            socklen_t client_addr_size = sizeof(client_addr);
            clientSocket = accept(serverSocket, (struct sockaddr*) &client_addr, &client_addr_size);
            state = messageState::WaitingSync;
            syncAt = 0;
            if(clientSocket < 0) {
                //TODO: This probably means we have to quit/restart the OTA application
                clientSocket = -2;
            }
        } else if(clientSocket >= 0 && FD_ISSET(clientSocket, &rset)) {
            // there is data to read
            int bread = read(clientSocket, rdbuf, rdbufLen);
            if(bread < 0) {
                // error, socket is probably closed
                close(clientSocket);
                clientSocket = -2;
            } else if(bread == 0) {
                // Happens when the socket is closed
                close(clientSocket);
                clientSocket = -2;
            } else {
                ProcessData(rdbuf, bread);
            }
        } else {
        }

        return true;
    }

    void SocketInterface::ProcessData(uint8_t *data, int dataLen) {
        int processed = 0;
        while(processed < dataLen) {
            int dRemaining = dataLen - processed;

            switch(state) {
            case messageState::WaitingSync:
            {
                if(Message::sync[syncAt] == data[processed]) syncAt++;
                else syncAt = 0;
                processed++;

                if(syncAt == Message::SyncLength) {
                    state = messageState::ReadingHeader;
                    syncAt = 0;
                    hbufPos = 0;
                }
                break;
            }
            case messageState::ReadingHeader:
            {
                // Take 8 off of remaining to account for the sync we already read
                int hRemaining = Message::Header::Size(Message::DefaultRev) - hbufPos - 8;
                int toCopy = std::min(hRemaining, dRemaining);

                debug << "Reading header: " << hRemaining << ":" << toCopy << std::endl;

                memcpy(hbuf + hbufPos, data + processed, toCopy);
                hbufPos += toCopy;
                processed += toCopy;

                if(hbufPos >= Message::Header::Size(Message::DefaultRev) - 8) {
                    try {
                        message = Message(hbuf, false);
                    } catch(...) {
                        // The header was invalid, so restart
                        hbufPos = 0;
                        state = messageState::WaitingSync;
                        break;
                    }
                    debug << "Got header: " << message.header.pLen << ":" << std::endl;
                    hbufPos = 0;
                    state = messageState::ReadingPayload;
                    if(message.header.pLen > 0) break;
                    // else we want to fall through to process the 0-length payload
                    //  if we don't we will end up waiting until there is data available on the socket
                } else {
                    break;
                }
            }
            case messageState::ReadingPayload:
            {
                int pRemaining = message.header.pLen - message.payload.size();
                int toCopy = std::min(pRemaining, dRemaining);
                debug << "Reading payload" << pRemaining << ":" << toCopy << std::endl;
                for(int i = 0; i < toCopy; i++) message.payload.push_back(data[processed++]);

                if(message.payload.size() == message.header.pLen) {
                    // We have the full payload, so we can process the message
                    callback(message);
                    message = Message();
                    state = messageState::WaitingSync;
                }
                break;
            }

            } // End switch(state)
        }
    }

    bool SocketInterface::Send(const Message &m) {
        if(clientSocket < 0) {
            return false;
        }

        // Get the header as an array of bytes and its length
        auto buf = m.header.ToByteArray();
        int len = m.header.Size();

        // Sanity check
        if(buf == nullptr || len <= 0) return false;

        // If the other side has closed the socket on us these writes will
        //  generate a SIGPIPE - broken pipe signal.
        // The server handles that and will call CloseConnection so we don't
        //  do anything about that here
        
        // Write the header data to the socket.  buf is a unique_ptr
        int wrote = write(clientSocket, buf.get(), len);

        // Then we have to write the payload data
        int pLen = m.payload.size();
        int wroteP = write(clientSocket, m.payload.data(), pLen);

        return (wrote == len && wroteP == pLen);
    }

    void SocketInterface::Stop() {
        if(clientSocket >= 0) {
            close(clientSocket);
            clientSocket = -1;
        }
        if(serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
    }

    SocketInterface::~SocketInterface() {
        Stop();
    }

    bool SocketInterface::IsOpen() const {
        return Listening() || ClientConnected();
    }

    bool SocketInterface::Listening() const {
        return serverSocket >= 0;
    }

    bool SocketInterface::ClientConnected() const {
        return clientSocket >= 0;
    }

  void SocketInterface::CloseConnection() {
    if(clientSocket >= 0) {
      close(clientSocket);
      clientSocket = -1;
    }
  }
};
