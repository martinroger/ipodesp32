[![PlatformIO CI](https://github.com/martinroger/ipodesp32/actions/workflows/platformIO.yml/badge.svg)](https://github.com/martinroger/ipodesp32/actions/workflows/platformIO.yml)

# iPodESP32 : an iPod emulator for 2nd gen Minis

## Summary

**Second generation Minis (R56, R57, R58...) did not come with Bluetooth music streaming unless the Visual Boost or Mini Connected navigation option was selected (which wasn't hugely popular). Some of the later Minis (usually called LCI, or facelift Mini) started shipping with iPod support in the form of an USB receptacle, a 3.5mm AUX input and a special "Y-Cable". This repository is an attempt at bridging that gap with an ESP32 and a bag of tricks.**

## How to identify compatible Minis

This solution is theoretically compatible with any facelift Mark II Mini, so usually with a manufacturing date after 2009 (varying a bit by market). This encompasses also Countryman minis and might even work on BMW vehicles, but remains untested.

An example of a 2013 post-facelift Mini R56 :

![A 2013 Mini Cooper S R56 GP2](/img/miniGP2.png)

The key requirements to make sure this can be used on a Mini are :

- A "Radio Boost CD" head unit, as pictured below, regardless of the availability of the Hands-Free Bluetooth Phone calls (option $6ND)
- The USB input and 3.5mm jack AUX input receptacle, usually installed forward of the cup holders. See below for visual references.
- A PC with a recent version of Windows or Linux
- An internet connection to clone this project, build it, flash it.
- A compatible ESP32 hardware... see the section below, loads are supported !
- No actual iPod or Y cable is needed !

![Radio Boost CD](/img/boostCD.png)

_A radio boot CD head unit, taken out of the center console_

![Mini R56 USB Aux connection block](/img/usbAux.png)

_The USB+Aux receptacle installed forward of the cupholders_

In theory these should be the only requirements, though it is possible that in some cases the car requires some coding to enable iPod support even despite the presence of the correct head unit and USB+AUX receptacle.

## Alternative solutions

Over the year, some companies (mostly in China) have developped similar devices, such as the ones sold by **[Bovee](https://a.co/d/5lkZTkV)** or **[Gitank](https://thegitank.com/products/gitank-bluetooth-5-0-aptx-hd-adapter-with-y-cable-for-bmw-and-mini-cooper-usb-aux-ipod-iphone-music-interface-300b)**. The general principle is the same : a BT speaker is running an A2DP profile, while sufficient iPod-like communication is performed with the Mini to make it think there is an actual iPod connected.

I have had a very mixed bag of results with those, and it appeared to be even worse when using an Android phone, as the Playback/Synchronisation/Metadata seemed to be mostly reserved to iPhone users.

## The iPodESP32 solution

Beyond the lame name, the solution proposed here does the following :

- Emulates an iPod Classic 5 - the U2 edition to be precise
- Passes song metadata from the phone/tablet (**Android or iOs**... or even others ?) regardless of what is playing the songs (Spotify remains the most tested to this date)
- Enables direct playback control from the radio interface, including steering wheel controls if present
- Auto-reconnects on ignition on, auto-pauses on ignition off
- Auto-pause and passes to whichever handsfree solution is present in case of phone call
- Provides many different interfaces for debug logging... because surely I didn't catch all them bugs
- Does not require any permanent modification of the car

## Supported base hardware setups

Because I could not find a one-size-fits-all solution (see [further technical details if you are curious](/doc/theoryOfOperation.md)), I devised some "standard" solutions. It is important to mention however, that this project's SW can easily be adapted to slight variations of the hardware. There are essentially two families of hardware that are implemented today, to support a variety of budgets : DAC-based, and CODEC-based. The performance difference between the two is barely noticeable.

### DAC-based hardware : ESP32 WROOM32 devkits, UDA1334A "Adafruit" DAC, CP2104 Serial interface

This is the most basic, and most "DIY"-looking solution. It is also one of the least compact but the cheapest to build and try out the functionality with reversible costs. The following hardware is required :

- An **ESP WROOM32 MCU-based** devkit board, the most commons being the "ESP32 DevKit C V4" for which there are [countless clones](https://www.az-delivery.de/it/products/esp-32-dev-kit-c-v4), or the [NodeMCU 32S](https://www.waveshare.com/nodemcu-32s.htm) . In general the hard requirements are that **BT Classic or BT EDR be supported**, that there is enough Flash space (over 2MB required, >=4MB advised). If you want to use the "Sandwich" carrier board to make things clean, be wary of use a compatible pin mapping. Please note : MCUs like the **ESP32-S3 that only support Bluetooth Low Energy are NOT SUPPORTED !!**
![Waveshare NodeMCU32S](/img/nodeMCU32s.png) 
- An "Adafruit" UDA1334 DAC breakout board. I do love the original Adafruit board, however they are difficult and expensive to get hands on in Europe, so I can also recommend like-for-like clones which are [quite available all over the place](https://www.amazon.com/AITRIP-CJMCU-1334-UDA1334A-Decoder-Arduino/dp/B09DG21C7G)
![UDA1334A DAC Decoder](/img/UDA1334A.png)
- A CP2104-based USB-UART interface, with one absolutely essential requirement : **the "DCD" (Device Carrier Detect) pin MUST be accessible**. My personal favourite is the very commonly found interface usually stamped "CNT-003B", which has the good taste of also being compatible with the "Sandwich" carrier board (if used). Please note : after modifying the VID and PID on the UART interface, it will NOT be usable without reprogramming in other applications.
![CNT-003B](/img/CNT003B.jpg)
- Optionally, I have designed a cheap PCB carrier called the "Sandwich" board, which can be used for doing the right connections and keeping things tight. You can also use jumper cables and prototyping boards first !
- A handful of 2.54mm header pins, if not supplied with the various boards.

Please note the following : 
- Usually the USB-UART comes with horizontal 2.54mm headers already soldered (as in the picture). If you want to use the sandwich board, **you will have to unsolder them**. In all cases it is recommended to solder header rows to both lines of 6pins on the sides, for easier mounting in the carrier board.
- Using the [software provided by Silabs in AN220](https://www.silabs.com/documents/public/example-code/AN220SW.zip) (the makers of the UART chip), you will need to edit the VID and PID of the chip to emulate a Prolific PL2303HXA. This means editing the Vendor ID to 0x067b and the Product ID to 0x2303 (see further for screenshots and a step-by-step). This is not irreversible, but may cause a BSOD in Windows, and is [hard to reverse without Linux access](https://blog.manzelseet.com/fixing-cp2102-with-custom-vidpid.html).
- Variants of this solution are easy to do with different DAC units, such as the popular PCM5102 breakout board. However, for those no sandwich is provided ...

Respect where is due to the underlying libraries needed for this sort of build : 
- [AudioTools](https://github.com/pschatzmann/arduino-audio-tools)
- [ESP32-A2DP library](https://github.com/pschatzmann/ESP32-A2DP)

For complete and detailed assembly instructions, please refer to **this Wiki page**.

---

---

---
## How does it work ?
### General aspects

The iPodESP32 relies primarily on three specific aspects :
-  **A2DP Bluetooth streaming to an I2S DAC** : this is done using the most excellent combination of [pschatzmann's ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) library, which provides an Arduino-compatible wrapper of the ESP A2DP API and links it with his [AudioTools](https://github.com/pschatzmann/arduino-audio-tools) library, which allows for advanced I2S audio streaming control to a DAC.
-  **AVRCP Control of the "target" over Bluetooth**, as in Playback control of the target Phone : this is integrated in the [ESP32-A2DP library](https://github.com/pschatzmann/ESP32-A2DP) and allows to control playback and fetch metadata from the source phone
-  **The iPod-emulating esPod class**, which is available in this repository, that provides a limited implementation of Apple's iAP accessory communication protocol, from the point of view of an iPod Classic 5.5.
This offers a packet-parsing method, processing loops and a bunch of properties to monitor and act upon the "state" of this "iPod" and allow it to control and receive commands for playback. This is the bit of code I developped.

All of this fancy code is living on an **ESP32 WROOM32 MCU**, and for the sake of easy sourcing, in my case a *[NodeMCU 32S](https://www.waveshare.com/nodemcu-32s.htm)*. Please do not that alternative ESP MCUs might work as long as they have enough Flash space (about 2MB), RAM (320kB or more) and processing power... on top of supporting Bluetooth Classic (**so no ESP32-S3 for example**).



The I2S stream generated from the ESP32 goes to a DAC chip (in my case an *UDA1334A*, but others are usable) to generate an audio signal on the aux Jack, and the Car <-> ESP32 interfacing is done through a very specific Serial interface chip (more on that below).

### Operating sequence

1.  **The iPodESP32 boots up** when power comes in the USB port of the car, and waits silently for a device to connect with it over Bluetooth.
2.  **Once a device connects to the iPodESP32**, it starts listening to the handshake packets coming from the vehicle and responds favourably to them, signalling that an iPod 5.5 is connected and ready, with Shuffle Off and the first track loaded. This makes the iPod option available in the media sources.
3.  **Once the audio source is pointing to the iPod on the Mini**, the iPodESP32 attempts to start playback, which usually launches the default media player, and fetches the current song metadata. From the perspective of the vehicle this means that there should be an audio stream going to the Aux jack and music should be playing.
4.  **When a song ends** and the list moves to the next song, the iPodESP32 attempts to fetch the new metadata and signals to the Mini that a new song is playing. This usually works OK and the displayed data is updated.
5.  **When a track is skipped** - backwards or forwards - via the Mini head unit, the iPodESP32 tries to reproduce the correct command on the phone via AVRCP. This usually leads to a successful load of the new track metadata too.
6.  **When the car is stopped** or the iPod mode is left, the iPodESP32 pauses playback on the phone and resets itself.

## What is needed ?
### PL2303-based USB-Serial interface

Unfortunately BMW's head unit doesn't seem to **natively** recognise the most widely available USB-Serial chips like the *FTDI 232* or the *CH340* and other variants, possibly because of trade deals and of their release dates.

Most if not all the Y-cables used to connect iPods to the Mini use a **PL2303HX chip** to let the iPod communicate over Serial with the car.

Working USB-Serial chips need to report the VID and PID of the **PL2303HX** converter to be recognised and unfortunately it is difficult to source in small quantities. Additionally this has been a widely copied chip so there is a host of fakes available on the Internet. These should just as fine as the real ones, but they are obviously not the real deal.

Hardcore DIY makers can get a PL2303G interface board like the [one manufactured by Waveshare](https://www.waveshare.com/product/pl2303-usb-uart-board-type-c.htm?sku=20265) and edit the OTPROM (only once) with the PL2303HX VID and PIDs, but PL2303HX alternatives from [Amazon/Aliexpress](https://a.co/d/a89Kzf7) and consorts might work just as well without trying to find a way to edit the VID and PID.

![PL2303 Converters](/img/PL2303.png)

### A BT Classic ESP32 board

My personal choice in that case was to use a **NodeMCU 32S** based on the WROOM32, which is readily available in many shapes and forms and can be bought in many outlets at a very cheap price.
These usually feature largely enough RAM and Flash to host the app and run without stuttering.

Other boards, such as the **[Sparkfun Thing Plus](https://www.sparkfun.com/products/20168)**, will work, as will some of the **[DevKit C V4](https://www.az-delivery.de/it/products/esp-32-dev-kit-c-v4)** from AZ-Delivery.

**Please note : ESP32-S3 do not support BT classic, and therefore WILL NOT WORK.**

![SparkFun Thing Plus](/img/SparkfunThingPlus.png)

### An external DAC breakout board

Please note, because of deprecation issues with ESP's A2DP-I2S implementation, this project uses now [pschatzmann's Arduino Audio Tools lib](https://github.com/pschatzmann/arduino-audio-tools) for the A2DP-I2S implementation. It works just the same with some syntax adjustments.

Before I may get around to providing an all-in-one board, the best solution is to use an external DAC with a 3.5mm barrel jack output. The possible models are described, along with some configuration hints, on [pschatzmann's ESP32-A2DP library wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/External-DAC) pages.

In my case I went for a **UDA1334A**, which is really a copy of a board apparently originally sold by Adafruit, and is [quite available all over the place](https://www.amazon.com/AITRIP-CJMCU-1334-UDA1334A-Decoder-Arduino/dp/B09DG21C7G).



### Jumper wires

For the *McGyver style*, a handful of jumper wires can suffice to connect all this hardware together.

*I may provide a base PCB at a later point to provide a "standard" footpring and cleaner set-up, but it is not an absolute necessity.*  

## What is supported ?

At the time of writing, this is what is supported :
- Auto-reconnect to the phone
- iPod emulation to the car
- Skip FW/RW and restart song
- Metadata update, sometimes with a couple small issues (but less and less)
- Auto-pause on leave

Some features are also partially supported but known to generate some bugs or unintended effects :
- Shuffle mode does not affect the Shuffle state of the phone, and can lead to some issues with Next/Previous commands needing to be alternatively activated and metadata fetching delays
- Controlling playback from the phone (Pause/Play, Next/Previous) should work but can generate synchronisation issues
- Repeat tracks control is essentially ineffective
- Metadata update can sometimes need a bit of encouragement by pressing the "Track" button on the Mini, though there is a patch coming for that
- Sometimes starting can be laggy or paused with Spotify if there is a no data connection available on the mobile phone. This is unfortunately an issue with online streaming. Forcing "play" on the phone, or skipping to the next track on the phone usually resolves everything.

Some features that seem to be there but are actually not doing what they are originally intended to :
- Artist/Genre/Album browsing and selection : it will display only one entry to the current song and will not allow dynamic selections
- Playlist selection : this is a limitation of how AVRC is often implemented on phones and in the ESP API : it is not possible to browse for playlists from the Head Unit
- Non ASCII track metadata will generate garbage on the display (but play fine)

## Remaining work to do

- [x] Work on improving the reliability of the metadata fetching and displaying, especially on song changes
- [x] Revise the Previous/Next detection, especially when performed directly on the phone
- [ ] Extend the code to more external DACs
- [ ] Design and get manufactured a all-in-one PCB, for the clean looks of it
- [ ] Design a 3D-printable enclosure
- [ ] Look into using more modern transmission Codecs like AptX

## How to flash it

This repository is built around a **Platformio project and VSCode**, which allows automatic pulling of the right libraries, at the right version, from the right places, and good auto-configuration based on a couple build flags.

### For those who know

For the recommended hardware configuration, the simplest is to **clone this repository**, open it in Platformio (in VS Code for instance), connect the ESP32 board (without the DAC or the USB-Serial interface) and flash it like any other ESP32 bit of software.

For more exotic configurations, the build flags may need to be updated and the *main.cpp* file may also require some fiddling. I may provide some pre-made configurations in the future.

Please be aware that it is not currently possible to flash via the PL2303HX because it is not connected to the UART0 pins of the ESP32. Use USB CDC or the embedded USB-UART chips on the ESP32 devboard to flash via the main UART.

### For those who do not know

First start by installing [Visual Studio Code](https://code.visualstudio.com/) and from within its Extensions interface, [PlatformIo IDE](https://platformio.org/).
Once they are both installed, download [the latest release](https://github.com/martinroger/ipodesp32/releases/latest) or clone the repository on your computer.
Unzip the release file and from within the folder, right-click and choose "Open with Code" as shown below.

![Open with Code](/img/openWithCode.png)

The PlatformIO extension should automatically recognize and configure the project (which might take a few minutes). Once it is ready, select the build configuration you need (most likely *nodeMCUESP32S_externalDAC* ) and hit the "Build and Upload" arrow in the bottom bar, after connecting your NodeMCU32S board to the computer and letting it install the drivers.

![Build and Upload](/img/selectAndUpload.png)

VS Code should automatically find the target board and upload the software to it. After this, disconnect it, remount/reconnect as below, and look over Bluetooth for iPodEsp 2.

## Resources for a clean build

To be completed when a clean build exists... 

### Connection diagram

For a NodeMCU32S or a Devkit C V4, the shape of the board may change but the pin numbers remain the same. The view and schematics are looking like so :

![DevKit C V4 Routing](/img/routingDevKitCV4.png)

![Devkit C V4 Scheme](/img/SchemeDevKitCV4.png)

Pins numbers may change according to the DAC being used or the board. For the case of a Adafruit-style UDA1334A, the table is as follows :

|UDA1334A|Pin number on DevKit board |
|--|--|
| WSEL | 25 |
| DIN | 22 |
| BCLK | 26 |
| VIN | 3V3 or 5V |
| GND | GND |

And on the PL2303 interface board :
|PL2303|Pin number on DevKit board|
|--|--|
|TX|RX0|
|RX|TX0|
|VIN|5V or VIN|
|GND|GND|

### Enclosure

To be designed.
