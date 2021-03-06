/*
 * Authors (alphabetical order)
 *
 * - Rick Kilvington (2018).
 *
 */


#ifndef xmega_board_h
#define xmega_board_h

#include "../../OpenAVRc.h"

#define GETADC_COUNT 1  // Used in switches.cpp

// USART General defines.
#define USART_SET_BAUD_9K6(usartx)   _USART_SET_BAUD(usartx, 12, +4)
#define USART_SET_BAUD_57K6(usartx)  _USART_SET_BAUD(usartx, 135,-2)
#define USART_SET_BAUD_100K(usartx)  _USART_SET_BAUD(usartx, 159,-3)
#define USART_SET_BAUD_125K(usartx)  _USART_SET_BAUD(usartx, 127,-3)

#define _USART_SET_BAUD(usartx, bsel, bscale) \
  { usartx.BAUDCTRLA = bsel & 0xFF; usartx.BAUDCTRLB = ((int8_t) bscale << USART_BSCALE_gp) | (bsel >> 8); }
#define USART_SET_MODE_8N1(usartx) \
  { usartx.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | (0 << USART_SBMODE_bp) | USART_CHSIZE_8BIT_gc; } // 8N1
#define USART_SET_MODE_8E2(usartx) \
  { usartx.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_EVEN_gc | (1 << USART_SBMODE_bp) | USART_CHSIZE_8BIT_gc; } // 8E2

#define USART_ENABLE_TX(usartx)  usartx.CTRLB |= USART_TXEN_bm;
#define USART_ENABLE_RX(usartx)  usartx.CTRLB |= USART_RXEN_bm;
#define USART_DISABLE_TX(usartx)  usartx.CTRLB &= ~USART_TXEN_bm;
#define USART_DISABLE_RX(usartx)  usartx.CTRLB &= ~USART_RXEN_bm;

#define USART_XCK_PIN_bm    PIN1_bm
#define USART_RXD_PIN_bm    PIN2_bm
#define USART_TXD_PIN_bm    PIN3_bm

#if defined (MULTIMODULE)
#define PROTO_HAS_MULTISUPIIIK
#endif

// All communication to RF module(s) will use the same pin ... e.g. USART MSPI, USART Asynch and PPM.
#define MULTI_USART             USARTE0
#define MULTI_USART_PORT        PORTE
#define MULTI_USART_RXC_VECT    USARTE0_RXC_vect
#define MULTI_USART_DRE_VECT    USARTE0_DRE_vect

// Telemetry USART.
#define TLM_USART               USARTD0
#define TLM_USART_PORT          PORTD
#define TLM_USART_RXC_VECT      USARTD0_RXC_vect
#define TLM_USART_DRE_VECT      USARTD0_DRE_vect


// token pasting
#define token_paste2_int(x, y) x ## y
#define token_paste2(x, y) token_paste2_int(x, y)
#define token_paste3_int(x, y, z) x ## y ## z
#define token_paste3(x, y, z) token_paste3_int(x, y, z)


// RF Module Timer.
#define RF_TC                      TCE0
#define RF_PORT                    PORTE
#define RF_TIMER_COMPA_VECT        TCE0_CCA_vect
#define RF_TIMER                   RF_TC.CNT
#define RF_TIMER_COMPA_REG         RF_TC.CCA
#define RF_TIMER_RESUME_INTERRUPT  RF_TC.INTCTRLB |=  (0b11 << TC0_CCAINTLVL_gp); // Level 3 - High Priority.
#define RF_TIMER_PAUSE_INTERRUPT   RF_TC.INTCTRLB &= ~(0b11 << TC0_CCAINTLVL_gp);
#define RF_TIMER_CLEAR_COMPA_FLAG  RF_TC.INTFLAGS = TC0_CCAIF_bm; // clear ccaif.
#define RF_OUT_PIN                 3
#define RF_OUT_PIN_CTRL_REG        token_paste3(RF_PORT.PIN, RF_OUT_PIN, CTRL) // e.g. "PORTx.PINnCTRL"


