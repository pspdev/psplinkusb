# psplink-mcp

An [MCP](https://modelcontextprotocol.io) server that lets Claude (Claude Code or
Claude Desktop) drive a real PSP running PSPLink. Claude can run apps, read
their output, take screenshots, press buttons and reset the device.

```
Claude ──MCP stdio──> psplink-mcp ──TCP 127.0.0.1──> usbhostfs_pc ──USB──> PSP (PSPLink + remotejoy.prx)
                       10000 shell · 10002 stdout · 10003 stderr · 10004 remotejoy
```

The folder `usbhostfs_pc` serves is the PSP's `host0:`. To push an app, build it
into that folder; `psp_run` then loads it straight from there over USB.

## Setup (macOS / Linux)

Run these from the repository root.

1. **Host tools**: `make -C usbhostfs_pc` (needs libusb-1.0: `brew install libusb`).
   `make -C pspsh` is optional; it's handy for poking around by hand.
2. **remotejoy.prx**, needed for button input. Build it with the PSPSDK on your `PATH`:
   ```sh
   make -C libpsplink_driver && make -C libusbhostfs_driver && make -C tools/remotejoy
   ```
   The server copies `tools/remotejoy/remotejoy.prx` into `<root>/.psplink-mcp/`
   and loads it the first time an input tool is used.
3. **PSP**: start PSPLink on the PSP and connect the USB cable.
4. **usbhostfs_pc**: run it in its own terminal, giving it the folder to serve as
   `host0:` (it defaults to its working directory):
   ```sh
   /path/to/psplink/usbhostfs_pc/usbhostfs_pc ~/psp-work
   ```
5. **Register the server** with Claude Code. `PSPLINK_ROOT` must be the same folder:
   ```sh
   claude mcp add psplink --env PSPLINK_ROOT=$HOME/psp-work -- \
     uv run --directory /path/to/psplink/tools/psplink-mcp psplink-mcp
   ```
   The repository's `.mcp.json` does the same thing for sessions opened in this
   repository, using `${PSPLINK_ROOT}` from your environment.

Then ask Claude to "check the PSP status". It should report the PSPLink version.

## Tools

| Tool | What it does |
|---|---|
| `psp_status` | Shows channel connections, the PSPLink version, loaded modules and whether remotejoy is loaded. |
| `psp_shell(command, timeout_s)` | Runs any PSPLink shell command (`thlist`, `modlist`, `meminfo`, `peekw addr`, `exprint`, `ls`…). |
| `psp_run(path, args, wait_s)` | Loads and starts a `.prx`/`.elf` from `host0:` (or any PSP path), then returns the load result plus stdout, stderr and exception text. |
| `psp_output(clear)` | Returns stdout, stderr and unsolicited shell text received since the last read. |
| `psp_screenshot(scale, pri)` | Takes a 480×272 PNG of the current framebuffer, using PSPLink's `scrshot`. |
| `psp_press(buttons, hold_ms)` | Presses a chord of buttons: `cross circle square triangle up down left right ltrigger rtrigger start select`. |
| `psp_buttons(down, up)` | Holds or releases buttons, for presses that last across calls. |
| `psp_analog(x, y, duration_ms)` | Moves the analog stick (0–255, 128 is centre), then recentres it. |
| `psp_input_sequence(steps)` | Runs several `press`/`wait_ms`/`analog` steps in one call. |
| `psp_reset(timeout_s)` | Resets PSPLink, which unloads the app, and waits for the PSP to answer again. |

## Options

| Flag | Env var | Default | Meaning |
|---|---|---|---|
| `--root` | `PSPLINK_ROOT` | none | Folder usbhostfs_pc serves as `host0:`. Screenshots and remotejoy need it. |
| `--host` | `PSPLINK_HOST` | `127.0.0.1` | Host running usbhostfs_pc. |
| `--base-port` | `PSPLINK_BASE_PORT` | `10000` | Matches `usbhostfs_pc -b`. |
| `--remotejoy` | `PSPLINK_REMOTEJOY` | `tools/remotejoy/remotejoy.prx` | Where to find remotejoy.prx. |

## Limits

- usbhostfs_pc accepts **one client per channel**, and a new client kicks out the
  old one. Don't run `pspsh` while the server is connected; use `psp_shell` instead.
- pspsh-only commands (`asm`, `help`, `set`, `echo`, `symload`, `tty`…) aren't
  available, because they run on the PC side of pspsh.
- remotejoy can't send HOME/NOTE/volume/screen keys and doesn't support the latch
  functions (`sceCtrlReadLatch`). Apps that read input only through latches won't see presses.
- To run an app again, call `psp_reset` first, the same as with pspsh.
- `psp_run` loads modules with `sceKernelLoadModule`, so it can't run an `EBOOT.PBP`,
  and on 6.xx firmware it can't run a static ELF either (`0x80020148`). Build the app as
  a PRX (`BUILD_PRX=1` in a Makefile, or `-DBUILD_PRX=ON` / `BUILD_PRX` in
  `create_pbp_file` with CMake) and run the `.prx`.

## Development

```sh
uv run pytest
```

The tests run against `tests/fake_usbhostfs.py`, which speaks the same wire
formats as usbhostfs_pc and PSPLink. No device is needed.
