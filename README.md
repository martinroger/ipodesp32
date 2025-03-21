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

Because I could not find a one-size-fits-all solution (see [further technical details if you are curious](https://github.com/martinroger/ipodesp32/wiki/Theory-Of-Operation)), I devised some "standard" solutions. It is important to mention however, that this project's SW can easily be adapted to slight variations of the hardware. There are essentially two families of hardware that are implemented today, to support a variety of budgets : DAC-based, and CODEC-based. The performance difference between the two is barely noticeable.

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

![Sandwich board render](/img/sandwichBoard.png)

Please note the following : 
- Usually the USB-UART comes with horizontal 2.54mm headers already soldered (as in the picture). If you want to use the sandwich board, **you will have to unsolder them**. In all cases it is recommended to solder header rows to both lines of 6pins on the sides, for easier mounting in the carrier board.
- Using the [software provided by Silabs in AN721SW](https://www.silabs.com/documents/public/example-code/AN721SW.zip) (the makers of the UART chip), you will need to edit the VID and PID of the chip to emulate a Prolific PL2303HXA. This means editing the Vendor ID to 0x067b and the Product ID to 0x2303 ([see further for screenshots and a step-by-step](https://github.com/martinroger/ipodesp32/wiki/Editing-the-CP210x-VID-and-PID)). This is not irreversible, but may cause a BSOD in Windows, and is [hard to reverse without Linux access](https://blog.manzelseet.com/fixing-cp2102-with-custom-vidpid.html).
- Variants of this solution are easy to do with different DAC units, such as the popular PCM5102 breakout board. However, for those no sandwich is provided ...

Respect where is due to the underlying libraries needed for this sort of build : 
- [AudioTools](https://github.com/pschatzmann/arduino-audio-tools)
- [ESP32-A2DP library](https://github.com/pschatzmann/ESP32-A2DP)

For complete and detailed assembly instructions, please refer to [**this Wiki page**](https://github.com/martinroger/ipodesp32/wiki/DIY-DAC%E2%80%90based-solution-(UDA1334A)).

### CODEC-based hardware : AiThinker ESP A1S AudioKit (+ recommended "Top Hat") board

**At this date, this is the recommended option for people wanting a "clean" solution**

The ESP32-WROOM and ESP32-WROVER modules have been transformed in audio-specific modules by Ai-Thinker called the ESP32 A1S. It is now easily found pre-assembled on a nifty audio development board usually called "ESP32 AudioKit", with a few different revisions available on popular Chinese websites.

![ESP32 A1S AudioKit](/img/esp32A1SAudioKit.jpg)

The board is quite well made and is supported by another awesome library called [arduino-audio-driver](https://github.com/pschatzmann/arduino-audio-driver) that integrates perfectly with [AudioTools](https://github.com/pschatzmann/arduino-audio-tools) and [ESP32-A2DP library](https://github.com/pschatzmann/ESP32-A2DP). Two issues remain with the board however : the DCD pin of the onboard CP2102 UART chip is not easily accessible (and not grounded) and the interface itself is sensitive to fluctuations in the 5V supply of the USB, which I have observed to happen, because of how it is wired. 

To remedy those hardware shortcomings, two options exist : 
- Combining an LDO breakout board (5V->3V3, recommended 0.8A output and some ESD protection diodes ) such as [the ones based on the ever popular AMS1117-3V3](https://a.co/d/54oFsbZ) or even some switched break-out 3V3 power supplies, as long as their output voltage is relatively "clean", with a CP2102 or CP2104-based USB-UART breakout board **with accessible DCD pin**. A bit of soldering and/or wires is necessary, [correct connections are available on this wiki page.](https://github.com/martinroger/ipodesp32/wiki/AudioKit---floating-LDO-and-UART-interface)
- Buying/Copying the provided "Haut-de-forme" board (Top-Hat, in French) which essentially integrates a CP2104 UART interface with an LDO in a convenient form factor that mounts quite low on top of the AudioKit board. Full instructions are available on [this wiki page.](https://github.com/martinroger/ipodesp32/wiki/AudioKit-and-Haut%E2%80%90de%E2%80%90Forme-board)

![LDO breakout board](/img/LDOBoard.jpg)

_A common AMS1117 LDO breakout board_

![Haut de forme board](/img/hautDeForme.png)

_A render of the Haut de Forme board_

In any of the scenarios it will be necessary to edit the VID and PID on the UART interface in order for it to be recognized by the car. [Same instructions apply as for the more DIY solution.](https://github.com/martinroger/ipodesp32/wiki/Editing-the-CP210x-VID-and-PID)

To note : there is the possibility of modifying the board itself by uncovering some of the solder mask near the DCD pin of the CP2102 and trying to hand solder a bridge or an enameled wire. It is quite difficult to do as the package is a QFN2x, and issues can still persist if there are inconsistencies in the grounding or if the USB supply on the car is a bit fluctuating, as the UART chip is **not powered from the onboard LDO**. There is no easy palliative for that shortcoming, so it may work inconsistently (in my experience at least).

### CODEC-based hardware : A1S Mini Board AiO

The Audiokit board features a large amount of unused hardware (AMP outputs, microphones, line-in) for this application, and has the aforementioned circuitry shortcomings. In order to propose an All-in-One solution, I designed the "A1S Mini Board" which is essentially a remix of the AudioKit hardware, featuring the following :
- 2 USB-UART converters : one for communication with the car (and editable VID/PID) and one for programming/updating.
- A 3.5mm audio jack for connection to the AUX output

Unfortunately, because the ESP32 A1S module is currently not in production anymore, the board comes without it and requires sourcing and soldering one separately, until I can find a different solution with the PCB supplier. Limited batches may be produced if there is interest !

![A1S Mini board](/img/a1sMini.png)

_A render of the A1S Mini board_


## How to flash the firmware

This repository is built around a **Platformio project and VSCode**, which allows automatic pulling of the right libraries, at the right version, from the right places, and good auto-configuration based on a couple build flags.

### For those who know

For the recommended hardware configuration, the simplest is to **clone this repository**, open it in Platformio (in VS Code for instance), connect the ESP32 board (without the DAC or the USB-Serial interface) and flash it like any other ESP32 bit of software.

For more exotic configurations, the build flags may need to be updated and the *main.cpp* file may also require some fiddling. I may provide some pre-made configurations in the future.

Please be aware that it is not possible to flash using the UART with the modified PID and VID, because it will not be recognised properly.

### For those who do not know

First start by installing [Visual Studio Code](https://code.visualstudio.com/) and from within its Extensions interface, [PlatformIo IDE](https://platformio.org/).
Once they are both installed, download [the latest release](https://github.com/martinroger/ipodesp32/releases/latest) or clone the repository on your computer.
Unzip the release file and from within the folder, right-click and choose "Open with Code" as shown below.

![Open with Code](/img/openWithCode.png)

The PlatformIO extension should automatically recognize and configure the project (which might take a few minutes). Once it is ready, select the build configuration you need ([more info here](https://github.com/martinroger/ipodesp32/wiki/Flashing-the-firmware) ) and hit the "Build and Upload" arrow in the bottom bar, after connecting your NodeMCU32S board to the computer and letting it install the drivers.

![Build and Upload](/img/selectAndUpload.png)

VS Code should automatically find the target board and upload the software to it. After this, disconnect it, remount/reconnect as below, and look over Bluetooth for iPodEsp or MiniPod.

## How to use it, and some tips

Although it should be pretty straightforward at this stage, to use this, you need to connect the USB with the modified VID/PID to the USB receptacle of the car, and the audio jack to the AUX input (with a male-male stereo cable, for example).

In its default configuration, the controller will wait for an established BT connection to a phone before it starts signalling to the vehicle that it is available. This is only necessary the first time a phone is connected to it, in other cases the controller will try to auto-reconnect.

Once the controller has established connection with the phone, it starts the handshake process with the head unit, and if all is good the Aux and USB options in the "Mode" source selector of the head unit are replaced by an "iPod" option. This normally takes a few seconds if the controller was just connected, so re-check after 10s if the iPod option is not available.
If it still doesn't come up, in some occasions it is necessary to disconnect the USB cable, wait 5-10 seconds and reconnect it and wait for the BT pairing to go through (check your phone!).

The Mini will try to automatically start playback once the iPod option is selected. Depending on your phone configuration, it may automatically open your favourite music player and start playing. In some cases it might be necessary to manually open a music player for things to get started. At this stage the display should update with the track info.

When you leave the car, the USB gets shut down after about 2 minutes (no risks of draining the battery) and normally the Mini negotiates a playback pause as the ignition is turned off... 
