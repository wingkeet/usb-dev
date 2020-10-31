# usb-dev

### Introduction
The goal of this repository is twofold: first, to document the steps required
to set up Ubuntu 20.04 LTS in order to develop user-mode C programs to
perform I/O on USB devices; and secondly, to demonstrate complete
programs that actually work. My test USB device is a [Microsoft Xbox
Wireless Controller](https://en.wikipedia.org/wiki/Xbox_Wireless_Controller)
(model 1708), which is connected to my Linux machine with a micro USB cable.

### Prerequisites
- These programs were developed and tested on Ubuntu 20.04 LTS.
- Superuser privilege is required.

### Install libusb development files
```
$ sudo apt install libusb-1.0-0-dev
$ sudo updatedb && locate libusb.h
```

I'm using libusb version 1.0.23, which was released on 29 August 2019.
To find out the version that you installed, use this command:
```
$ dpkg -s libusb-1.0-0-dev
```

### Create udev rule
Connect your Xbox controller to any of your computer's USB port. Then use
the `lsusb` command to find out its vendor ID and product ID. Create a
new udev rules file for your controller:
```
$ sudo nano /etc/udev/rules.d/40-microsoft.xbox.controller.rules
```

Here's the content of `/etc/udev/rules.d/40-microsoft.xbox.controller.rules`:
```
# Microsoft Xbox controller
ATTRS{idVendor}=="045e", ATTRS{idProduct}=="02ea", MODE="664", GROUP="plugdev"
```

Reload the rules files on your Linux system:
```
$ sudo udevadm control --reload
```

Finally, disconnect and then reconnect your controller.

Without this udev rule, the `libusb_open_device_with_vid_pid()` function
call in `gamepad.c` will fail:
```
libusb v1.0.23.11397 (http://libusb.info)
libusb: error [_get_usbfs_fd] libusb couldn't open USB device /dev/bus/usb/001/004: Permission denied
libusb: error [_get_usbfs_fd] libusb requires write access to USB device nodes.
Cannot open device
```

### Build programs
```
$ make
```

### Usage
Type `./gamepad` and play with the buttons on your controller. You should
see some text output. To activate rumble, press the A button. To exit
the program, press the X button. Below is a sample run of the program
when both the A button and the X button are pressed at the same time:
```
libusb v1.0.23.11397 (http://libusb.info)
Device opened
Port path: 1
libusb_set_auto_detach_kernel_driver() returned 0
Claimed interface
Read successful! 18 bytes: 2000270E5000000000002FFDF8FE2DFE8FF3
  type: 0x20
  const_0: 0x00
  id: 0x27
  length: 0x0E
  a,b,x,y: 1,0,1,0
  sync,menu,view: 0,0,0
  bumper left,right: 0,0
  stick left,right: 0,0
  dpad up,down,left,right: 0,0,0,0
  trigger left,right: 0,0
  lstick x,y: -721,-264
  rstick x,y: -467,-3185
Released interface
```

### License
The software contained herein is released into the public domain under the
[Unlicense](https://unlicense.org/). In layman's terms, there are no
restrictions and you are free to do whatever you want with it.

### References
- https://libusb.info/
- https://linuxconfig.org/tutorial-on-how-to-write-basic-udev-rules-in-linux
- https://www.dreamincode.net/forums/topic/148707-introduction-to-using-libusb-10/
- https://github.com/paroj/xpad
- https://github.com/quantus/xbox-one-controller-protocol
- https://docs.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad
- https://en.wikipedia.org/wiki/Xbox_One_controller
- https://support.xbox.com/help/hardware-network/controller/xbox-one-wireless-controller
