/*
**************************************************************************
*                                                                        *
*                 ____                ___ _   _____                      *
*                / __ \___  ___ ___  / _ | | / / _ \____                 *
*               / /_/ / _ \/ -_) _ \/ __ | |/ / , _/ __/                 *
*               \____/ .__/\__/_//_/_/ |_|___/_/|_|\__/                  *
*                   /_/                                                  *
*                                                                        *
*              This file is part of the OpenAVRc project.                *
*                                                                        *
*                         Based on code(s) named :                       *
*             OpenTx - https://github.com/opentx/opentx                  *
*             Deviation - https://www.deviationtx.com/                   *
*                                                                        *
*                Only AVR code here for visibility ;-)                   *
*                                                                        *
*   OpenAVRc is free software: you can redistribute it and/or modify     *
*   it under the terms of the GNU General Public License as published by *
*   the Free Software Foundation, either version 2 of the License, or    *
*   (at your option) any later version.                                  *
*                                                                        *
*   OpenAVRc is distributed in the hope that it will be useful,          *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*   GNU General Public License for more details.                         *
*                                                                        *
*       License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html          *
*                                                                        *
**************************************************************************
*/

#include <wx/msgdlg.h>
#include <wx/frame.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <cstdarg>
#include <cstdio>

#include "BluetoothFrame.h"
#include "OpenAVRc_DesktopMain.h"
#include "../OpenAVRc/thirdparty/xmodem/xmodem.cpp"
#include "tcp/tcpport.h"
#define SD_ROOT ("/")

#define START_TIMOUT() \
 timout = true;        \
 TimerRX.StartOnce(350);

#define IS_SD_DIR(x)       \
 ((x.StartsWith("[")) && (x.EndsWith("]"))) // x is a wxString

#define IS_SD_ROOT(x)       \
 (x == SD_ROOT) // x is a wxString

Tserial *BTComPort;

static wxFrame* XmdmLogFrame = nullptr;
static wxTextCtrl* XmdmLogCtrl = nullptr;

