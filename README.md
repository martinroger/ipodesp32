[![PlatformIO CI](https://github.com/martinroger/ipodesp32/actions/workflows/platformIO.yml/badge.svg)](https://github.com/martinroger/ipodesp32/actions/workflows/platformIO.yml)

# iPodESP32 : an iPod emulator for 2nd gen Minis

*A picture here would be nice.*

## Summary (TL;DR)

Second generation Minis (R56, R57, R58...) did not come with Bluetooth music streaming unless the Visual Boost or Mini Connected navigation option was selected (which wasn't hugely popular). 
Some of the later Minis (usually called LCI, or facelift Mini) started shipping with iPod support in the form of an USB receptacle, a 3.5mm AUX input and a special "Y-Cable". 

This repository is an attempt at bridging that gap with an ESP32 and a bag of tricks to provide **BT music streaming, retaining track data on the display and radio/steering wheel playback controls.**

## What is needed

Aside from a [Mini with a compatible radio head unit](https://github.com/martinroger/ipodesp32/wiki/How-to-identify-compatible-Minis) : 

- For the [full-DIY build](https://github.com/martinroger/ipodesp32/wiki/DIY-DAC%E2%80%90based-solution-(UDA1334A)) :
  - An ESP32 (not S3!) that has Bluetooth Classic or EDR support
  - A DAC (UDA1334 or PCM5102) breakout board
  - A CP2102 or CP2104 breakout board with exposed DCD pin (no CP2102N)
  - Jumper wires
  - Possibly some smaller hardware (resistors, capacitors) and a soldering iron

- For the Sandwich Carrier Board build, [follow this article](https://github.com/martinroger/ipodesp32/wiki/Sandwich-Carrier-Board-build)
- For the ESP32 AudioKit Top Hat build, [follow this other article](https://github.com/martinroger/ipodesp32/wiki/AudioKit-and-Haut%E2%80%90de%E2%80%90Forme-board)
- For the fancy ones out there wanting a small ESP32 A1S Mini, [it's here](https://github.com/martinroger/ipodesp32/wiki/ESP32%E2%80%90A1S%E2%80%90Mini-build)
- For the fancy but alternative-minded ones who also want an All-In-One, [it's there](https://github.com/martinroger/ipodesp32/wiki/ESP32%E2%80%90AiO-build)

## Alternative solutions

Over the year, some companies (mostly in China) have developped similar devices, such as the ones sold by **[Bovee](https://a.co/d/5lkZTkV)** or **[Gitank](https://thegitank.com/products/gitank-bluetooth-5-0-aptx-hd-adapter-with-y-cable-for-bmw-and-mini-cooper-usb-aux-ipod-iphone-music-interface-300b)**. The general principle is the same : a BT speaker is running an A2DP profile, while sufficient iPod-like communication is performed with the Mini to make it think there is an actual iPod connected.

I have had a very mixed bag of results with those, and it appeared to be even worse when using an Android phone, as the Playback/Synchronisation/Metadata seemed to be mostly reserved to iPhone users. Playback controls were also hit-and-miss.

## The iPodESP32 solution

Beyond the lame name, the solution proposed here does the following :

- Emulates an iPod Classic 5 - the U2 edition to be precise
- Passes song metadata from the phone/tablet (**Android or iOs**... or even others ?) regardless of what is playing the songs (Spotify remains the most tested to this date)
- Enables direct playback control from the radio interface, including steering wheel controls if present
- Auto-reconnects on ignition on, auto-pauses on ignition off
- Auto-pause and passes to whichever handsfree solution is present in case of phone call
- Provides many different interfaces for debug logging... because surely I didn't catch all them bugs
- Does not require any permanent modification of the car

## External (essential) libraries used

Respect where is due to the underlying libraries needed for this to work : 
- [AudioTools](https://github.com/pschatzmann/arduino-audio-tools)
- [ESP32-A2DP library](https://github.com/pschatzmann/ESP32-A2DP)
- [Arduino Audio Driver](https://github.com/pschatzmann/arduino-audio-driver)

## How to use it, and some tips

Although it should be pretty straightforward at this stage, to use this, you need to connect the USB with the modified VID/PID to the USB receptacle of the car, and the audio jack to the AUX input (with a male-male stereo cable, for example).

In its default configuration, the controller will wait for an established BT connection to a phone before it starts signalling to the vehicle that it is available. This is only necessary the first time a phone is connected to it, in other cases the controller will try to auto-reconnect.

Once the controller has established connection with the phone, it starts the handshake process with the head unit, and if all is good the Aux and USB options in the "Mode" source selector of the head unit are replaced by an "iPod" option. This normally takes a few seconds if the controller was just connected, so re-check after 10s if the iPod option is not available.
If it still doesn't come up, in some occasions it is necessary to disconnect the USB cable, wait 5-10 seconds and reconnect it and wait for the BT pairing to go through (check your phone!).

The Mini will try to automatically start playback once the iPod option is selected. Depending on your phone configuration, it may automatically open your favourite music player and start playing. In some cases it might be necessary to manually open a music player for things to get started. At this stage the display should update with the track info.

When you leave the car, the USB gets shut down after about 2 minutes (no risks of draining the battery) and normally the Mini negotiates a playback pause as the ignition is turned off... 
