PSPLINK allows you to start and debug homebrew for the Playstation Portable through USB. This can speed up development and make finding out what is causing a crash or bug easier. This page explains how to set it up and.

# Getting started

## Install the PSPDEV toolchain

To get PSPLINK up and running, first the PSPDEV toolchain will need to be installed. This should contain the tools for the PC. So follow the instructions [here](https://pspdev.github.io/) first!

## Setup

Each system involved in the use of PSPLINK requires a bit of setup for it to work. This includes both PSP and PC. Below are instructions for both.

### PSP

Download the latest version of PSPLINK for the PSP [here](https://github.com/pspdev/psplinkusb/releases/download/latest/psplink.zip) and extract it in ``ms0:/PSP/GAME`` on the PSP memory card.

### PC

Depending on the operating system used the setup on PC is different. Follow the on below which is relevant to your system.

#### Windows

On Windows a driver needs to be installed before PSPLINK can be used. To do this take the following steps:

1. Make sure the programs ``usbhostfs_pc`` and ``pspsh`` are available from the msys2 terminal. Otherwise download them [here](https://github.com/pspdev/psplinkusb/releases/download/latest/pspsh-windows.zip).
2. Start PSPLINK on the Playstation Portable and connect it to the computer through USB.
3. Download [Zadig](https://zadig.akeo.ie/) and start it. It will ask if you want to run it as administrator, click yes.
4. In Zadig, click on ``options`` -> ``List All Devices``.
5. Select the entry ``"PSP" type B`` from the dropdown list.
6. Left of driver, select the ``libusb-win32`` driver. Then click install.
7. Wait for the installation to finish, then disconnect the USB cable from the PSP.

Now PSPLINK can be used with Windows. See below how to do that.

#### Linux

With Linux PSPLINK will work without making any changes, but it will require using sudo for the ``usbhostfs_pc`` command. To make it work without sudo, a udev rule can be added.

To make using PSPLINK without sudo create file called ``/etc/udev/rules.d/50-psplink.rules`` (for example with ``sudo nano /etc/udev/rules.d/50-psplink.rules``) and add the following content:

```
SUBSYSTEM=="usb", ATTR{idVendor}=="054c", ATTR{idProduct}=="01c9", SYMLINK+="psp", MODE="0666"
```

Save this, in Nano this can be done with Ctrl+O and pressing enter. The run the following command:

```
sudo udevadm control --reload
```

Now PSPLINK can be used without sudo. See below how to do that.

## Using PSPLINK

To be able to use PSPLINK with Playstation Portable homebrew, the homebrew will need to be build into an unencrypted ``.prx`` file. This can be done by running CMake like ``psp-cmake -DBUILD_PRX=1 .`` or if you're using a Makefile by adding ``BUILD_PRX=1`` to it. The build the homebrew.

In the build directory, open a terminal and run the following program:

```
usbhostfs_pc
```

Keep this running!

Then open another terminal window and run the following there:

```
pspsh
```

Now we can simply start our homebrew on the PSP by running the following command in the pspsh window:

```
./myhomebrew.prx
```

Replace myhomebrew with the name of the ``.prx`` file which was generated.

When you're done with the current build, just run ``reset``, rebuild the homebrew and try again.

Options available can be found when using the ``help`` command, but here are some notable ones:

- ``scrshot screenshotname.bmp`` for taking a screenshot.
- ``exit`` for closing PSPLINK on the PSP.
- ``poweroff`` for shutting down the PSP.

## Debugging crashes

When a crash happens a crash log will be shown with a hint of what might have happened at the top and some additional info. If you wish to figure out where the crash happened, only the address is needed.

To figure out where the crash happened, open another terminal in the build directory and use the address shown by PSPLINK in the following command:

```
psp-addr2line -e myhomebrew address
```

Replace ``address`` with the actual adress and replace myhomebrew with the name of the elf file. This is **NOT** the ``.prx`` file and either has no extension or ``.elf`` depending on the build system used.

If no result is returned, make sure to build with the ``-g`` or``-g3`` option to make sure psp-addr2line knowns the function names and locations.

## Full manual

If you need any additional information, check out the [complete online manual](psplink_manual.pdf).

# License

 - (c) TyRaNiD 2005-2007
 - (c) Julian T 2005/2006
 - (c) Rasmus B 2006
 - (c) John_K 2005
 - (c) pspdev 2010-2020

PSPLINK is licensed under the BSD license, see LICENSE file for details.
