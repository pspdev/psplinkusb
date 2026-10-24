# Issue #74: USB HostFS sleep/wake diagnosis

The long-uptime failure was reproduced on a real PSP: \`pspsh\` timed out even
though macOS still enumerated the device and the host daemon still listened on
its TCP ports. The host main thread was waiting for a USB bulk-IN transfer.
macOS had entered sleep and performed repeated maintenance wake cycles.

This **does not establish sleep as the cause**. The pre-failure PSP-side USB
request state was not captured. See GitHub issue #74 for the original evidence.

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
