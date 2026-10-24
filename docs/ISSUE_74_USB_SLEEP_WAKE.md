# Issue #74: long-idle USB HostFS stall and recovery

## Confirmed failures

Twice, a physical PSP running PSPLinkUSB stopped answering `pspsh` after many
hours. macOS still enumerated Sony USB device `054c:01c9`; the Mac host daemon
remained alive, with normal memory usage and its TCP ports open. Thread samples
showed its main thread polling for USB bulk-IN data. macOS also entered sleep
and completed repeated DarkWake cycles; these events are correlated, **not a
confirmed root cause**.

The second failure was captured with an opt-in host USB trace:

```text
[USB trace 2026-10-24 23:53:05] async channel=0: submitted 14 bytes, transferred=-7
```

`-7` is `LIBUSB_ERROR_TIMEOUT`. The daemon received a TCP shell command, but
its USB bulk OUT endpoint `0x03` write to the PSP did not complete within ten
seconds. The Mac's bulk-IN timeout loop continued running and the existing
libusb device object remained enumerated. This does not prove whether the PSP
endpoint, host USB pipe, or host/device synchronization first became stuck.

## Controlled recovery observations

After the second *naturally occurring* hang, killing the host daemon without
cleaning up its USB interface, then launching a fresh host process, recovered
120/120 shell/HostFS commands **without USB re-enumeration or PSP reboot**.
The macOS USB session ID remained unchanged. Thus the PSP application was not
permanently deadlocked and a full reset is not always necessary.

However, a controlled **one-shot injected endpoint-3 timeout** exposed a
recovery distinction: gracefully releasing/closing and reclaiming the same USB
interface **without a device reset did not recover the handshake**. The host
repeatedly connected but could not establish a functioning PSPLink session.
Terminating a host process without cleanup is therefore **not equivalent** to
gracefully releasing a libusb interface. The existing `-N` switch remains
only for diagnosis; it is not recommended for production recovery.

## Experimental automatic recovery

The investigation branch implements a conservative host-side recovery path:

1. The async thread acquires the active libusb handle and increments an
   in-flight counter under a mutex. It releases the mutex before the actual
   bulk OUT write, so normal shell responsiveness is not slowed by a
   concurrent one-second bulk-IN wait.
2. A failed/short endpoint-3 write sets a reconnect flag; later async writers
   cannot acquire the failed handle.
3. The main thread checks this flag between its bounded bulk-IN reads, stops
   accepting new async transfers, and waits for existing writes to finish via
   a condition variable before touching the handle.
4. The main thread releases the interface, performs a **USB device reset**,
   closes the handle, and reconnects/handshakes. This reproduces the previously
   successful manual recovery without restarting either computer or PSPLinkUSB.

The existing endpoint-3 write timeout is ten seconds, and the idle bulk-IN read
normally has a one-second timeout. Recovery begins when a write error is
observed; **a command already in flight can be lost and may need retrying**.
Do not blindly replay non-idempotent commands.

This is a *recovery mechanism*, not an explanation or elimination of the
underlying long-uptime failure. It is host-only: the PSP firmware is unchanged.

## Validation

- `make -C tests test` passes native-C HostFS directory, USB device identity,
  and async recovery concurrency tests. The concurrency test holds an async
  writer active while another thread attempts to retire the handle, then
  checks that close/reset occurs only once it is safe.
- Normal Mac host daemon builds successfully; no new warnings beyond the
  existing unused `newpathlen` warning.
- A test-only binary built with `-DPSPLINKUSB_TEST_INJECT_EP3_TIMEOUT` injects
  **one** endpoint-3 `LIBUSB_ERROR_TIMEOUT`, without submitting that frame to
  the PSP. The first shell command correctly fails. With USB reset enabled,
  the daemon **automatically reconnects** and the native-C physical PSP test
  subsequently passes **120/120 commands** (6.35 s, p95 77 ms, max 85 ms).
  macOS observes USB re-enumeration as expected. The PSP application remains
  running. The same injection with `-N` did **not** recover successfully.
- The production build omits this injection; a separate 12/12 physical-PSP
  baseline passed after switching to the normal host binary.

Evidence from the original hang and both injected tests was preserved locally
under `~/Downloads/psplinkusb-issue74-*/`. See the GitHub issue for public
observations; logs may contain local host file paths and are not uploaded.

## Next required validation

Leave the non-injected, auto-recovery-capable host process attached to the
physical PSP during a normal long idle period, including a macOS sleep/wake
cycle if convenient. When it recurs, inspect whether the trace shows endpoint-3
failure followed by the automatic USB reset and reconnection. Then verify a
**new** shell and HostFS command succeeds without restarting the daemon or PSP.

If the host remains unresponsive despite the automatic recovery, capture the
USB device enumeration, daemon threads and trace **before resetting anything**.
That would justify targeted PSP-side endpoint/request-state instrumentation.
Do not resubmit PSP static `UsbdDeviceReq` structures after a timeout unless
request cancellation/completion and endpoint quiescence are confirmed.
