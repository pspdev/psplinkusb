PSPLINK allows you to start and debug homebrew for the Playstation Portable through USB. This can speed up development and make finding out what is causing a crash or bug easier. This page explains how to set it up and use it.

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

1. Make sure the programs ``usbhostfs_pc`` and ``pspsh`` are available in cmd. Otherwise download them [here](https://github.com/pspdev/psplinkusb/releases/download/latest/pspsh-windows.zip).
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

To be able to use PSPLINK with Playstation Portable homebrew, the homebrew will need to be build into an unencrypted ``.prx`` file. This can be done by running CMake like ``psp-cmake -DBUILD_PRX=1 .`` or if you're using a Makefile by adding ``BUILD_PRX=1`` to it. Then build the homebrew.

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

## Debugging with GDB

Sometimes debugging with the built in instruction level debugger is not enough, this is where
PSPLINK's GDB Server can come in handy.

Let's get started:

### Preparation

Prepare a separate terminal for `usbhostfs_pc`, `pspsh` and `psp-gdb`. Open all of them in the directory in which your compiled `.prx` and `.elf` files are located.

### 1. usbhostfs_pc

Run `usbhostfs_pc` on your terminal dedicated for `usbhostfs_pc` and you will see the `waiting for device...` status. 

Now start the PSPLINK app on your PSP and connect the USB cable. You should see the `connected to device` status in the terminal, which means success. 

**Do not close this terminal after that.**

### 2. pspsh

Run `pspsh` on your terminal dedicated for `pspsh` and you will see the `host0:/>`. Now run `debug file.prx`, and it will display something like this:

> You need to replace `file.prx`  with the file you need to debug. PRX file not ELF.

```sh
PSPLink USB GDBServer (c) 2k7 TyRaNiD
host0:/> Loaded host0:/<file.prx> - UID 0x0408A763, Entry 0x088040AC
```

It means the debuggee is succesfully loaded. You can type `reset` if there's something wrong with your GDBServer.

### 3. psp-gdb

Run `psp-gdb file -q` on your terminal dedicated for `psp-gdb` and you will see something like this:

> You need to replace `file` with an elf file you need to debug. They have the same name as your loaded `.prx` file in the pspsh.

```sh
Reading symbols from <filename>...
(gdb)
```
> `<filename>` is the name of your current debuggee.

then type the `target remote :10001` to connect to your GDBServer and you will see the gdb output something like this:

```sh
Remote debugging using :10001
_start (args=0, argp=0x0) at crt0_prx.c:103
103         if (&sce_newlib_nocreate_thread_in_start != NULL) {
(gdb)
```
This will display the `_start` routine, it means you succesfully connected and ready to the debug your app!

Here are a few useful commands for getting around in psp-gdb:
- `b` or `break` - for setting breakpoints
- `c` or `continue` - for resuming program execution until the next breakpoint or program completion
- `s` or `step` - for executing the current line and, if it contains a function call, step into that function
- `n` or `next` - for executing the current line, but if it contains a function call, step over it without diving into the function
- `f` or `finish` - for executing the remaining lines of the current function and return to the caller
- `bt` or `backtrace` - for getting stacktrace
- `p $var` or `print $var` - for displaying the value of specific variable
- `i r` or `info registers` - for displaying the contents of CPU registers
- `d` or `delete` - for deleting all breakpoints
- `q` or `quit` - for exiting from psp-gdb

You can type `help` for more information about the psp-gdb commands.

## Full manual

If you need any additional information, check out the [complete online manual](psplink_manual.pdf).

# License

 - (c) TyRaNiD 2005-2007
 - (c) Julian T 2005/2006
 - (c) Rasmus B 2006
 - (c) John_K 2005
 - (c) pspdev 2010-2020

PSPLINK is licensed under the BSD license, see LICENSE file for details.
