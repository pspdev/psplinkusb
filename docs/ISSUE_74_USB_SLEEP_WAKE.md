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

## Third naturally reproduced hang: completed endpoint-3 writes, no response

On 2026-10-25 after approximately 14h 13m of uptime with the endpoint-error
recovery candidate running, `pspsh` became unresponsive again following
multiple macOS system-sleep / DarkWake cycles. The host still enumerated the
original PSP USB session and listened on the TCP shell port. Unlike the second
hang, **USB endpoint 0x03 transferred the entire 14-byte shell frame** both at
13:28:20 and 13:30:11 CET. The native-C shell echo timed out without any
response from the PSP. There were no USB transfer errors and thus the first
recovery implementation did not trigger. Host thread samples and logs were
saved under `~/Downloads/psplinkusb-issue74-freeze3-20261025/`.

This demonstrates two observable failure modes: the host-to-PSP USB request
may time out, or it may report success **without the PSP producing shell
output**. A successful host bulk transfer does not imply that the PSP's async
receive callback/command parser ran. Sleep correlates strongly with both,
but is not yet proven causal.

## Experimental `-W`: reconnect after macOS sleep

To address the successful-write/silent-shell mode without replaying possibly
non-idempotent commands, run the experimental host binary with `-W` (macOS
only). It compares `CLOCK_MONOTONIC` (advances through sleep) and
`CLOCK_UPTIME_RAW` (only advances while awake). When the delta shows more than
five seconds asleep, the main thread stops admitting new async writes, waits
for any in-flight writes to complete, and **resets and reconnects USB**. The
existing endpoint-3 error recovery remains active. `-W` cannot be combined
with diagnostic `-N` (which suppresses USB reset). Wake recovery is deliberately
opt-in and changes only the Mac daemon, not the PSP firmware.

`-W` detects system sleep, **including maintenance DarkWake cycles**. This
could cause periodic USB re-enumeration overnight, so review log frequency
before enabling the policy by default. The reset itself can interrupt a
command that was already running when the Mac suspended; commands are not
replayed automatically.

A test-only build with `-DPSPLINKUSB_TEST_INJECT_WAKE` simulated a 120-second
suspend immediately after the initial HostFS handshake. The daemon detected
the synthetic wake, automatically reset/re-enumerated the PSP and recovered
**120/120 real-PSP shell/HostFS operations** (6.73 s, p95 76ms, max 91ms).
The normal (non-injected) `-T -W -v` daemon then passed **12/12** followed by
**1,000/1,000** PSP operations (50.47 s, p95 77ms, max 91ms) with no errors.
All four C regression tests pass, including a new injected-clock test for
normal elapsed time vs sleep gaps.

The normal experimental daemon was started as PID 19673 on 2026-10-25 around
13:40 CET. The test injectors are compiled out of this normal binary. Before
claiming a fix, the next step is a **real macOS sleep and wake**, followed by
a bounded PSP shell and HostFS test. Then a longer soak is necessary. The
underlying PSP-side endpoint state across sleep is not yet known, and #74
must remain open.

## Overnight validation: real repeated DarkWake cycles (2026-10-26)

After the first real wake test, the user left the normal, non-injected
`usbhostfs_pc -T -W -v` host daemon and physical PSP running overnight.
At **07:53 CET on 2026-10-26**, the original host process PID **19673**
was still alive after **18h 13m** (RSS approximately **7.9 MiB**),
owned the Sony PSP USB interface, and still accepted shell connections.
A new native-C hardware smoke test passed **120/120** shell/HostFS commands
in **6.34s** (p95 **76ms**, maximum **82ms**) without restarting PSPLinkUSB,
the cable, or the host process.

The overnight trace records **28** wake-triggered reconnects, each followed
by successful USB re-enumeration/HostFS reconnection, and **two** bulk-IN
`LIBUSB_ERROR_IO` reconnect events (15:50:11 and 18:33:58 on October 25).
There were **zero** endpoint-3 async write failures in this period.
macOS power history corroborates repeated maintenance DarkWake cycles
throughout the night.

This is our **first successful real overnight workaround validation** and
supports the hypothesis that proactively refreshing USB after sleep is useful.
It does **not** establish the original root cause or that the link would
otherwise have failed during this particular overnight period.

**Remaining problem:** `-W` resets the entire PSP USB device on *every*
qualifying wake, including DarkWake maintenance periods, causing **28 USB
resets in approximately 18 hours**. This is more intrusive than desirable for
a production patch. Future work should distinguish maintenance DarkWake from
interactive full wake or use an application-level PSP liveness/HELLO roundtrip
before resetting, while avoiding unsafe retransmission of commands. Do not
promote `-W` to a production default yet. Keep issue #74 open.

Full local snapshots: `~/Downloads/psplinkusb-issue74-wake-recovery-20261025/overnight-20261026/`.
