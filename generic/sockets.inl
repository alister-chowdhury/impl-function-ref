
//
// NB: Not tested really
//


// sockets.h

#include <cstdint>
#include <string>

#define SOCKET_BAD_HOST -1
#define SOCKET_COULDNT_CREATE_SOCKET -2
#define SOCKET_COULDNT_CONNECT -3


int socket_open(const char* host, const uint16_t port) noexcept;
void socket_close(int sock) noexcept;
int socket_write(int sock, const char *buffer, const int length) noexcept;
int socket_read(int sock, char* out, const int max_size) noexcept;

std::string socket_readline(int sock) noexcept;


// sockets.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>



#if __has_include(<sys/socket.h>)
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #define closesocket close

#elif __has_include(<winsock.h>)
    #include <winsock.h>

    // Windows needs to call WSAStartup before anything else
    // so we're just making a hidden thing that'll just do it
    // external to anything else.
    namespace {
    namespace detail {
    namespace windows {

    struct WindowsBootstrap {
        WindowsBootstrap()
        {
            WSADATA wsa_data;
            int ok = WSAStartup(MAKEWORD(2 ,2), &wsa_data);
            if(ok != 0) {
                std::fprintf(stderr, "Error calling WSAStartup: Error code = %i\n", ok);
                std::abort();
            }
        }
    };

    const static WindowsBootstrap windows_bootstrap;

    } // windows
    } // detail
    } // unnamed namespace


#else
    #error "Unsupported platform"
#endif


int socket_open(const char* host, const uint16_t port) noexcept {

    hostent *remote_host = gethostbyname(host);
    if(remote_host == nullptr || remote_host->h_addrtype == INADDR_NONE) {
        return SOCKET_BAD_HOST;
    }

    int sock = socket(remote_host->h_addrtype, SOCK_STREAM, IPPROTO_TCP);
    if(sock < 0) {
        return SOCKET_COULDNT_CREATE_SOCKET;
    }

    sockaddr_in socket_desc;
    socket_desc.sin_family = remote_host->h_addrtype;
    std::memcpy(&socket_desc.sin_addr, remote_host->h_addr_list[0], remote_host->h_length);
    socket_desc.sin_port = htons(port);

    if(connect(sock, (sockaddr*)&socket_desc, sizeof(socket_desc))) {
        closesocket(sock);
        return SOCKET_COULDNT_CONNECT;
    } 

    return sock;
}


void socket_close(int sock) noexcept {
    closesocket(sock);
}


int socket_write(int sock, const char *buffer, const int length) noexcept {

    int written = 0;

    while(written < length) {
        int written_block = send(sock, &buffer[written], length-written, 0);
        if(written_block < 1) {
            return false;
        }
        written += written_block;
    }

    return true;
}


int socket_read(int sock, char* out, const int max_size) noexcept {

    int read = 0;
    while(read < max_size) {
        int read_block = recv(sock, &out[read], max_size-read, 0);
        if(read_block < 1) {
            break;
        }
        read += read_block;
    }
    return read;

}


int socket_peek(int sock, char* out, const int max_size) noexcept {

    int read = recv(sock, &out[read], max_size-read, MSG_PEEK);
    if(read < 0) { read = 0; }
    return read;

}


std::string socket_readline(int sock) noexcept {

    const static int buffer_size = 1024;

    std::string result;

    char buffer[buffer_size];
    char* buffer_start = (char*)&buffer;

    // Keep reading until we either hit a newline, or until we cannot read anymore.
    while(int read = socket_peek(sock, buffer_start, buffer_size)) {

        char* buffer_end = buffer_start + read;
        char* newline_addr = std::find(buffer_start, buffer_end, '\n');

        // No newline found
        if(newline_addr == buffer_end) {
            socket_read(sock, buffer_start, read);
            result.append(buffer_start, buffer_end);
        }

        // Newline found
        else {

            // publically consume the newline but nothing beyond that
            socket_read(sock, buffer_start, (newline_addr - buffer_start) + 1);

            // Strip off \r if it preceeds \n
            if(buffer_start != newline_addr) {
                if(newline_addr[-1] == '\r') {
                    --newline_addr;
                }
            }
    
            // Make sure we haven't already read '\r' into our result
            else if(!result.empty() && result.back() == '\r') {
                result.pop_back();
            }

            result.append(buffer_start, newline_addr);
            break;
        }

    }

    return result;
}

