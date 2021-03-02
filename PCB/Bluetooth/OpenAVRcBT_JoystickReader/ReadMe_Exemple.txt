![Comic Sans](https://github.com/AlessandroAU/ExpressLRS/blob/a52464c7095d6dde1e0224589ebb9bfebf745def/img/wiki.png)

# Welcome to the `ExpressLRS` wiki!

### :hatching_chick:`TLDR?` [Get started quickly!](https://github.com/AlessandroAU/ExpressLRS/wiki/Toolchain-and-Git-Setup)

# [FAQ](https://github.com/AlessandroAU/ExpressLRS/wiki/FAQ)

## Tech Facts

`ExpressLRS` is an **open-source** remote control link!

* Based on `SX127x`/`SX1280` hardware combined with an `ESP8285`, `ESP32` or `STM32` for the receiver (RX) and transmitter (TX) respectively.
* Multi-Band Support for **868/915 MHz and 2.4 GHz** - possibly more in the future.
* Can be flashed onto **existing hardware** (`Frsky R9M`) or **custom PCBs** can be made.
* Runs at up to 250hz (or down to 50hz) updates per second (transmission rate) depending on your preference of **range or low latency**:
  * At 868/915 MHz a packet rate of **up to 200 Hz** is supported. Stick latency can be low as 6.5ms as seen on firmware with `crsfshot` (aka `OpenTX 2.4` mixer scheduling) support. 🧠 
  * At **2.4 GHz** up to 500 Hz is supported (only on 2.3.10). 🚤 
  * Due to the optimized packet structure **only basic telemetry** that gives up/downlink information is currently supported. Basic telemetry currently includes: RQLY, TQLY, RSSI x2, SNR x2, and VBAT⚠️

## Build Status

We do [continuous integration](https://github.com/AlessandroAU/ExpressLRS/actions) and according quality assurance as far as possible.

![Build Status](https://github.com/AlessandroAU/ExpressLRS/workflows/Build%20ExpressLRS/badge.svg)
![Release](https://img.shields.io/github/v/release/AlessandroAu/ExpressLRS?include_prereleases)
![License](https://img.shields.io/github/license/AlessandroAU/ExpressLRS)
![Stars](https://img.shields.io/github/stars/AlessandroAU/ExpressLRS)

## Community 

You are welcome to 

 * Join the development community on [Discord](https://discord.gg/dS6ReFY) :family_man_girl_boy:
 * Post on the [**rcgroups** forum](https://www.rcgroups.com/forums/showthread.php?3437865-ExpressLRS-DIY-LoRa-based-race-optimized-RC-link-system) 📰 **(relatively inactive, the discord is much better)**

## Supported Hardware

Development is ongoing but the following hardware is currently compatible

### 900 MHz Hardware:

- **TX**
    - [FrSky R9M (2018)](https://www.frsky-rc.com/product/r9m/) (Full Support, requires resistor mod)
    - [FrSky R9M (2019)](https://www.frsky-rc.com/product/r9m-2019/) (Full Support, no mod required)
    - [FrSky R9M Lite](https://www.frsky-rc.com/product/r9m-lite/) (Full Support, power limited)
    - [TTGO LoRa V1/V2](http://www.lilygo.cn/products.aspx?TypeId=50003&fid=t3:50003:3) (Full Support, V2 recommended w/50 mW power limit)
    - DIY Module (Full Support, 50mW limit, limited documentation)
- **RX**
    - [FrSky R9mm](https://www.frsky-rc.com/product/r9-mm-ota/) (Full Support, OTA version can be used)
    - [FrSky R9 Mini](https://www.frsky-rc.com/product/r9-mini-ota/) (Full Support, OTA version can be used)
    - [FrSky R9mx](https://www.frsky-rc.com/product/r9-mx/) (Full Support)
    - [FrSky R9 Slim+](https://www.frsky-rc.com/product/r9-slim-ota/) (Full Support, OTA version can be used, diversity not yet implemented)
    - [Jumper R900 mini](https://www.jumper-b2b.com/jumper-r900-mini-receiver-900mhz-long-range-rx-p0083.html) (Full Support, only flashable via STLink, Bad Stock antenna)
    - [DIY mini RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/900MHz/RX_Mini_v1.1) (Full Support, supports WiFi Updates)
    - [DIY 20x20 RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/900MHz/RX_20x20_0805_SMD) (Full Support, supports WiFi Updates)

### 2.4 GHz Hardware:

- **TX**
    - [DIY JR Bay](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/TX_SX1280) (Full Support, 27dBm, supports WiFi Updates)
    - [DIY Slim TX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/TX_SX1280_Slim) (Full Support, 27dBm, supports Wifi Updates, fits Slim Bay)
    - [DIY Slimmer TX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/TX_SX1280_Slimmer) (Full Support, 27dBm, supports Wifi Updates, fits Slim Bay)
    - [GHOST TX](https://www.immersionrc.com/fpv-products/ghost/) (Beta Support, 250 mW output power)
- **RX**
    - [GHOST Atto](https://www.immersionrc.com/fpv-products/ghost/) (Full Support, Only STLink Flashing)
    - [GHOST Zepto](https://www.immersionrc.com/fpv-products/ghost/) (Full Support, Only STLink Flashing)
    - [DIY 20x20 RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/RX_20x20) (Full Support, easy to build. WiFi Updating)
    - [DIY Nano RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/RX_Nano) (Full Support, CRSF Nano Footprint, WiFi Updating)
    - [DIY Nano CCG RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/RX_CCG_Nano) (Full Support, CRSF Nano Pinout, STM32 Based)
    - [DIY Nano Ceramic RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/2400MHz/RX_Nano_Ceramic) (Full Support, CRSF Nano Footprint, WiFi Updating, Built in antenna)

**For a more exhaustive list refer to the [Supported Hardware](https://github.com/AlessandroAU/ExpressLRS/wiki/Supported-Hardware) Page**

🐰 Please [join the discord](https://github.com/AlessandroAU/ExpressLRS/wiki#community) to extend hardware support.

## Software Installation/Quick Start

1. [Toolchain Setup](https://github.com/AlessandroAU/ExpressLRS/wiki/Toolchain-and-Git-Setup) :robot:
2. [Flashing ExpressLRS](https://github.com/AlessandroAU/ExpressLRS/wiki/Flashing-ExpressLRS) 🧑‍✈️ 
3. [Transmitter and Flight Controller Setup](https://github.com/AlessandroAU/ExpressLRS/wiki/OpenTX-and-Betaflight-Setup) :seat:
4. [Binding](https://github.com/AlessandroAU/ExpressLRS/wiki/Binding) [🛫](https://github.com/AlessandroAU/ExpressLRS/wiki/_Binding-Phrases)

## Custom Hardware 

### Transmitters

- [TTGO LoRa Version 1 and 2](https://github.com/AlessandroAU/ExpressLRS/wiki/Supported-Off-The-Shelf-Hardware#ttgo-lora-v1) 📶 
- [DIY TX](Building-an-Esp-based-Tx) :artificial_satellite: 

### Recievers

- [DIY RX](Building-an-Esp-Rx) :satellite:

### Advanced

- [Adding an ESP Backpack](https://github.com/AlessandroAU/ExpressLRS/wiki/ESP-Backpack-Addon) :tent:

## Videos

We have some [📼 to share](https://github.com/AlessandroAU/ExpressLRS/wiki/ExpressLRS-Videos) showing various milestones achievements and range testing - [please have a look](https://github.com/AlessandroAU/ExpressLRS/wiki/ExpressLRS-Videos)! :eye:

## Contributing

We would love [**you** to help](https://github.com/AlessandroAU/ExpressLRS/wiki#community) - please join the development fun. Thank you! :purple_heart:

## Support

Like our work? Feel free to donate to help support the project. 

[![Donate](https://img.shields.io/badge/Donate-PayPal-253B80.svg)](https://www.paypal.com/donate?hosted_button_id=FLHGG9DAFYQZU)


## Legal

👮 The use and operation of this type of device may require a license and some countries may forbid its use.
It is entirely up to the end-user to __ensure compliance with local regulations__.
This is experimental software and hardware without any guarantee of stability or reliability.
The software is not licensed and should be used with user discretion.