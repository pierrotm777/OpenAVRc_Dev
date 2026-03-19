#pragma once
#include <wx/string.h>

// RAW TCP transport with Tserial-like API (add-only)
//
// Linux prerequisites (Debian/Ubuntu):
//   sudo apt update
//   sudo apt install build-essential g++ libwxgtk3.2-dev
// (Depending on distro, package name may be libwxgtk3.0-gtk3-dev)
//
class TcpPort {
public:
  TcpPort();
  ~TcpPort();

  bool connect(const wxString& host, int port);
  void disconnect();
  bool isConnected() const;

  void flush();
  int  getNbrOfBytes();
  int  getArray(char* buf, int len);
  int  sendArray(const char* buf, int len);

private:
  int sock;
#ifdef _WIN32
  bool wsaInit;
#endif
};
