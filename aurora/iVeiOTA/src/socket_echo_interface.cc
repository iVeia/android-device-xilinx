
#include <string.h>
#include <errno.h>

#include "socket_echo_interface.hh"
#include "debug.hh"
namespace iVeiOTA {

  SocketEchoInterface::SocketEchoInterface(EchoCallback callback, bool server, const std::string &name) :
        server(server), callback(callback) {
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

    bool SocketEchoInterface::Process() {
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
        }
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
              if(callback != nullptr) callback(rdbuf, bread);
            }
        } else {
        }

        return true;
    }

  bool SocketEchoInterface::Send(uint8_t *dat, int len) {
        if(clientSocket < 0) {
            return false;
        }

        int wroteP = write(clientSocket, dat, len);
        return (wroteP == len);
    }

    void SocketEchoInterface::Stop() {
        if(clientSocket >= 0) {
            close(clientSocket);
            clientSocket = -1;
        }
        if(serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
    }

    SocketEchoInterface::~SocketEchoInterface() {
        Stop();
    }

    bool SocketEchoInterface::IsOpen() const {
        return Listening() || ClientConnected();
    }

    bool SocketEchoInterface::Listening() const {
        return serverSocket >= 0;
    }

    bool SocketEchoInterface::ClientConnected() const {
        return clientSocket >= 0;
    }
};
