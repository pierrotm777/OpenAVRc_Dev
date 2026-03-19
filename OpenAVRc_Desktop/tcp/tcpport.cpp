#include "tcpport.h"

/*
  Linux prerequisites (Debian/Ubuntu):
    sudo apt update
    sudo apt install build-essential libwxgtk3.2-dev
*/

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <sys/ioctl.h>
#endif

#include <string.h>

TcpPort::TcpPort()
#ifdef _WIN32
  : sock((unsigned long)INVALID_SOCKET), wsaInit(false)
#else
  : sock(-1)
#endif
{
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2,2), &wsa) == 0) {
    wsaInit = true;
  }
#endif
}

TcpPort::~TcpPort() {
  disconnect();
#ifdef _WIN32
  if (wsaInit) WSACleanup();
#endif
}

bool TcpPort::connect(const wxString& host, int port) {
  disconnect();

#ifdef _WIN32
  SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short)port);

  const char* h = host.mb_str().data();

  // 1) Try dotted IPv4
  unsigned long ip = inet_addr(h);
  if (ip != INADDR_NONE) {
    addr.sin_addr.s_addr = ip;
  } else {
    // 2) Fallback DNS via gethostbyname (MinGW-friendly)
    hostent* he = gethostbyname(h);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
      closesocket(s);
      return false;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
  }

  int r = ::connect(s, (sockaddr*)&addr, sizeof(addr));
  if (r != 0) {
    closesocket(s);
    return false;
  }

  // non-blocking
  u_long mode = 1;
  ioctlsocket(s, FIONBIO, &mode);

  sock = (unsigned long)s;
  return true;

#else
  int s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return false;

  // Resolve host (IPv4) with getaddrinfo
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo* res = nullptr;
  char portStr[16];
  snprintf(portStr, sizeof(portStr), "%d", port);

  int gai = getaddrinfo(host.mb_str().data(), portStr, &hints, &res);
  if (gai != 0 || !res) {
    close(s);
    return false;
  }

  int ok = (::connect(s, res->ai_addr, (socklen_t)res->ai_addrlen) == 0);
  freeaddrinfo(res);

  if (!ok) {
    close(s);
    return false;
  }

  // non-blocking
  int flags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, flags | O_NONBLOCK);

  sock = s;
  return true;
#endif
}

void TcpPort::disconnect() {
#ifdef _WIN32
  SOCKET s = (SOCKET)sock;
  if (s != INVALID_SOCKET) {
    closesocket(s);
    sock = (unsigned long)INVALID_SOCKET;
  }
#else
  if (sock >= 0) {
    close(sock);
    sock = -1;
  }
#endif
}

bool TcpPort::isConnected() const {
#ifdef _WIN32
  return ((SOCKET)sock) != INVALID_SOCKET;
#else
  return sock >= 0;
#endif
}

void TcpPort::flush() {
  char tmp[256];
  while (getArray(tmp, (int)sizeof(tmp)) > 0) {}
}

int TcpPort::getNbrOfBytes() {
#ifdef _WIN32
  SOCKET s = (SOCKET)sock;
  if (s == INVALID_SOCKET) return 0;
  u_long bytes = 0;
  ioctlsocket(s, FIONREAD, &bytes);
  return (int)bytes;
#else
  if (sock < 0) return 0;
  int bytes = 0;
  ioctl(sock, FIONREAD, &bytes);
  return bytes;
#endif
}

int TcpPort::getArray(char* buf, int len) {
#ifdef _WIN32
  SOCKET s = (SOCKET)sock;
  if (s == INVALID_SOCKET) return 0;
  int r = recv(s, buf, len, 0);
  if (r == SOCKET_ERROR) return 0;
  return r;
#else
  if (sock < 0) return 0;
  int r = (int)recv(sock, buf, (size_t)len, 0);
  if (r <= 0) return 0;
  return r;
#endif
}

int TcpPort::sendArray(const char* buf, int len) {
#ifdef _WIN32
  SOCKET s = (SOCKET)sock;
  if (s == INVALID_SOCKET) return 0;
  int r = send(s, buf, len, 0);
  if (r == SOCKET_ERROR) return 0;
  return r;
#else
  if (sock < 0) return 0;
  int r = (int)send(sock, buf, (size_t)len, 0);
  if (r <= 0) return 0;
  return r;
#endif
}
