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
#include "frsky.h"

/*static const char * const FRSKYV_opts[] = {
  _tr_noop("Freq-Fine"),  "-127", "+127", NULL,
  NULL
};

enum FRSKYV_opts {
  FRSKYV_OPT_FREQFINE =0,
  FRSKYV_OPT_LAST,
};
*/

static uint32_t seed;
static uint8_t dp_crc_init;

const static uint8_t ZZ_frskyVInitSequence[] PROGMEM = {
  CC2500_17_MCSM1, 0x0C, // Stay in receive after packet reception, Idle state after transmission
  CC2500_18_MCSM0, 0x18, // Auto calibrate when going from idle to tx/rx/fstxon
  CC2500_06_PKTLEN, 0xFF,
  CC2500_07_PKTCTRL1, 0x04,
  CC2500_08_PKTCTRL0, 0x05,
  CC2500_0B_FSCTRL1, 0x08,
  CC2500_0D_FREQ2, 0x5C,
  CC2500_0E_FREQ1, 0x58,
  CC2500_0F_FREQ0, 0x9D,
  CC2500_10_MDMCFG4, 0xAA,
  CC2500_11_MDMCFG3, 0x10, // 26977 baud.
  CC2500_12_MDMCFG2, 0x93,
  CC2500_13_MDMCFG1, 0x23,
  CC2500_14_MDMCFG0, 0x7A,
  CC2500_15_DEVIATN, 0x41, // 0x41 (+-28564Hz) for 1way, 0x42 for 2way.
  CC2500_19_FOCCFG, 0x16,
  CC2500_1A_BSCFG, 0x6C,
  CC2500_1B_AGCCTRL2, 0x43,
  CC2500_1C_AGCCTRL1, 0x40,
  CC2500_1D_AGCCTRL0, 0x91,
  CC2500_21_FREND1, 0x56,
  CC2500_22_FREND0, 0x10, // power index 0
  CC2500_23_FSCAL3, 0xA9, // Enable charge pump calibration, calibrate for each hop.
  CC2500_24_FSCAL2, 0x0A,
  CC2500_25_FSCAL1, 0x00,
  CC2500_26_FSCAL0, 0x11,
  CC2500_29_FSTEST, 0x59,
  CC2500_2C_TEST2, 0x88,
  CC2500_2D_TEST1, 0x31,
  CC2500_2E_TEST0, 0x0B,
  CC2500_03_FIFOTHR, 0x07,
  CC2500_09_ADDR, 0x00 // address 0
  };


static void FRSKYV_init(uint8_t bind)
{
  CC2500_Reset(); // 0x30

  uint_farptr_t pdata = pgm_get_far_address(ZZ_frskyVInitSequence);

  for (uint8_t i=0; i<(DIM(ZZ_frskyVInitSequence)/2); i++) { // Send init sequence.
    uint8_t add = pgm_read_byte_far(pdata);
    uint8_t dat = pgm_read_byte_far(++pdata);
    CC2500_WriteReg(add,dat);
    ++pdata;
  }


  CC2500_SetTxRxMode(TX_EN);
  CC2500_WriteReg(CC2500_0C_FSCTRL0, (int8_t) -50); // TODO Model.proto_opts[PROTO_OPTS_FREQFINE]);
//  CC2500_Strobe(CC2500_SIDLE); // Go to idle...
  CC2500_Strobe(CC2500_SFTX); // 3b
  CC2500_Strobe(CC2500_SFRX); // 3a
  CC2500_SetPower(bind ? TXPOWER_1 : TXPOWER_1);
  CC2500_Strobe(CC2500_SIDLE); // Go to idle...

}

static uint8_t FRSKYV_crc8(uint8_t result, uint8_t *data, uint8_t len)
{
  for(uint8_t i = 0; i < len; i++) {
    result = result ^ data[i];
    for(uint8_t j = 0; j < 8; j++) {
      if(result & 0x80) result = (result << 1) ^ 0x07;
      else result = result << 1;
    }
  }
  return result;
}

#if 0
static uint8_t FRSKYV_reflect8(uint8_t in)
{
  // Reflects the bits in a byte.
  uint8_t i, j, out=0;

  for(i = 0x80, j=0x01; i; i>>=1, j<<= 1) {
    if(in & i) out |= j;
  }
  return out;
}

