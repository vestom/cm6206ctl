# Description
Command line utility to control a CM6206 based USB sound card. Enables readout of all registers and enables control of special settings (e.g. internal mixer settings, SPDIF parameters etc.).

The program is made for Linux, but should be portable to Windows, if anybody would like to.

Screenshot:
```
$ ./cm6206ctl -A -v
Devices:
 [0001:000e:03] Serial: (null), Manufacturer: (null), Product: USB Sound Device
== REG0 ==
Raw value: 0x2004       (Reset value: 0x2000)
[15] DMA Master                            DAC                     {0="DAC", 1="SPDIF Out"}
[14:12] SPDIF Out sample rate              48 kHz                  {0="44.1 kHz", 2="48 kHz", 3="32 kHz", 6="96 kHz"}
[11:04] Category code                      0
[03] Emphasis                              None                    {0="None", 1="CD_Type"}
[02] Copyright                             Not Asserted            {0="Asserted", 1="Not Asserted"}
[01] Non-audio                             PCM                     {0="PCM", 1="non-PCM (e.g. AC3)"}
[00] Professional/Consumer                 Consumer                {0="Consumer", 1="Professional"}
== REG1 ==
Raw value: 0x3000       (Reset value: 0x3002)
[15] <Reserved>
[14] SEL Clk (test)                        24.576 MHz              {0="24.576 MHz", 1="22.58 MHz"}
[13] PLL binary search Enable              Yes                     {0="No", 1="Yes"}
[12] Soft Mute Enable                      Yes                     {0="No", 1="Yes"}
[11] GPIO4 Out Status                      No                      {0="No", 1="Yes"}
[10] GPIO4 Out Enable                      No                      {0="No", 1="Yes"}
......
```

## Building

### Dependencies
- libhidapi-dev

### Make
```$ gcc cm6206ctl.c -l hidapi-libusb -o cm6206ctl```

## Running

```
$ ./cm6206ctl -h
cm6206ctl: Utility to read and control registers of USB sound card with CM6206 chip
Build: Nov 26 2019 13:37:25

Usage: cm6206ctl  [-r <reg> [-m <mask>] [-w <value>]][other options]
Generic Options:
    -A            Printout content of all registers in decoded form
    -h            Print this help text
    -m <mask>     Binary mask for reading/writing only some bits (e.g. 0x8000) [default=0xFFFF]
    -q            Quiet. Only output necessary values
    -r <reg>      Register to read or write
    -v            Verbose printout
    -w <value>    Write value to selected register
Shortcut Options:
    -DMASPDIF     Set DMA master to SPDIF (equivalent to '-r 0 -m 0x8000 -w 0x8000')
    -DMADAC       Set DMA master to DAC (equivalent to '-r 0 -m 0x8000 -w 0x0000')
    -INIT         Initialize all registers to sane default values (same as Linux driver)

Examples:
 cm6206ctl -A -v                    # Printout content of all registers in verbose form
 cm6206ctl -r 0                     # Read content of register 0
 cm6206ctl -r 2 -m 0x6000 -q        # Read and only output value of mask bits (example is 'Headphone source')
 cm6206ctl -r 0 -w 0 0x8000 -m 0x8000    # Write 1 to bit 15 in register 0

Supported devices: (USB)
 ID 0d8c:0102 CM6206
```

### Access rights ###
The program requires access to USB HID devices, which are normally only accessible by root. Instead of running the program as root the device can be made accessible by other users.
```# echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="0d8c", ATTR{idProduct}=="0102", MODE="0666"' >/etc/udev/rules.d/50-cm6206.rules```

## Links

### Documentation
- [CMedia IC Description](https://www.cmedia.com.tw/products/USB20_FULL_SPEED/CM6206)
- [hidapi](https://github.com/libusb/hidapi)

### Misc
- [Sound card performance test](http://www.daqarta.com/dw_gguu.htm)

### Devices
- [Delock USB Sound Box 7.1](https://www.delock.com/produkte/G_61803/merkmale.html) (tested - works!)
- [LogiLink USB Sound Box 7.1](https://www.2direct.de/notebook-computer/adapter/usb-2.0/audio/433/usb-sound-box-7.1-8-kanal) (not tested)
- [Noname $10 5.1 USB Sound card](https://www.aliexpress.com/wholesale?SearchText=cm6206) (not tested)