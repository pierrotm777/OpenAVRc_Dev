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

#include "../OpenAVRc.h"
#include "menu_general.h"
#include "menus.h"

enum menuGeneralBTItems
{
  ITEM_BT_ONOFF,
  ITEM_BT_NAME,
  ITEM_BT_ROLE,
  ITEM_BT_PIN,
  ITEM_BT_PEER,
  ITEM_BT_AUTOCONNECT,
  ITEM_BT_RESCANN,
  ITEM_BT_END
};
#define BT_Tab 0
#define BT_2ND_COLUMN 10*FW

#define STR_BLUETOOTH      PSTR("BLUETOOTH")
#define STR_BT_ROLE        PSTR("Role")
#define STR_BT_PIN         PSTR("Pin")
#define STR_BT_M_S         PSTR("\006""Slave\0""Master")
#define STR_BT_PAIR        PSTR("Pair")
#define STR_AUTOCON        PSTR("Auto-con.")
#define STR_RESCANN        PSTR("Re-Scan")

#define IF_NO_ERROR(x) if (x >= 0)

const pm_char STR_BTACTIVE[] PROGMEM = TR_ACTIVED;

void loadDataFromModule()
{
  if (g_eeGeneral.BT.Power)
    {
    bluetooth_AtCmdMode(ON);

    IF_NO_ERROR(bluetooth_getName(reusableBuffer.bluetooth.name_str, sizeof(reusableBuffer.bluetooth.name_str), BT_GET_TIMEOUT_MS))
    {
      str2zchar(reusableBuffer.bluetooth.name_zchar, reusableBuffer.bluetooth.name_str, sizeof(reusableBuffer.bluetooth.name_zchar));

      IF_NO_ERROR(bluetooth_getPswd(reusableBuffer.bluetooth.pin_str, sizeof(reusableBuffer.bluetooth.pin_str), BT_GET_TIMEOUT_MS))
      {
        str2zchar(reusableBuffer.bluetooth.pin_zchar, reusableBuffer.bluetooth.pin_str, sizeof(reusableBuffer.bluetooth.pin_zchar));

        //IF_NO_ERROR(bluetooth_getRemoteName(g_eeGeneral.BT.Peer.Mac, reusableBuffer.bluetooth.peer_name_str, sizeof(reusableBuffer.bluetooth.peer_name_str), BT_READ_RNAME_TIMEOUT_MS))
        {
          //success
        }
        //else
        {
          //reusableBuffer.bluetooth.peer_name_str[0] = 0; // none
        }
      }
    }
    bluetooth_AtCmdMode(OFF);
    reusableBuffer.bluetooth.firstMenuRun = 1;
  }
  else
    {
      reusableBuffer.bluetooth.firstMenuRun = 0;
    }
}

void writeDataToModule()
{
  checkIncDec_Ret = 0;
  bluetooth_init(&Serial1); // init or power off
  if (g_eeGeneral.BT.Power)
    {
      zchar2str(reusableBuffer.bluetooth.name_str, reusableBuffer.bluetooth.name_zchar, strlen(reusableBuffer.bluetooth.name_zchar));
      bluetooth_AtCmdMode(ON);
      //bluetooth_addSuffix(reusableBuffer.bluetooth.name_str);
      bluetooth_setName(reusableBuffer.bluetooth.name_str, BT_SET_TIMEOUT_MS);
      //bluetooth_nameSet(reusableBuffer.bluetooth.name_zchar);
      zchar2str(reusableBuffer.bluetooth.pin_str, reusableBuffer.bluetooth.pin_zchar, 4);
      bluetooth_setPswd(reusableBuffer.bluetooth.pin_str, BT_SET_TIMEOUT_MS);
      if(g_eeGeneral.BT.Master && g_eeGeneral.BT.AutoCnx)
      {
        bluetooth_linkToRemote(g_eeGeneral.BT.Peer.Mac, BT_SET_TIMEOUT_MS);
      }
      bluetooth_AtCmdMode(OFF);
      loadDataFromModule();
    }
}

void onPairSelected(const char *result)
{
  // result is the new pair name!!
  strcpy(reusableBuffer.bluetooth.peer_name_str, result);
  str2zchar(reusableBuffer.bluetooth.peer_name_str, reusableBuffer.bluetooth.peer_name_str, sizeof(reusableBuffer.bluetooth.peer_name_str));
  memcpy(g_eeGeneral.BT.Peer.Mac, reusableBuffer.bluetooth.scann.Remote[shared_u8].MAC, BT_MAC_BIN_LEN);
  bluetooth_AtCmdMode(ON);
  IF_NO_ERROR(bluetooth_linkToRemote(g_eeGeneral.BT.Peer.Mac, BT_SET_TIMEOUT_MS))
  {
    eeDirty(EE_GENERAL);
    // TODO show "linked :)"
  }
  bluetooth_AtCmdMode(OFF);
}