static uint8_t FRSKYV_calc_dp_crc_init(void)
{
// How TxId relates to data packet initial crc value.
// ID      crc start value
// 0x0000  0x0E
// 0x0001  0xD7
// 0x0002  0xBB
// 0x0003  0x62
// 0x1257  0xA6
// 0x1258  0x7D
// 0x1259  0xA4
// 0x1E2D  0x89
// 0x1E2E  0xE5
// 0x1E2F  0x3C
// 0x3210  0x1F
// 0x3FFF  0x45

// Apparently it's crc8 polynominal = 0xC1 initial value=0x6B reflect in and out xor value=0.
// Fast bit by bit algorithm without augmented zero bytes.

  uint8_t c, bit;
  uint8_t crc = 0x6B; // Initial value.
  static const uint8_t poly = 0xC1;

    c = FRSKYV_reflect8(frsky_id >> 8);
    for(uint8_t j=0x80; j; j>>=1) {
      bit = crc & 0x80;
      crc<<= 1;
      if (c & j) bit^= 0x80;
      if (bit) crc^= poly;
    }

    c = FRSKYV_reflect8(frsky_id & 0xFF);
    for(uint8_t j=0x80; j; j>>=1) {
      bit = crc & 0x80;
      crc<<= 1;
      if (c & j) bit^= 0x80;
      if (bit) crc^= poly;
    }
  return FRSKYV_reflect8(crc);
}
#endif

static uint8_t FRSKYV_crc8_le(void)
{
  uint8_t result = 0xD6;

    result = result ^ (frsky_id >> 8);
    for(uint8_t j = 0; j < 8; j++) {
      if(result & 0x01) result = (result >> 1) ^ 0x83;
      else result = result >> 1;
    }

    result = result ^ (frsky_id & 0xFF);
    for(uint8_t j = 0; j < 8; j++) {
      if(result & 0x01) result = (result >> 1) ^ 0x83;
      else result = result >> 1;
    }

  return result;
}


static void FRSKYV_build_bind_packet()
{
  static uint8_t bind_idx =0;

  packet[0] = 0x0E; //Length
  packet[1] = 0x03; //Packet type
  packet[2] = 0x01; //Packet type
  packet[3] = frsky_id & 0xFF;
  packet[4] = frsky_id >> 8;
  packet[5] = bind_idx *5; // Index into channels_used array.
  packet[6] =  channels_used[ (packet[5]) +0];
  packet[7] =  channels_used[ (packet[5]) +1];
  packet[8] =  channels_used[ (packet[5]) +2];
  packet[9] =  channels_used[ (packet[5]) +3];
  packet[10] = channels_used[ (packet[5]) +4];
  packet[11] = 0x00;
  packet[12] = 0x00;
  packet[13] = 0x00;
  packet[14] = FRSKYV_crc8(0x93, packet, 14);

  ++bind_idx;
  if(bind_idx > 9) bind_idx = 0;
}


static void FRSKYV_build_data_packet()
{
  static uint8_t V_state =0;
  uint8_t ofsetChan = 0;

  packet[0] = 0x0E;
  packet[1] = frsky_id & 0xFF;
  packet[2] = frsky_id >> 8;
  packet[3] = seed & 0xFF;
  packet[4] = seed >> 8;

  // Appears to be a bitmap relating to the number of channels sent e.g.
  // 0x0F -> first 4 channels, 0x70 -> channels 5,6,7, 0xF0 -> channels 5,6,7,8
  if(V_state == 0 || V_state == 2) packet[5] = 0x0F;
  else if(V_state == 1 || V_state == 3) { ofsetChan = 4; packet[5] = 0xF0;}
  else packet[5] = 0x00;


  for(uint8_t i = 0; i < 4; i++) {
      // 0x08CA / 1.5 = 1500 (us). Probably because they use 12MHz clocks.
      // 0x05DC -> 1000us 5ca
      // 0x0BB8 -> 2000us bca

      int16_t value = channelOutputs[i + ofsetChan];
      value -= (value>>2); // x-x/4
      value = limit((int16_t)-(640 + (640>>1)), value, (int16_t)+(640 + (640>>1)));
      value += 0x08CA;

      packet[6 + (i*2)] = value & 0xFF;
      packet[7 + (i*2)] = (value >> 8) & 0xFF;
  }

  packet[14] = FRSKYV_crc8(dp_crc_init, packet, 14);
  ++V_state;
  if(V_state > 4) V_state =0;
  // Potentially if we had only four channels we could send them every 9ms.
}