void setup_rf_tc(void);
void rf_usart_serial_init(void);

#define CALCULATE_LAT_JIT()  dt = (RF_TC.CNT - RF_TIMER_COMPA_REG ) // Calculate latency and jitter.

#if defined(SPIMODULES) // PORTE 0
char rf_usart_mspi_xfer(char c);
#define RF_SPI_xfer  rf_usart_mspi_xfer
void rf_usart_mspi_init(void);
#define RF_SPI_INIT  rf_usart_mspi_init

#define RF_USART                  USARTE0
#define RF_CS_PIN_bm              PIN0_bm

#define RF_CS_CC2500_ACTIVE()     PORTE.OUTCLR = RF_CS_PIN_bm
#define RF_CS_CC2500_INACTIVE()   PORTE.OUTSET = RF_CS_PIN_bm
#define RF_CS_CYRF6936_ACTIVE()   PORTE.OUTCLR = RF_CS_PIN_bm
#define RF_CS_CYRF6936_INACTIVE() PORTE.OUTSET = RF_CS_PIN_bm
#define RF_CS_NRF24L01_ACTIVE()   PORTE.OUTCLR = RF_CS_PIN_bm
#define RF_CS_NRF24L01_INACTIVE() PORTE.OUTSET = RF_CS_PIN_bm
#define RF_CS_A7105_ACTIVE()      PORTE.OUTCLR = RF_CS_PIN_bm
#define RF_CS_A7105_INACTIVE()    PORTE.OUTSET = RF_CS_PIN_bm

#define WAIT_RF_BUFFER_EMPTY()    while(! (RF_USART.STATUS & USART_DREIF_bm) )
#define WAIT_RF_TX_FIN()          while(! (RF_USART.STATUS & USART_TXCIF_bm) )
#define WAIT_RF_RX_FIN()          while(! (RF_USART.STATUS & USART_RXCIF_bm) )
#endif // SPIMODULES


// Trainer Pulses.
// Xmega uses the same pin for PPM input and output.
#define PULSES_IN_TC         TCF0
#define TRAINER_TC_REG       PULSES_IN_TC.CCA
#define PULSES_IN_TC_VECT    TCF0_CCA_vect
#define TRAINER_TC_VECT      PULSES_IN_TC_VECT
#define TRN_PULSES_IN_PIN    3
#define TRN_PULSES_IN_PORT   PORTF
#define TRN_PULSES_IN_PIN_CTRL_REG   token_paste3(TRN_PULSES_IN_PORT.PIN, TRN_PULSES_IN_PIN, CTRL) // e.g. "PORTF.PIN1CTRL"

#define PULSES_OUT_TC        TCF0
#define TRN_PULSES_OUT_PIN   3
#define TRAINER_PORT         PORTF
#define TRN_PULSES_OUT_PIN_CTRL_REG  token_paste3(TRAINER_PORT.PIN, TRN_PULSES_OUT_PIN, CTRL)
void setup_trainer_tc(void);


// Teacher / Pupil Mode.
#define ENABLE_TRAINER_INTERRUPT()   PULSES_IN_TC.INTCTRLB |=  (0b10 << TC0_CCAINTLVL_gp); // Level 2 - Medium Priority.
#define DISABLE_TRAINER_INTERRUPT()  PULSES_IN_TC.INTCTRLB &= ~(0b11 << TC0_CCAINTLVL_gp);
#define WAIT_PUPIL()                 ENABLE_TRAINER_INTERRUPT()
#define PPM16_CONF()                 DISABLE_TRAINER_INTERRUPT()


// Switch cross mapping.
#define SW_ID0 SW_Jup
#define SW_ID1 SW_Jmi
#define SW_ID2 SW_Jdn

#define SW_GEA SW_Gdn
#define SW_RUD SW_Ldn
#define SW_ELE SW_Odn

#define SW_THR SW_N
#define SW_AIL SW_I
#define SW_TRN SW_M


// MPX
#define MPX_USART       USARTE0
#define MPX_PORT        PORTE


