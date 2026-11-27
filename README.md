# Introduction

PSPLINK allows you to start and debug homebrew for the Playstation Portable through USB. This can speed up development and make finding out what is causing a crash or bug easier. This page explains how to set it up and use it.

# Usage

The usage of psplinkusb was migrated, check it out [here](https://pspdev.github.io/debugging.html).

## Full manual

If you need any additional information, check out the [complete online manual](psplink_manual.pdf).

# Building from source

Requirement: [PSPSDK](https://github.com/pspdev/pspsdk)

Get it using git:

```bash
git clone https://github.com/pspdev/psplinkusb.git && cd psplinkusb
```

Build it:

```bash
make all
```

## Native client builds

`pspsh` and the RemoteJoy clients use the controller header from the installed
PSPSDK (located with `psp-config`). To build only the host clients without a PSP
toolchain, use an existing PSPSDK source checkout instead:

```bash
git clone https://github.com/pspdev/pspsdk.git ../pspsdk
make -f Makefile.clients PSPSDK_CTRL_DIR="$(cd ../pspsdk/src/ctrl && pwd)"
```

Only `pspctrl.h` is needed from that checkout; PSPSDK does not need to be built.
The standalone `usbhostfs_pc` build does not require the controller header.

# License

 - (c) TyRaNiD 2005-2007
 - (c) Julian T 2005/2006
 - (c) Rasmus B 2006
 - (c) John_K 2005
 - (c) pspdev 2010-2020

PSPLINK is licensed under the BSD license, see [LICENSE](LICENSE) file for details.