static uint16_t FRSKYV_data_cb()
{
  // Build next packet.
  seed = (uint32_t) (seed * 0xAA) % 0x7673; // Prime number 30323.
  FRSKYV_build_data_packet(); // 16MHz AVR = 127us.

  /* TODO Update options which don't need to be every 9ms. */
  static uint8_t option = 0;
  if(option == 0) CC2500_SetTxRxMode(TX_EN); // Keep Power Amp activated.
  else if(option == 64) CC2500_SetPower(TXPOWER_2); // TODO update power level.
  else if(option == 128) CC2500_WriteReg(CC2500_0C_FSCTRL0, (int8_t) 0); // TODO Update fine frequency value.
  else if(option == 196) CC2500_Strobe(CC2500_SIDLE); // MCSM1 register setting puts CC2500 back into idle after TX.

  CC2500_WriteReg(CC2500_0A_CHANNR, channels_used[ ( (seed & 0xFF) % 50) ] ); // 16MHz AVR = 38us.
  CC2500_Strobe(CC2500_SFTX); // Flush Tx FIFO.
  CC2500_WriteData(packet, 15);
  CC2500_Strobe(CC2500_STX); // 8.853ms before we start again with the idle strobe.

  // Wait for transmit to finish. Timing is tight. Only 581uS between packet being emitted and idle strobe.
  // while( 0x0F != CC2500_Strobe(CC2500_SNOP)) { _delay_us(5); }
  heartbeat |= HEART_TIMER_PULSES;
  dt = TCNT1 - OCR1A; // Calculate latency and jitter.
  return 9000 *2;
}

static uint16_t FRSKYV_bind_cb()
{
  FRSKYV_build_bind_packet();
  CC2500_Strobe(CC2500_SIDLE);
  CC2500_WriteReg(CC2500_0A_CHANNR, 0);
  CC2500_Strobe(CC2500_SFTX); // Flush Tx FIFO
  CC2500_WriteData(packet, 15);
  CC2500_Strobe(CC2500_STX); // Tx
  heartbeat |= HEART_TIMER_PULSES;
  dt = TCNT1 - OCR1A; // Calculate latency and jitter.
  return 18000U *2;
}


static void FRSKYV_initialise(uint8_t bind)
{
  CLOCK_StopTimer();

  frsky_id = SpiRFModule.fixed_id & 0x7FFF;

  // Build channel array.
  /*channel_offset = frsky_id % 5;
  uint8_t chan_num;
  for(uint8_t x = 0; x < 50; x ++) {
    chan_num = (x*5) + 6 + channel_offset;
	channels_used[x] = chan_num ? chan_num : 1; // Avoid binding channel 0.
  }*/
  FRSKY_generate_channels();

//dp_crc_init = FRSKYV_calc_dp_crc_init();
  dp_crc_init = FRSKYV_crc8_le();

  if(bind) {
    FRSKYV_init(1);
    PROTOCOL_SetBindState(0xFFFFFFFF);
    CLOCK_StartTimer(25000U *2, FRSKYV_bind_cb);
  } else {
    FRSKYV_init(0);
    seed = 2UL;
    FRSKYV_build_data_packet();
    CLOCK_StartTimer(25000U *2, FRSKYV_data_cb);
  }
}



const void * FRSKYV_Cmds(enum ProtoCmds cmd)
{
  switch(cmd) {
  case PROTOCMD_INIT:
    FRSKYV_initialise(0);
    return 0;
//        case PROTOCMD_DEINIT:
//        case PROTOCMD_RESET:
//            CLOCK_StopTimer();
//            return (void *)(CC2500_Reset() ? 1L : -1L);
  case PROTOCMD_RESET:
    CLOCK_StopTimer();
    CC2500_Reset();
    return 0;
  //case PROTOCMD_CHECK_AUTOBIND:
    //return 0; //Never Autobind.
  case PROTOCMD_BIND:
    FRSKYV_initialise(1);
    return 0;
  case PROTOCMD_GETNUMOPTIONS:
    return (void *)1L;
  //case PROTOCMD_NUMCHAN:
    //return (void *)8L;
  //case PROTOCMD_DEFAULT_NUMCHAN:
    //return (void *)8L;
//        case PROTOCMD_CURRENT_ID: return Model.fixed_id ? (void *)((unsigned long)Model.fixed_id % 0x4000) : 0;
  //case PROTOCMD_GETOPTIONS:
    //return FRSKYV_opts;
  //case PROTOCMD_TELEMETRYSTATE:
    //return (void *)(long) PROTO_TELEM_UNSUPPORTED;
  default:
    break;
  }
  return 0;
}

