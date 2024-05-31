# iPodESP32 : an iPod emulator for 2nd gen BMW Minis
## What is it for?
**TL;DR : This is an iPod emulator that creates a bridge between an older car head unit (typically one found on a LCI BMW Mini Cooper, aka "Boost CD") and a mobile phone, and allows streaming audio content over A2DP to the car while retaining the radio commands such as Next, Previous and Track metadata display.**

Second-generation BMW Mini Coopers (R56, 55, 57, 58...) usually did not come with Bluetooth music streaming capabilities, except for some that had the "Visual Boost" or "Mini Connected" packages selected. These are easily identifiable because they replace the large central speedometer with a touch-screen surrounded by a speedo ring.

In the case of "LCI" (aka facelift), post-2010 Mini Coopers and derivatives, a good portion of them came with the "Radio Boost CD" head unit, which in some cases may feature Hands-free Bluetooth, USB input and an Aux 3.5mm jack input (typically those with option $6ND).

A large number of thoses are capable of reading USB drives and the likes on the USB input, and in some cases they also can "control" good old 30-pins iPods, via a now-rare and often-faked "Y-cable". 
This normally allows access to a special audio source on the screen ("iPod") and provides some niceties like Playback control, metadata display and even Playlist/Artist/Album/Genre browsing.
Unfortunately, connecting any other device on this USB will usually not work, as it is not operating in Host mode and our modern phones are now too advanced anyways.

As a palliative, some companies like **Bovee** or **Gitank** created some similar types of emulators, that connect to the USB + Aux and appear on the phones as BT Speakers.
I have had a very mixed bag of results with those, and it appeared to be even worse when using an Android phone, as the Playback/Synchronisation/Metada seemed to be mostly reserved to iPhone users.

Taking this into account, I chose to combine together a USB <-> Serial interface connected to an ESP32 WROOM32 development board, hook this up to an external DAC with a 3.5mm audio jack socket, and finally to spin some code in order to make my Mini think that there is a very special type of iPod connected to it... the kind that doesn't exist ! 

**The objectives were** : 
- To retain control of the playback via the car's inputs (Next/Previous buttons, radio knob, steering wheel buttons)
- To be able to display some metadata on the screen so it doesn't look ugly
- To not modify anything irreversibly on the car
- To stream music from the phone to the car as seamlessly as possible, via BT

## How does it work ?

### General aspects
The iPodESP32 relies primarily on three specific aspects : 

- **A2DP Bluetooth streaming to an I2S DAC** : this is done using the most excellent combination of pschatzmann's ESP32-A2DP library, which provides an Arduino-compatible wrapper of the ESP A2DP API and links it with his AudioTools library, which allows for advanced I2S audio streaming control to a DAC. 
- **AVRCP Control of the "target" over Bluetooth**, as in Playback control of the target Phone : this is integrated in the ESP32-A2DP library and allows to control playback and fetch metadata from the source phone
- **The iPod-emulating esPod class**, which is available in this repository, that provides a limited implementation of Apple's iAP accessory communication protocol, from the point of view of an iPod Classic 5.5. 
This offers a packet-parsing method, processing loops and a bunch of properties to monitor and act upon the "state" of this "iPod" and allow it to control and receive commands for playback. This is the bit of code I developped.

All of this fancy code is living on an **ESP32 WROOM32 MCU**, and for the sake of easy sourcing, in my case a *NodeMCU 32S*. Please do not that alternative ESP MCUs might work as long as they have enough Flash space (about 2MB), RAM (320kB or more) and processing power... on top of supporting Bluetooth Classic (**so no ESP32-S3 for example**).

The I2S stream generated from the ESP32 goes to a DAC chip (in my case an *UDA1334A*, but others are usable) to generate an audio signal on the aux Jack, and the Car <-> ESP32 interfacing is done through a very specific Serial interface chip (more on that below).

### Operating sequence

1. **The iPodESP32 boots up** when power comes in the USB port of the car, and waits silently for a device to connect with it over Bluetooth.  
2. **Once a device connects to the iPodESP32**, it starts listening to the handshake packets coming from the vehicle and responds favourably to them, signalling that an iPod 5.5 is connected and ready, with Shuffle Off and the first track loaded. This makes the iPod option available in the media sources.
3. **Once the audio source is pointing to the iPod on the Mini**, the iPodESP32 attempts to start playback, which usually launches the default media player, and fetches the current song metadata. From the perspective of the vehicle this means that there should be an audio stream going to the Aux jack and music should be playing. 
4. **When a song ends** and the list moves to the next song, the iPodESP32 attempts to fetch the new metadata and signals to the Mini that a new song is playing. This usually works OK and the displayed data is updated. 
5. **When a track is skipped** - backwards or forwards - via the Mini head unit, the iPodESP32 tries to reproduce the correct command on the phone via AVRCP. This usually leads to a successful load of the new track metadata too. 
6. **When the car is stopped** or the iPod mode is left, the iPodESP32 pauses playback on the phone and resets itself.