static void EnsureXmdmLogWindow(wxWindow* parent)
{
  if (XmdmLogFrame && XmdmLogCtrl) return;

  XmdmLogFrame = new wxFrame(parent, wxID_ANY, "XMODEM Log",
                             wxDefaultPosition, wxSize(700, 250),
                             wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT);

  XmdmLogCtrl = new wxTextCtrl(XmdmLogFrame, wxID_ANY, "",
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

  wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(XmdmLogCtrl, 1, wxEXPAND | wxALL, 5);
  XmdmLogFrame->SetSizer(sizer);
  XmdmLogFrame->Layout();
  XmdmLogFrame->Show();
}

void XmdmLogClear()
{
  if (XmdmLogCtrl) XmdmLogCtrl->Clear();
}

void XmdmLog(const char* fmt, ...)
{
  if (!XmdmLogCtrl) return;

  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  XmdmLogCtrl->AppendText(wxString::FromUTF8(buf));
  XmdmLogCtrl->AppendText("\n");
}

static void XmdmLogBufferHex(const char* tag, const char* buffer, int len)
{
  if (!buffer || len <= 0)
    {
      XmdmLog("%s <empty>", tag);
      return;
    }

  char line[512];
  int pos = snprintf(line, sizeof(line), "%s", tag);
  for (int i = 0; i < len && pos < (int)sizeof(line) - 4; ++i)
    {
      pos += snprintf(line + pos, sizeof(line) - pos, " %02X", (unsigned char)buffer[i]);
    }
  XmdmLog("%s", line);
}

// Optional RAW TCP transport (add-only, does not change serial behavior)
TcpPort *BTNetPort = nullptr;
bool UseTcpPort = false;

// Transport-neutral helpers (COM or TCP). Used by UI and by XMODEM macros.
static inline void BT_Flush() {
  if (UseTcpPort && BTNetPort) BTNetPort->flush();
  else BTComPort->flush();
}
static inline int BT_GetNbrOfBytes() {
  if (UseTcpPort && BTNetPort) return BTNetPort->getNbrOfBytes();
  return BTComPort->getNbrOfBytes();
}
static inline int BT_GetArray(char* buf, int len) {
  if (UseTcpPort && BTNetPort) return BTNetPort->getArray(buf, len);
  return BTComPort->getArray(buf, len);
}
static inline int BT_SendArray(const char* buf, int len) {
  if (UseTcpPort && BTNetPort) return BTNetPort->sendArray(buf, len);
  // Tserial::sendArray returns void
  BTComPort->sendArray((char*)buf, len);
  return len;
}

// --- Exported functions required by BluetoothFrame.h XMODEM macros ---
int BT_AnyAvailable() {
  return BT_GetNbrOfBytes();
}
int BT_AnyReadByte() {
  char b = 0;
  int r = BT_GetArray(&b, 1);
  if (r <= 0) return -1;
  return (unsigned char)b;
}
int BT_AnyWrite(const char* buf, int len) {
  return BT_SendArray(buf, len);
}
void BT_AnyFlushRx() {
  BT_Flush();
}

static bool IsTcpSpec(const wxString& s) {
  return s.Upper().StartsWith("TCP:");
}
static bool ParseTcpSpec(const wxString& s, wxString& host, int& port) {
  // Accept formats:
  //   TCP:192.168.0.31:3333
  //   TCP:192.168.0.31
  wxString t = s;
  if (!t.Upper().StartsWith("TCP:")) return false;
  t = t.Mid(4); // after TCP:
  port = 3333;

  int colon = t.Find(':');
  if (colon == wxNOT_FOUND) {
    host = t;
    return host.Length() > 0;
  }

  host = t.Left(colon);
  wxString pstr = t.Mid(colon + 1);
  long p;
  if (pstr.ToLong(&p) && p > 0 && p < 65536) port = (int)p;
  return host.Length() > 0;
}

// Send FT:STA on control port 3334 (best effort, add-only)
// This allows Desktop to start ESP32 FT automatically before using DATA port 3333.
static void SendFtCtrl(const wxString& host, const char* cmd)
{
  if (host.IsEmpty() || cmd == nullptr || *cmd == '\0') return;

  TcpPort ctrl;
  if (ctrl.connect(host, 3334)) {
    ctrl.sendArray(cmd, (int)strlen(cmd));
    ctrl.disconnect();
  }
}

// Remember last TCP endpoint so we can send control command on exit
static wxString gLastTcpHost;
static int gLastTcpDataPort = 3333;


// Scan LAN for ESP32 FT CTRL servers (port 3334) and append TCP entries.
// Add-only: does not change COM behavior.
static void ScanTcpDevices(wxComboBox* combo)
{
  const int PORT_CTRL = 3334;
  // DHCP range (Freebox): 192.168.0.10 .. 192.168.0.50
  for (int i = 10; i <= 50; i++) {
    wxString ip = wxString::Format("192.168.0.%d", i);
    TcpPort test;
    if (test.connect(ip, PORT_CTRL)) {
      // Optionally ask status (best effort)
      const char* cmd = "FT:STATUS\n";
      test.sendArray(cmd, (int)strlen(cmd));
      test.disconnect();

      wxString entry = wxString::Format("TCP:%s:3333", ip);
      if (combo->FindString(entry) == wxNOT_FOUND) {
        combo->Append(entry);
      }
    }
  }
}



#if defined(USE_DDE_LINK)
// DDE
DdeServer * dynDdeServer = NULL;
DdeClient * dynDdeClient = NULL;
DdeConnectionOut * dynDdeConnectionOut = NULL;
DdeConnectionIn * dynDdeConnectionIn = NULL;
wxString hostName;
wxString DdeServerName;
wxString DdeExtServerName;
wxString DdeTopicName;
#endif

//popup menu ID
enum MenuIDs {POPUP_ID_DELETE = wxID_HIGHEST + 1, POPUP_ID_CREATE_REPERTORY};

extern wxString AppPath;
//(*InternalHeaders(BluetoothFrame)
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/string.h>
//*)

//(*IdInit(BluetoothFrame)
const long BluetoothFrame::ID_STATICBOX1 = wxNewId();
const long BluetoothFrame::ID_COMBOBOX1 = wxNewId();
const long BluetoothFrame::ID_STATICTEXT1 = wxNewId();
const long BluetoothFrame::ID_STATICTEXT2 = wxNewId();
const long BluetoothFrame::ID_STATICTEXT3 = wxNewId();
const long BluetoothFrame::ID_STATICTEXT4 = wxNewId();
const long BluetoothFrame::ID_REBOOTBUTTON = wxNewId();
const long BluetoothFrame::ID_STATICBOX2 = wxNewId();
const long BluetoothFrame::ID_STATICBOXSD = wxNewId();
const long BluetoothFrame::ID_TREECTRLSD = wxNewId();
const long BluetoothFrame::ID_GENERICDIRCTRL1 = wxNewId();
const long BluetoothFrame::ID_BITMAPBUTTONREFRESH = wxNewId();
const long BluetoothFrame::ID_GAUGE = wxNewId();
const long BluetoothFrame::ID_PANEL1 = wxNewId();
const long BluetoothFrame::ID_TIMERRX = wxNewId();
//*)

BEGIN_EVENT_TABLE(BluetoothFrame,wxFrame)
//(*EventTable(BluetoothFrame)
//*)
END_EVENT_TABLE()

BluetoothFrame::BluetoothFrame(wxWindow* parent,wxWindowID id,const wxPoint& pos,const wxSize& size)
{
//(*Initialize(BluetoothFrame)
Create(parent, wxID_ANY, _("Bluetooth"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE, _T("wxID_ANY"));
SetClientSize(wxSize(645,409));
Panel1 = new wxPanel(this, ID_PANEL1, wxPoint(392,176), wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL1"));
StaticBoxCom = new wxStaticBox(Panel1, ID_STATICBOX1, _("Communication"), wxPoint(8,8), wxSize(624,88), 0, _T("ID_STATICBOX1"));
ComboBoxCom = new wxComboBox(Panel1, ID_COMBOBOX1, wxEmptyString, wxPoint(64,32), wxSize(160,23), 0, 0, 0, wxDefaultValidator, _T("ID_COMBOBOX1"));
StaticText1 = new wxStaticText(Panel1, ID_STATICTEXT1, _("Port :"), wxPoint(16,32), wxSize(40,16), wxALIGN_RIGHT, _T("ID_STATICTEXT1"));
StaticText2 = new wxStaticText(Panel1, ID_STATICTEXT2, _("Mémoire libre :"), wxPoint(328,48), wxSize(96,16), wxALIGN_RIGHT, _T("ID_STATICTEXT2"));
StaticTextFreeMem = new wxStaticText(Panel1, ID_STATICTEXT3, _("------"), wxPoint(432,48), wxSize(56,16), wxALIGN_LEFT, _T("ID_STATICTEXT3"));
StaticTextVersion = new wxStaticText(Panel1, ID_STATICTEXT4, wxEmptyString, wxPoint(24,64), wxSize(360,16), 0, _T("ID_STATICTEXT4"));
BitmapButtonReboot = new wxBitmapButton(Panel1, ID_REBOOTBUTTON, wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_WARNING")),wxART_BUTTON), wxPoint(568,24), wxSize(48,24), wxBU_AUTODRAW, wxDefaultValidator, _T("ID_REBOOTBUTTON"));
BitmapButtonReboot->SetToolTip(_("Redémarrer"));
StaticBoxLocal1 = new wxStaticBox(Panel1, ID_STATICBOX2, _("Local"), wxPoint(8,104), wxSize(216,296), 0, _T("ID_STATICBOX2"));
StaticBoxSD = new wxStaticBox(Panel1, ID_STATICBOXSD, _("Carte SD"), wxPoint(232,104), wxSize(216,296), 0, _T("ID_STATICBOXSD"));
TctrlSd = new wxTreeCtrl(Panel1, ID_TREECTRLSD, wxPoint(240,120), wxSize(200,272), wxTR_DEFAULT_STYLE, wxDefaultValidator, _T("ID_TREECTRLSD"));
DirCtrl = new wxGenericDirCtrl(Panel1, ID_GENERICDIRCTRL1, wxEmptyString, wxPoint(16,120), wxSize(200,272), 0, wxEmptyString, 0, _T("ID_GENERICDIRCTRL1"));
BitmapButtonRefresh = new wxBitmapButton(Panel1, ID_BITMAPBUTTONREFRESH, wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_REDO")),wxART_BUTTON), wxPoint(568,56), wxSize(48,24), wxBU_AUTODRAW, wxDefaultValidator, _T("ID_BITMAPBUTTONREFRESH"));
BitmapButtonRefresh->SetToolTip(_("Rafraichir"));
Gauge = new wxGauge(Panel1, ID_GAUGE, 100, wxPoint(328,24), wxSize(224,16), 0, wxDefaultValidator, _T("ID_GAUGE"));
TimerRX.SetOwner(this, ID_TIMERRX);
TimerRX.Start(200, true);

Connect(ID_COMBOBOX1,wxEVT_COMMAND_COMBOBOX_SELECTED,(wxObjectEventFunction)&BluetoothFrame::OnComboBoxComSelected);
Connect(ID_COMBOBOX1,wxEVT_COMMAND_COMBOBOX_DROPDOWN,(wxObjectEventFunction)&BluetoothFrame::OnComboBoxComDropdown);
Connect(ID_REBOOTBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&BluetoothFrame::OnBitmapButtonRebootClick);
Connect(ID_TREECTRLSD,wxEVT_COMMAND_TREE_BEGIN_DRAG,(wxObjectEventFunction)&BluetoothFrame::OnTctrlSdBeginDrag);
Connect(ID_TREECTRLSD,wxEVT_COMMAND_TREE_ITEM_RIGHT_CLICK,(wxObjectEventFunction)&BluetoothFrame::OnTctrlSdItemRightClick);
Connect(ID_BITMAPBUTTONREFRESH,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&BluetoothFrame::OnBitmapButtonRefreshClick);
Connect(ID_TIMERRX,wxEVT_TIMER,(wxObjectEventFunction)&BluetoothFrame::OnTimerRXTrigger);
Connect(wxID_ANY,wxEVT_CLOSE_WINDOW,(wxObjectEventFunction)&BluetoothFrame::OnClose);
//*)

 {
  SetIcon(wxICON(oavrc_icon));
 }

 EnsureXmdmLogWindow(this);

 Connect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&BluetoothFrame::OnSdPopupChoice);
 DirCtrl->Connect(wxID_ANY, wxEVT_TREE_BEGIN_DRAG, wxTreeEventHandler(BluetoothFrame::OnDirCtrlBeginDrag), NULL, this);
 DirCtrl->SetPath(AppPath + "\\SD\\");
 BTComPort = new Tserial();
 comIsValid = false;
 uCLI = "uCLI>";

 DnD_TctrlSd_Txt * txtTctrlSdDropTarget = new DnD_TctrlSd_Txt(this);
 txtTctrlSdDropTarget->SetDataObject(new wxTextDataObject());
 TctrlSd->SetDropTarget(txtTctrlSdDropTarget); //Enable droping objects on SD

 DnD_DirCtrl_Txt * txtDirCtrlDropTarget = new DnD_DirCtrl_Txt(this);
 txtDirCtrlDropTarget->SetDataObject(new wxTextDataObject());
 DirCtrl->SetDropTarget(txtDirCtrlDropTarget); //Enable droping objects on HDD

#if defined(USE_DDE_LINK)
 // DDE exchange
 DdeLink();
#endif
}

BluetoothFrame::~BluetoothFrame()
{
//(*Destroy(BluetoothFrame)
//*)
}

void BluetoothFrame::OnClose(wxCloseEvent& event)
{
 // If using TCP FT bridge, ask ESP32 to leave FT mode on exit (best effort)
 if (UseTcpPort && !gLastTcpHost.IsEmpty()) {
  SendFtCtrl(gLastTcpHost, "FT:OFF\n");
  // Backward compatibility (older firmwares)
  SendFtCtrl(gLastTcpHost, "w off\n");
  // Compatibility: some firmwares may use a different stop command
  //SendFtCtrl(gLastTcpHost, "FT:STP\n"); // legacy stop (unused)
 }

 // Disconnect TCP transport if used
 if (BTNetPort) {
  BTNetPort->disconnect();
  delete BTNetPort;
  BTNetPort = NULL;
 }
 UseTcpPort = false;

 if (BTComPort != NULL) delete BTComPort;
 if (XmdmLogFrame) { XmdmLogFrame->Destroy(); XmdmLogFrame = NULL; XmdmLogCtrl = NULL; }
#if defined(USE_DDE_LINK)
 if (dynDdeConnectionOut != NULL) delete dynDdeConnectionOut;
 if (dynDdeConnectionIn != NULL) delete dynDdeConnectionIn;
 if (dynDdeClient != NULL) delete dynDdeClient;
 if (dynDdeServer != NULL) delete dynDdeServer;
#endif
 OpenAVRc_DesktopFrame *parent = wxDynamicCast(this->GetParent(), OpenAVRc_DesktopFrame);
 if(parent)
  parent->EnableBluetoothSelectedMenu();
 Destroy();
}

void BluetoothFrame::DetectSerial()
{
 TCHAR Devices [5000];
 for(int i=0; i<255; i++) // checking ports from COM0 to COM255
  {
   wxString str;
   str = str.Format(wxT("%i"),i);
   wxString ComName = "COM"+str; // converting to COM0, COM1, COM2
   long test = QueryDosDevice(ComName.c_str(), Devices, 5000); //Win32(64) API only
   if (test!=0) //QueryDosDevice returns zero if it didn't find an object
    {
     ComboBoxCom->Insert(ComName,0); // add to the ComboBox
    }
  }
  // Add RAW TCP option (ESP32 FT bridge)
  if (ComboBoxCom->FindString("TCP:192.168.0.31:3333") == wxNOT_FOUND)
    ComboBoxCom->Append("TCP:192.168.0.31:3333");
}

void BluetoothFrame::ConnectBTCom(wxString name)
{
 int error;
 char comMame[64];
 strncpy(comMame, (const char*)name.mb_str(wxConvUTF8), 63);
 comMame[63] = 0;
 assert(BTComPort);
 wxBusyCursor wait;
 if (IsTcpSpec(name)) {
  // RAW TCP mode: use ComboBox value like "TCP:192.168.1.37:3333"
  wxString host; int port;
  if (!ParseTcpSpec(name, host, port)) {
    error = -1;
  } else {
    SendFtCtrl(host, "FT:STA\n");

    gLastTcpHost = host;
    gLastTcpDataPort = port;

    if (!BTNetPort) BTNetPort = new TcpPort();
    UseTcpPort = BTNetPort->connect(host, port);
    error = UseTcpPort ? 0 : -1;
  }
} else {
  UseTcpPort = false;
  error = BTComPort->connect(comMame, 115200, spNONE);
}
if (error == 0)
  {
   comIsValid = true;
   Gauge->SetRange(100);
   Gauge->SetValue(0);
   getAndShowRam();
   Gauge->SetValue(10);
   getAndShowVer();
   Gauge->SetValue(20);
   Populate_SD();
  }
 else
  {
   wxString intString = wxString::Format(wxT("%i"), error);
   wxMessageBox("Erreur N"+ intString + " port COM");
  }
}

void BluetoothFrame::OnComboBoxComDropdown(wxCommandEvent& event)
{
  Gauge->Pulse();
  if (UseTcpPort && BTNetPort) { BTNetPort->disconnect(); UseTcpPort=false; } else { BTComPort->disconnect(); }
  ComboBoxCom->Clear();
  StaticTextFreeMem->SetLabel("------");
  StaticTextFreeMem->Update();
  StaticTextVersion->SetLabel("");
  StaticTextVersion->Update();
  TctrlSd->DeleteAllItems();

  DetectSerial();   // contient déjà Append("TCP:192.168.0.31:3333")

  event.Skip();
}



void BluetoothFrame::OnComboBoxComSelected(wxCommandEvent& event)
{
 ConnectBTCom(ComboBoxCom->GetValue());
}

void BluetoothFrame::OnTimerRXTrigger(wxTimerEvent& event)
{
 timout = false;
}

wxString BluetoothFrame::sendCmdAndWaitForResp(wxString BTcommand, wxString* BTanwser)
{
 if (comIsValid)
  {
   BT_Flush();  // flush buffer

   int16_t l = BTcommand.length();
   if (l != 0)
    {
     char cstring[40] = {0};
     strncpy(cstring, (const char*)BTcommand.mb_str(wxConvUTF8), l);
     XmdmLog("[FT CTRL] TX: %s", cstring);
     // Desktop uCLI over TCP: send CR only to avoid empty-command on LF
     if (UseTcpPort) {
       char CR = '\r';
       BT_SendArray(cstring, l); // Send uCli command
       BT_SendArray(&CR, 1);     // CR only
     }
     else {
       char CRLF[2] = {'\r','\n'};
       BT_SendArray(cstring, l); // Send uCli command
       BT_SendArray(CRLF, 2);    // CRLF for COM
     }
     wxBusyCursor wait;
     int Num = BT_GetNbrOfBytes();

     for( int i=0; i<10; ++i)
      {
       START_TIMOUT();
       do
        {
         wxYieldIfNeeded();
        }
       while (timout);
       int newNum = BT_GetNbrOfBytes();
       if (newNum > Num) Num = newNum;
       else break;
      }
     XmdmLog("[FT CTRL] RX bytes=%d", Num);
     if (Num)
      {
       char buffer[Num+1] = {0};
       BT_GetArray(buffer, Num);
       XmdmLogBufferHex("[FT CTRL] RX HEX:", buffer, Num);
       *BTanwser = (const char*)(buffer);
       XmdmLog("[FT CTRL] RX TXT: %s", (const char*)BTanwser->mb_str(wxConvUTF8));

       // Normal case: prompt is at the beginning: "uCLI>cmd:\r\nanswer..."
       if (BTanwser->StartsWith(uCLI))
        {
         BTcommand = BTanwser->BeforeFirst(wxUniChar('\r'));
         BTcommand = BTcommand.AfterFirst(wxUniChar('>'));
         *BTanwser = BTanwser->AfterFirst(wxUniChar('\n'));
         if (BTanwser->EndsWith(uCLI))
          {
           BTanwser->RemoveLast(uCLI.length());
          }
         while (BTanwser->EndsWith("\r") || BTanwser->EndsWith("\n"))
          {
           BTanwser->RemoveLast();
          }
         XmdmLog("[FT CTRL] parsed ret=%s", (const char*)BTcommand.mb_str(wxConvUTF8));
         XmdmLog("[FT CTRL] parsed answer=%s", (const char*)BTanwser->mb_str(wxConvUTF8));
         Sleep(200);
         return BTcommand;
        }

       // Some replies arrive as: "cmd:\r\nanswer\r\nuCLI>"
       if (BTanwser->EndsWith(uCLI))
        {
         wxString full = *BTanwser;
         BTcommand = full.BeforeFirst(wxUniChar('\r'));
         full = full.AfterFirst(wxUniChar('\n'));
         if (full.EndsWith(uCLI))
          {
           full.RemoveLast(uCLI.length());
          }
         while (full.EndsWith("\r") || full.EndsWith("\n"))
          {
           full.RemoveLast();
          }
         *BTanwser = full;
         XmdmLog("[FT CTRL] parsed ret=%s", (const char*)BTcommand.mb_str(wxConvUTF8));
         XmdmLog("[FT CTRL] parsed answer=%s", (const char*)BTanwser->mb_str(wxConvUTF8));
         Sleep(200);
         return BTcommand;
        }

       XmdmLog("[FT CTRL] RX invalid prompt (expected %s)", (const char*)uCLI.mb_str(wxConvUTF8));
       return "ERR";
      }
     XmdmLog("[FT CTRL] RX timeout/no data");
    }
  }
#if defined(USE_DDE_LINK)
  else if ((dynDdeConnectionIn != NULL) && (dynDdeConnectionOut != NULL))// use DDE
  {
    ddeResponce = ""; // flush
    dynDdeConnectionOut->Poke(DdeTopicName,BTcommand);

   for( int i=0; i<10; ++i)
      {
       START_TIMOUT();
       do
        {
         wxYieldIfNeeded();
        }
       while (timout);
       if (ddeResponce.Last() == '\n') break;
      }
     return ddeResponce;
  }
#endif
 return "ERR";
}

void BluetoothFrame::OnBitmapButtonRebootClick(wxCommandEvent& event)
{
 char Reboot[] = {'r','e','b','o','o','t','\r','\n'};
 BT_SendArray(Reboot, sizeof(Reboot)); // Send BTcommand
 OnComboBoxComDropdown(event);
}

void BluetoothFrame::OnBitmapButtonRefreshClick(wxCommandEvent& event)
{
 if (comIsValid)
  {
   wxBusyCursor wait;
   Gauge->SetRange(100);
   Gauge->SetValue(0);
   getAndShowRam();
   Gauge->SetValue(10);
   Populate_SD();
   Gauge->SetValue(100);
  }
}

wxString BluetoothFrame::getAndShowRam()
{
 wxString ram;
 sendCmdAndWaitForResp("ram", &ram);
 ram = ram.BeforeFirst('\r'); // remove all after \r (\n)
 StaticTextFreeMem->SetLabel(ram);
 StaticTextFreeMem->Update();
 return ram;
}

wxString BluetoothFrame::getAndShowVer()
{
 wxString ver;
 sendCmdAndWaitForResp("ver", &ver);
 ver = ver.BeforeFirst('\r'); // remove all after \r (\n)
 ver.Replace("\036"," ");
 ver.Replace("\037"," ");
 ver.Replace("\033"," ");
 StaticTextVersion->SetLabel(ver);
 StaticTextVersion->Update();
 return ver;
}

void BluetoothFrame::Populate_Dir(wxTreeItemId * dir)
{
 wxString sourceName = TctrlSd->GetItemText(*dir);
 if IS_SD_DIR(sourceName)
  {
   sourceName.Replace("[","/");
   sourceName.Replace("]","/");
  }
 wxString dirEnt = "";
 sendCmdAndWaitForResp("ls " + sourceName, &dirEnt); // [MODELS]\r\n[LOGS]\r\n[VOICE]\r\n
 dirEnt.Replace("\r\n", "\r", 1);
 wxString tmp = "";
 do
  {
   tmp = dirEnt.BeforeFirst('\r');
   dirEnt.BeforeFirst('\r',&dirEnt);
   TctrlSd->AppendItem(*dir, tmp);
  }
 while (dirEnt != "");
}

void BluetoothFrame::Populate_SD()
{
 TctrlSd->DeleteAllItems(); // first reset all
 wxTreeItemId rootId = TctrlSd->AddRoot(SD_ROOT);
 wxBusyCursor wait;
 Populate_Dir(&rootId);
 Gauge->SetValue(50);
 wxTreeItemIdValue cookie;
 wxTreeItemId child = TctrlSd->GetFirstChild(rootId, cookie);

 while (child.IsOk())
  {
   if IS_SD_DIR(TctrlSd->GetItemText(child))
    {
     Populate_Dir(&child);
    }
   child = TctrlSd->GetNextSibling(child);
  }
 Gauge->SetValue(100);
 TctrlSd->Expand(rootId);
}

wxString BluetoothFrame::GetFullPathTctrlItem(wxTreeItemId item)
{
 wxString path = "";
 wxTreeItemId root = TctrlSd->GetRootItem();
 if (item != root)
  {
   if (item.IsOk())
    {
     path = TctrlSd->GetItemText(item);
     if IS_SD_DIR(path)
      {
       path.Replace("[","");
       path.Replace("]","/");
      }
     wxTreeItemId tmp = item;
     wxString tmpPath;
     do
      {
       tmp = TctrlSd->GetItemParent(tmp);
       if (tmp.IsOk())
        {
         tmpPath = TctrlSd->GetItemText(tmp);
        }
       if IS_SD_DIR(tmpPath)
        {
         tmpPath.Replace("[","");
         tmpPath.Replace("]","/");
        }
       path = tmpPath + path;
      }
     while (tmp != root);
    }
   return path;
  }
 return SD_ROOT;
}

void BluetoothFrame::OnTctrlSdBeginDrag(wxTreeEvent& event) // SD drag
{
 wxTreeItemId item = event.GetItem();
 if (item.IsOk())
  {
   wxTextDataObject dragData(GetFullPathTctrlItem(item));
   if (dragData.GetTextLength())
    {
     dragSource.SetData(dragData);
     dragResult = dragSource.DoDragDrop(true);
    }
  }
}

void BluetoothFrame::OnDirCtrlBeginDrag(wxTreeEvent& event) // HDD Drag
{
 wxTreeItemId item = event.GetItem();
 if (item.IsOk())
  {
   DirCtrl->GetTreeCtrl()->SetFocusedItem(item);
   wxString path = DirCtrl->GetFilePath();
   if (path != "")  // Drag only files
    {
     wxTextDataObject dragData(path);
     if (dragData.GetTextLength())
      {
       dragSource.SetData(dragData);
       dragResult = dragSource.DoDragDrop(true);
      }
    }
  }
}

void BluetoothFrame::SdToSdCpy(wxString dest, wxString file)
{
 if (comIsValid)
  {
   wxBusyCursor wait;
   if IS_SD_DIR(dest)
    {
     dest.Replace("[","/");
     dest.Replace("]","/");
    }
   wxString uCliCommand = "cp SD" + file + " SD" + dest + file.AfterLast('/');
//wxMessageBox(uCliCommand);
   wxString BTanwser = "";
   sendCmdAndWaitForResp(uCliCommand, &BTanwser);
//wxMessageBox(BTanwser);
   Gauge->SetValue(0);
   Gauge->SetRange(100);
   Sleep(200);
   Populate_SD();
  }
}

void BluetoothFrame::HddToSdCpy(wxString dest, wxString file)
{
 if (comIsValid)
  {
   wxBusyCursor wait;
   if IS_SD_DIR(dest)
    {
     dest.Replace("[","/");
     dest.Replace("]","/");
    }
   wxString file2 = file.AfterLast('\\');
   file2.Replace(" ","_");
   //wxString uCliCommand = "cp xmdm SD" + dest + file2;
   wxString uCliCommand = "xrecv SD" + dest + file2;
//wxMessageBox(uCliCommand);
   wxString BTanwser = "";
   wxString retVal = "";
   XmdmLogClear();
   XmdmLog("[DESKTOP] cmd: %s", (const char*)uCliCommand.mb_str(wxConvUTF8));

   // Flush stale CLI bytes BEFORE sending xrecv, otherwise XSend may read old uCLI text
   // instead of the first XMODEM handshake byte.
   BT_Flush();

   // IMPORTANT: xrecv/xsend and XMODEM use the same data link.
   // Do NOT wait for a uCLI response here, otherwise we may consume handshake bytes
   // that must be read by XSend/XReceive.
   int16_t l = uCliCommand.length();
   if (l != 0)
    {
     char cstring[128] = {0};
     strncpy(cstring, (const char*)uCliCommand.mb_str(wxConvUTF8), sizeof(cstring)-1);
     XmdmLog("[FT CTRL] TX: %s", cstring);
     if (UseTcpPort)
      {
       char CR = '\r';
       BT_SendArray(cstring, l);
       BT_SendArray(&CR, 1);
      }
     else
      {
       char CRLF[2] = {'\r','\n'};
       BT_SendArray(cstring, l);
       BT_SendArray(CRLF, 2);
      }
    }

   XmdmLog("[DESKTOP] retVal: %s", (const char*)retVal.mb_str(wxConvUTF8));
   XmdmLog("[DESKTOP] answer: %s", (const char*)BTanwser.mb_str(wxConvUTF8));
   Set_BluetoothFrame_Gauge_Pointer(Gauge);
   wxCharBuffer srcPath = file.mb_str(wxConvFile);
   XmdmLog("[DESKTOP] starting XSend file=%s", srcPath.data());
   int ret = XSend(srcPath.data());
   XmdmLog("[DESKTOP] XSend ret=%d", ret);
   if (retVal == "-8") wxMessageBox(_("Le fichier existe dj"));
   if (ret) wxMessageBox(wxString::Format(wxT("%i"),ret));
   Gauge->SetValue(0);
   Gauge->SetRange(100);
   Sleep(200);
   Populate_SD();
  }
}

void BluetoothFrame::SDToHddCpy(wxString dest, wxString file)
{
 if (comIsValid)
  {
   wxBusyCursor wait;
   if IS_SD_DIR(file)
    {
     file.Replace("[","/");
     file.Replace("]","/");
    }
   //wxString uCliCommand = "cp SD" + file + " xmdm";
   wxString uCliCommand = "xsend SD" + file;
//wxMessageBox(uCliCommand);
   wxString BTanwser = "";
   wxString retVal = "";
   XmdmLogClear();
   XmdmLog("[DESKTOP] cmd: %s", (const char*)uCliCommand.mb_str(wxConvUTF8));

   // Flush stale CLI bytes BEFORE sending xsend, otherwise XReceive may read old uCLI text
   // instead of the first XMODEM handshake byte.
   BT_Flush();

   // IMPORTANT: xrecv/xsend and XMODEM use the same data link.
   // Do NOT wait for a uCLI response here, otherwise we may consume handshake bytes
   // that must be read by XSend/XReceive.
   int16_t l = uCliCommand.length();
   if (l != 0)
    {
     char cstring[128] = {0};
     strncpy(cstring, (const char*)uCliCommand.mb_str(wxConvUTF8), sizeof(cstring)-1);
     XmdmLog("[FT CTRL] TX: %s", cstring);
     if (UseTcpPort)
      {
       char CR = '\r';
       BT_SendArray(cstring, l);
       BT_SendArray(&CR, 1);
      }
     else
      {
       char CRLF[2] = {'\r','\n'};
       BT_SendArray(cstring, l);
       BT_SendArray(CRLF, 2);
      }
    }

   XmdmLog("[DESKTOP] retVal: %s", (const char*)retVal.mb_str(wxConvUTF8));
   XmdmLog("[DESKTOP] answer: %s", (const char*)BTanwser.mb_str(wxConvUTF8));
   if (wxDirExists(dest)) // this is a dir ?
    {
     dest += "\\";
    }
   else if (wxFileExists(dest)) // this this a file ?
    {
     dest = dest.BeforeLast('\\'); // remove the file name to keep the dir.
     dest += "\\";
    }
   else // this is a drive
    {
     dest += "\\";
    }
   dest += file.AfterLast('/');
   Set_BluetoothFrame_Gauge_Pointer(Gauge);
   wxCharBuffer dstPath = dest.mb_str(wxConvFile);
   XmdmLog("[DESKTOP] starting XReceive file=%s", dstPath.data());
   int ret = XReceive(dstPath.data());
   XmdmLog("[DESKTOP] XReceive ret=%d", ret);
//wxMessageBox(retVal);
   if (ret) wxMessageBox(wxString::Format(wxT("%i"),ret));
   Gauge->SetValue(0);
   Gauge->SetRange(100);
   DirCtrl->ReCreateTree();
  }
}

bool DnD_TctrlSd_Txt::OnDropText(wxCoord x, wxCoord y, const wxString& text) // SD Drop
{
 wxPoint point(x,y);
 int flag = wxTREE_HITTEST_ABOVE | wxTREE_HITTEST_ONITEMLABEL;
 wxTreeItemId dest = BluetoothFrame->TctrlSd->HitTest(point, flag);
 if (dest.IsOk())
  {
   wxString destName = BluetoothFrame->TctrlSd->GetItemText(dest);
   if (!(IS_SD_DIR(destName) || IS_SD_ROOT(destName)))
    {
     dest = BluetoothFrame->TctrlSd->GetItemParent(dest);
    }
   if (dest.IsOk())
    {
     wxString destName = BluetoothFrame->TctrlSd->GetItemText(dest);
     if (text.StartsWith(SD_ROOT)) // Sender is SD
      {
       BluetoothFrame->SdToSdCpy(destName, text);
      }
     else
      {
       BluetoothFrame->HddToSdCpy(destName, text);
      }
    }
   //BluetoothFrame->TctrlSd->AppendItem(dest, text);
   return true;
  }
 return false;
}

bool DnD_DirCtrl_Txt::OnDropText(wxCoord x, wxCoord y, const wxString& text) // HDD Drop
{
 wxPoint point(x,y);
 int flag = wxTREE_HITTEST_ABOVE | wxTREE_HITTEST_ONITEMLABEL;
 wxTreeItemId dest = BluetoothFrame->DirCtrl->GetTreeCtrl()->HitTest(point, flag);
 if (dest.IsOk())
  {
   wxString path = BluetoothFrame->DirCtrl->GetPath(dest);
   if (text.StartsWith(SD_ROOT)) // Verify sender is SD
    {
     BluetoothFrame->SDToHddCpy(path, text);
     //BluetoothFrame->DirCtrl->GetTreeCtrl()->AppendItem(dest, text);
    }
   return true;
  }
 return false;
}

void BluetoothFrame::OnTctrlSdItemRightClick(wxTreeEvent& event)
{
 wxTreeItemId item = event.GetItem();
 if (item.IsOk())
  {
   TctrlSd->SelectItem(item);
   wxString destName = TctrlSd->GetItemText(item);
   wxMenu pop;
   pop.Append(POPUP_ID_DELETE, _("Supprimer"));
   if (IS_SD_ROOT(destName))
    {
     pop.Append(POPUP_ID_CREATE_REPERTORY, _("Créer un répertoire")); // We don't support sub dir.
    }
   PopupMenu(&pop);
  }
}

void BluetoothFrame::SdDeleteFile(wxString file)
{
 if ((file != SD_ROOT) && (file != ""))
  {
   wxString BTanwser = "";
   //wxMessageBox(file);
   wxString retVal = sendCmdAndWaitForResp("rm SD" + file, &BTanwser);
   //wxMessageBox(BTanwser);
   //wxMessageBox(retVal);
   Sleep(200);
   Populate_SD();
  }
}

void BluetoothFrame::OnSdPopupChoice(wxCommandEvent& event)
{
 wxTreeItemId item = TctrlSd->GetSelection();
 if (item.IsOk())
  {
   wxString itemText = GetFullPathTctrlItem(item);
   switch (event.GetId())
    {
    case POPUP_ID_DELETE:
    {
     SdDeleteFile(itemText);
    }
    break;
    case POPUP_ID_CREATE_REPERTORY:
     wxMessageBox("md");
     break;
    }
  }
}

///////// XMODEM FILES OPERATIONS   /////////

ReusableBuffer ReBuff;

write_file(wxFile* fd, const uint8_t* buffer, int buffer_len)
{
 int ret = 0;

 ret = fd->Write(buffer, buffer_len);
 if (fd->Flush())
  {
   return ret;
  }
 return 0;
}

int seek_file(wxFile* fd, int32_t* offset, uint8_t whence)
{
 int ret = 0;

 if (fd->Seek(*offset,(wxSeekMode)whence) != wxInvalidOffset)
  {
   ret = 1;
  }
 return ret;
}

int read_file(wxFile* fd, uint8_t* buffer, uintptr_t buffer_len)
{
 return (int)fd->Read(buffer,buffer_len);
}

int FileExists(char * FullFileName)
{
 return wxFile::Exists(wxString::FromUTF8(FullFileName));
}

int delete_file(char * FullFileName)
{
 return wxRemoveFile(wxString::FromUTF8(FullFileName));
}

wxFile * FileOpenForWrite(char *FullFileName)
{
 wxFile *fd = NULL;
 fd = new(wxFile);
 if (fd->Create(wxString::FromUTF8(FullFileName), 0, wxS_DEFAULT))
  {
   return fd;
  }
 return NULL;
}

wxFile * FileOpenForRead(char *FullFileName)
{
 wxFile *fd = NULL;
 fd = new(wxFile);
 wxString path = (wxString::FromUTF8(FullFileName));
 if (wxFile::Exists(path))
  {
   (fd->Open(path, wxFile::read_write, wxS_DEFAULT));
   return fd;
  }
 return NULL;
}

#if defined(USE_DDE_LINK)
////// DDE ////////////////////////////

void BluetoothFrame::DdeLink()
{
 hostName = wxGetHostName();
 DdeServerName = "OPENAVRC1";
 DdeExtServerName = "OPENAVRC2";
 DdeTopicName = "BT";

 dynDdeServer = new DdeServer(this);
 dynDdeServer->Create(DdeServerName);
 DdeConnectTo(DdeExtServerName);
}

bool BluetoothFrame::DdeConnectTo(wxString ExtServerName)
{
 if (dynDdeConnectionOut != NULL) delete dynDdeConnectionOut;
 dynDdeConnectionOut = NULL;
 if (dynDdeClient != NULL) delete dynDdeClient;
 dynDdeClient = NULL;

 wxLogNull nolog;
 dynDdeClient = new DdeClient;
 dynDdeConnectionOut = (DdeConnectionOut *)dynDdeClient->MakeConnection(hostName, ExtServerName, DdeTopicName);
 if (dynDdeConnectionOut)
  {
   wxMessageBox("trouvé !", "Client serveur");
   dynDdeConnectionOut->Poke(DdeTopicName,"TOTO");
  }
 else
  {
   wxMessageBox("hoin ! !", "Client serveur");
   delete dynDdeConnectionOut;
   dynDdeConnectionOut = NULL;
   delete dynDdeClient;
   dynDdeClient = NULL;
  }
  return true;
}

wxConnectionBase * DdeServer::OnAcceptConnection(const wxString& topic)
{
if (topic == DdeTopicName)
{
wxLogNull nolog;
wxMessageBox("connection entrante");
dynDdeConnectionIn = new DdeConnectionIn(bluetoothFrame);
return dynDdeConnectionIn;
}
return NULL;
}

bool DdeConnectionIn::OnPoke(const wxString &topic, const wxString &item, const void *data, size_t size, wxIPCFormat format)
{
wxLogNull nolog;
 //wxMessageBox("poke ok", topic);
 if (dynDdeConnectionOut == NULL)
 {
   bluetoothFrame->DdeConnectTo(DdeExtServerName);
 }
 char* temp = (char*)data;
 bluetoothFrame->ddeResponce = wxString::FromUTF8(temp);
 return true;
}
////// DDE ////////////////////////////
#endif