void menuGeneralBluetooth(uint8_t event)
{
  if ((!reusableBuffer.bluetooth.firstMenuRun) && g_eeGeneral.BT.Power)
    {
      loadDataFromModule();
    }

  MENU(STR_BLUETOOTH, menuTabGeneral, e_Bluetooth, ITEM_BT_END+1, {BT_Tab});

  shared_u8 = s_menu_item; // shared_u8 store the popup scan selected line

  if (warningResult)
    {
      warningResult = false;
      bluetooth_scann(&reusableBuffer.bluetooth.scann, BT_SCANN_TIMEOUT_MS);

      POPUP_MENU_ITEMS_FROM_BSS();
      for (uint8_t i=0; i < REMOTE_BT_DEV_MAX_NB; ++i)
        {
          POPUP_MENU_ADD_ITEM(reusableBuffer.bluetooth.scann.Remote[i].Name);
        }
      popupMenuHandler = onPairSelected; // Selection is done in popup -> Call onPairSelected
    }

  uint8_t addExt = 0; // used to add _M or _S
  coord_t y = MENU_HEADER_HEIGHT + 1;
  uint8_t sub = menuVerticalPosition - 1;
  uint8_t slen;

  for (uint8_t i=0; i<LCD_LINES-1; ++i)
    {
      uint8_t k = i+menuVerticalOffset;
      uint8_t attr = (sub == k ? ((s_editMode>0) ? BLINK|INVERS : INVERS) : 0);

      switch(k)
        {
        case ITEM_BT_ONOFF :
          ON_OFF_MENU_ITEM(g_eeGeneral.BT.Power, BT_2ND_COLUMN, y, STR_BTACTIVE, attr, event);
          break;

        case ITEM_BT_NAME :
          editSingleName(BT_2ND_COLUMN, y, STR_NAME, reusableBuffer.bluetooth.name_zchar, strlen(reusableBuffer.bluetooth.name_zchar), event, attr, EE_NO, RANGE_UPPER);
          addExt = 1;
          break;

        case ITEM_BT_ROLE :
          lcdDrawTextLeft(y, STR_BT_ROLE);
          lcdDrawTextAtIndex(BT_2ND_COLUMN, y, STR_BT_M_S, g_eeGeneral.BT.Master, attr);
          if (attr)
            CHECK_INCDEC_GENVAR(event, g_eeGeneral.BT.Master, 0, 1);
          break;

        case ITEM_BT_PIN :
          editSingleName(BT_2ND_COLUMN, y, STR_BT_PIN, reusableBuffer.bluetooth.pin_zchar, 4, event, attr, EE_GENERAL, RANGE_NUMBER);
          break;

        case ITEM_BT_PEER :
          lcdDrawTextLeft(y, STR_BT_PAIR);
          slen = strlen(reusableBuffer.bluetooth.peer_name_str);
          if (slen)
          {
            lcdDrawSizedTextAtt(BT_2ND_COLUMN, y, reusableBuffer.bluetooth.peer_name_str, slen, BSS|attr);
          }
          else
          {
            lcdLastPos = BT_2ND_COLUMN;
          }
          addExt = 2;
          break;

        case ITEM_BT_AUTOCONNECT :
          g_eeGeneral.BT.AutoCnx = onoffMenuItem(g_eeGeneral.BT.AutoCnx, BT_2ND_COLUMN, y, STR_AUTOCON, attr, event);
          break;

        case ITEM_BT_RESCANN :
          lcdDrawTextAtt(7*FW,y,STR_RESCANN, g_eeGeneral.BT.Master? attr : BLINK);
          if (attr && (event==EVT_KEY_BREAK(KEY_ENTER)) && g_eeGeneral.BT.Master)
            {
              POPUP_CONFIRMATION(STR_RESCANN);
              s_editMode = 0;
            }
          break;
        }

        if ((checkIncDec_Ret) && (!s_editMode) && reusableBuffer.bluetooth.firstMenuRun)
          {
            writeDataToModule();
          }

      if (addExt)
        {
          lcdDrawTextAtt(lcdLastPos,y,((g_eeGeneral.BT.Master ^ (addExt & 0x1))? Str_BT_Slave : Str_BT_Master),attr);
          addExt = 0;
        }
      y += FH;
    }
}