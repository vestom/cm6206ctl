# Description
Command line utility to control a CM6206 based USB sound card. Enables readout of all registers and enables control of special settings.

The program is made for Linux, but should be easily portable to Windows, if anybody would like to.

## Building

### Dependencies
- libhidapi-dev

### Command
```$ gcc cm6206ctl.c -l hidapi-libusb -o cm6206ctl```

## Running
The program requires access to /etc/hidrawXX devices, which are normally only accessible by root. Instead of running the program as root the device can be made accessible by other users.
```# echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="0d8c", ATTR{idProduct}=="0102", MODE="0666"' >/etc/udev/rules.d/50-cm6206.rules```