// Audio - Piezo Sounder driver.
#define AUDIO_TC             TCD0
#define AUDIO_COMPARE_REG    AUDIO_TC.CCABUF
#define speakerOn()   AUDIO_TC.CTRLB |= TC0_CCAEN_bm
#define speakerOff()  AUDIO_TC.CTRLB &= ~TC0_CCAEN_bm
#define OD_AUDIO_bm          PIN0_bm


//#if defined(VOICE_JQ6500)
#define VOICE_USART       USARTC0
#define VOICE_USART_PORT  PORTC
#define VOICE_BUSY_PIN    4
#define VOICE_BUSY_PIN_CTRL_REG   token_paste3(PORTF.PIN, VOICE_BUSY_PIN, CTRL) // e.g. "PORTF.PIN4CTRL"
#define JQ6500_BUSY       (PORTF.IN & (1<<VOICE_BUSY_PIN))
#define VOICE_DRE_VECT    USARTC0_DRE_vect
extern void InitJQ6500UartTx();
//#endif

// Trims Button  Matrix.
#define IE_TRIM_COL_A_bm    PIN4_bm
#define IE_TRIM_COL_B_bm    PIN5_bm
#define IE_TRIM_COL_C_bm    PIN6_bm
#define IE_TRIM_COL_D_bm    PIN7_bm
#define OF_TRIM_ROW_A_bm    PIN2_bm


// Trim Buttons.
void read_trim_matrix(void);
#define TRIMS_PRESSED() false

// 10mS / 64us Counter.
#define MS064_TC                   TCC1
#define COUNTER_64uSH              MS064_TC.CNTH
#define COUNTER_64uSL              MS064_TC.CNTL
#define TIMER_10MS_VECT            TCC1_CCA_vect
#define TIMER_10MS_COMPVAL         MS064_TC.CCA
#define PAUSE_10MS_INTERRUPT       MS064_TC.INTCTRLB &= (0b11 << TC1_CCAINTLVL_gp);
#define RESUME_10MS_INTERRUPT      MS064_TC.INTCTRLB |=  (0b01 << TC1_CCAINTLVL_gp); // Level 1 - Low Priority.
#define MS064_TC_CLEAR_CCAIF_FLAG  MS064_TC.INTFLAGS = TC1_CCAIF_bm; // clear ccaif.

// SD Card driver.
#define sdDone()
#define SD_IS_HC()              (0)
#define SD_GET_SPEED()          (0)
#define SDCARD_SPI              SPIC
#define SDCARD_PORT             PORTC
#define O_SDCARD_CSN_bm         PIN4_bm
#define select_card()           SDCARD_PORT.OUTCLR = O_SDCARD_CSN_bm
#define SDCARD_CS_N_ACTIVE()    SDCARD_PORT.OUTCLR = O_SDCARD_CSN_bm
#define unselect_card()         SDCARD_PORT.OUTSET = O_SDCARD_CSN_bm
#define SDCARD_CS_N_INACTIVE()  SDCARD_PORT.OUTSET = O_SDCARD_CSN_bm
#define SPI_8M()                MSPI_16M(SDCARD_SPI)
#define SPI_250K()              MSPI_250K(SDCARD_SPI)
#define SPI_START_SPEED()       SPI_250K()
#define LEDON()    {}
#define LEDOFF()   {}

// SPI general.
#define MSPI_250K(spix)  { spix.CTRL &= ~(SPI_CLK2X_bm); spix.CTRL |= 0b11 << SPI_PRESCALER_gp; }
#define MSPI_500K(spix)  { spix.CTRL |=  (SPI_CLK2X_bm); spix.CTRL |= 0b11 << SPI_PRESCALER_gp; }
#define MSPI_2M(spix)    { spix.CTRL &= ~(SPI_CLK2X_bm | SPI_PRESCALER1_bm); spix.CTRL |= SPI_PRESCALER0_bm; }
#define MSPI_4M(spix)    { spix.CTRL |=  (SPI_CLK2X_bm | SPI_PRESCALER0_bm); spix.CTRL &= ~SPI_PRESCALER1_bm; }
#define MSPI_8M(spix)    { spix.CTRL &= ~(SPI_CLK2X_bm); spix.CTRL &= ~(0b11 << SPI_PRESCALER_gp); }
#define MSPI_16M(spix)   { spix.CTRL |=  (SPI_CLK2X_bm); spix.CTRL &= ~(0b11 << SPI_PRESCALER_gp); }

