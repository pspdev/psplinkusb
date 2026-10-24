# Issue #74: USB HostFS sleep/wake diagnosis

The long-uptime failure was reproduced on a real PSP: \`pspsh\` timed out even
though macOS still enumerated the device and the host daemon still listened on
its TCP ports. The host main thread was waiting for a USB bulk-IN transfer.
macOS had entered sleep and performed repeated maintenance wake cycles.

This **does not establish sleep as the cause**. The pre-failure PSP-side USB
request state was not captured. See GitHub issue #74 for the original evidence.

## Second reproduced hang and no-reset recovery (2026-10-24)

The diagnostic daemon was still alive after 10h 51m. The old USB device was
still enumerated (\`original device enumerated=1\`) while a \`pspsh\` shell echo
timed out. The host's **bulk OUT endpoint 0x03** write returned \`-7\`
(\`LIBUSB_ERROR_TIMEOUT\`). This is the first capture of the failed transfer:

\`\`\`text
[USB trace 2026-10-24 23:53:05] async channel=0: submitted 14 bytes, transferred=-7
\`\`\`

The original daemon was terminated with SIGKILL, deliberately avoiding its
\`libusb_reset_device()\` cleanup call. macOS kept the **same** PSP device
session ID \`118627976632\`. A new host process started with \`-T -N\`, reclaiming
the same device without explicitly resetting USB. Real PSP shell/HostFS
operations immediately recovered: first 3/3, then **120/120** native C smoke
commands passed (p95 62ms, maximum 76ms).

Thus **USB device reset and PSP reboot are not necessary for recovery**;
closing/reclaiming the host USB interface was enough. This makes stale host
interface/pipe state or a host/PSP endpoint synchronization problem more
plausible. Interface release/reclaim may still cause lower-level pipe reset.
The evidence does not prove which side caused the endpoint 3 stall.

The \`-N\` option omits the normal \`libusb_reset_device()\` during host close,
and is intended **only for controlled recovery comparisons**, not as a
production change. Both \`-T\` and \`-N\` are opt-in.

Candidate fix: when an async endpoint 3 write fails, request a safe main-thread
reconnect, coordinate with the writer to complete outstanding transfers, then
close and reclaim the interface without resetting the whole PSP USB device.
The existing global \`usbhdr\` has cross-thread lifetime races, so reconnect
must **never** close that handle while the async writer is still using it.

## Temporary host-only diagnostics

Build \`usbhostfs_pc\` and run it with \`-T\` for timestamped transport diagnostics:

\`\`\`sh
make -C usbhostfs_pc
usbhostfs_pc/usbhostfs_pc -T -v /path/to/host/root
\`\`\`

\`-T\` deliberately **does not change USB recovery behavior**. It records:

- Once per 30 seconds of idle bulk-IN timeouts, whether the original open
  \`libusb_device_handle\` refers to an object still in the current libusb device
  list (\`1\` yes, \`0\` no, \`-1\` enumeration failure).
- Errors from bulk-IN requests that end the main receive loop.
- TCP async channel writes, their intended lengths and their transfer results.
- Host USB handle shutdown/reconnect.

Device identity is based on libusb's *object pointer*, not VID/PID or USB bus
address, because re-enumeration may create a new device object with the same
identifiers while an obsolete handle still retains the old object. A \`0\`
result warrants a host reconnect investigation; it does **not** by itself
prove that PSP firmware is deadlocked.

The trace is printed to standard error, so capture it using a logfile.
Avoid verbose \`-vv\` in unattended overnight tests; it prints every transfer.
The installed unmodified \`usbhostfs_pc\` and PSP kernel module are not changed
by building the diagnostic binary.

## Manual sleep/wake test

1. With the PSP running PSPLinkUSB and the diagnostic host daemon connected,
   record the host PID, USB session identifier and a successful \`pspsh\` command.
2. Put the Mac to sleep **manually** for a short interval and wake it, leaving
   the PSP and USB cable untouched. Do not exit PSPLinkUSB.
3. Before resetting anything, capture the host log, USB enumeration and a
   bounded \`pspsh\` echo / HostFS listing.
4. Compare device identity, write results on endpoint 3 and whether the main
   bulk-IN loop resumes after waking.

If the PSP becomes unresponsive, collect process samples and USB state before
restarting the daemon. \`usbhostfs_pc\` calls \`libusb_reset_device()\` when
closing a USB handle: a daemon restart therefore also performs a USB reset.

Do not blindly retry PSP static \`UsbdDeviceReq\` structures after timeouts
unless prior request cancellation/completion and endpoint quiescence are
confirmed.

## Verification

\`make -C tests test\` uses mocked libusb device lists to test matching and
stale handles and enumeration failures, alongside the directory/HostFS
protocol regression test. No PSP is required.