## What is needed ?

### PL2303-based USB-Serial interface
Unfortunately BMW's head unit doesn't seem to recognise the most widely available USB-Serial chips like the *FTDI 232* or the *CH340* and other variants, possibly because of trade deals and of their release dates. 

Most if not all the Y-cables used to connect iPods to the Mini use a **PL2303 chip** to let the iPod communicate over Serial with the car. 
Working chips are the PL2303HX, PL2303GS and possibly some others... but unfortunately this has been a widely copied chip so there is a host of fakes available on the Internet. These should just as fine as the real ones, but they are obviously not the real deal.
For true DIY makers I would recommend getting a PL2303 interface board like the one manufactured by Waveshare, but alternatives from Amazon/Aliexpress and consorts might work just as well.

### A BT Classic ESP32 board
My personal choice in that case was to use a **NodeMCU 32S** based on the WROOM32, which is readily available in many shapes and forms and can be bought in many outlets at a very cheap price. 
These usually feature largely enough RAM and Flash to host the app and run without stuttering.
Other boards, such as the **Sparkfun Thing Plus**, will work, as will some of the **DevKit V4** from AZ-Delivery.

**Please note : ESP32-S3 do not support BT classic, and therefore WILL NOT WORK.**

### An external DAC breakout board
Before I may get around to providing an all-in-one board, the best solution is to use an external DAC with a 3.5mm barrel jack output. The possible models are described, along with some configuration hints, on pschatzmann's ESP32-A2DP library wiki pages. 
In my case I went for a **UDA1334A**, which is really a copy of a board apparently originally sold by Adafruit.

### Jumper wires
For the *McGyver style*, a handful of jumper wires can suffice to connect all this hardware together. 
*I may provide a base PCB at a later point to provide a "standard" footpring and cleaner set-up, but it is not an absolute necessity.*

## What is supported ?

At the time of writing, this is what is supported : 
- Auto-reconnect to the phone
- iPod emulation to the car
- Skip FW/RW and restart song, with some glitches
- Metadata update, sometimes with a couple small issues
- Auto-pause on leave

Some features are also partially supported but known to generate some bugs or unintended effects :
- Shuffle mode does not affect the Shuffle state of the phone, and can lead to some issues with Next/Previous commands needing to be alternatively activated and metadata fetching delays
- Controlling playback from the phone (Pause/Play, Next/Previous) should work but can generate synchronisation issues
- Repeat tracks control is ineffective
- Metadata update can sometimes need a bit of encouragement by pressing the "Track" button on the Mini

Some features that seem to be there but are actually not doing what they are originally intended to :
- Artist/Genre/Album browsing and selection : it will display only one entry to the current song and will not allow dynamic selections
- Playlist selection : this is a limitation of how AVRC is often implemented on phones and in the ESP API : it is not possible to browse for playlists from the Head Unit
- Non ASCII track metada will generate garbage on the display (but play fine)


## Remaining work to do
- [ ] Work on improving the reliability of the metadata fetching and displaying, especially on song changes
- [ ] Revise the Previous/Next detection, especially when performed directly on the phone
- [ ] Extend the code to more external DACs
- [ ] Design and get manufactured a all-in-one PCB, for the clean looks of it
- [ ] Design a 3D-printable enclosure
- [ ] Look into using more modern transmission Codecs like AptX

## How to flash it

This repository is built around a **Platformio project**, which allows automatic pulling of the right libraries, at the right version, from the right places, and good auto-configuration based on a couple build flags.

For the recommended hardware configuration, the simplest is to **clone this repository**, open it in Platformio (in VS Code for instance), connect the ESP32 board (without the DAC or the USB-Serial interface) and flash it like any other ESP32 bit of software. 

For more exotic configurations, the build flags may need to be updated and the *main.cpp* file may also require some fiddling. I may provide some pre-made configurations in the future.

Please be aware that if the PL2303 is connected via the GPIO pins to the UART0 interface, flashing may not work properly or start at all. 
It is preferable to disconnect the USB-Serial interface from the ESP32 board prior to flashing.

## Resources for a clean build

To be completed when a clean build exists...