// Power driver.
#define PWR_HOLD_PORT        PORTB
#define OB_PWR_HOLD_bm       PIN7_bm
//#define PWR_STATUS_PORT      PORTB
//#define IB_PWR_STATUS_bm     PIN7_bm
#define PWR_LED_PORT         PORTC
#define OC_PWR_LED_bm        PIN2_bm

// Power LED / Backlight driver.
#define BACKLIGHT_PORT        PORTC
#define OC_BACKLIGHT_bm       PIN2_bm
#if !defined(PWM_BACKLIGHT)
#define BACKLIGHT_ON()  (BACKLIGHT_PORT.OUTSET = OC_BACKLIGHT_bm)
#define BACKLIGHT_OFF() (BACKLIGHT_PORT.OUTCLR = OC_BACKLIGHT_bm)
#else
#define BACKLIGHT_ON()  (TCC2.LCMPC = ( (uint8_t) g_eeGeneral.blOnBright)  <<4)
#define BACKLIGHT_OFF() (TCC2.LCMPC = ( (uint8_t) g_eeGeneral.blOffBright) <<4)
#endif

#if defined(PWRMANAGE)
#define UNEXPECTED_SHUTDOWN()    ((mcusr & (RST_WDRF_bm)) || g_eeGeneral.unexpectedShutdown)
#else
#define UNEXPECTED_SHUTDOWN()    (mcusr & (RST_WDRF_bm))
#endif

// Rotary encoder pins.
#define ROT_ENC_PORT               PORTF
#define IF_ROT_ENC_LH_CLK_bm       PIN0_bm
#define ROT_ENC_LH_CLK_CTRL_REG    PORTF.PIN0CTRL
#define IF_ROT_ENC_LH_DIR_bm       PIN1_bm
#define IF_ROT_ENC_RH_CLK_bm       PIN6_bm
#define ROT_ENC_RH_CLK_CTRL_REG    PORTF.PIN6CTRL
#define IF_ROT_ENC_RH_DIR_bm       PIN7_bm

#define REA_DOWN()	switchState((EnumKeys)BTN_REa)
#define REB_DOWN()	switchState((EnumKeys)BTN_REb)
#define ROTENC_DOWN() (REA_DOWN() || REB_DOWN())

#define ENABLEROTENCAISR()     {}
#define ENABLEROTENCBISR()     {}
#define DISABLEROTENCAISR()    {}
#define DISABLEROTENCBISR()    {}

// TWI / I2C.
#define FRAM_RTC_TWI              TWIC
#define FRAM_RTC_TWI_TWIM_VECT    TWIC_TWIM_vect

// EEPROM driver.
#define EEPROM_EnableMapping()     NVM.CTRLB |= NVM_EEMAPEN_bm
#define EEPROM_DisableMapping()    NVM.CTRLB &= ~NVM_EEMAPEN_bm
#define NVM_EXEC()                 _PROTECTED_WRITE(NVM.CTRLA, 0b1);
void EEPROM_WaitForNVM(void);
void EEPROM_FlushBuffer(void);

#if !defined(SIMU)
#if defined(EXTERNALEEPROM)
#define ADDRESS_EXTERN_EEPROM  (0x50 << 1) // 0x50 with all strap closed on ZS042 module (or alone), EEPROM FM24W256, see datasheet
#define EEPROMREADBLOCK(a, b, c)   Ext_eeprom_read_block(a, b, c) //External EEPROM
#else
#define EEPROMREADBLOCK(a, b, c) \
  memcpy(a, (const void *) (b + MAPPED_EEPROM_START), c) // Xmega EEPROM is memory mapped for read access.
#endif

#endif // ! SIMU

#endif // xmega_board_h

