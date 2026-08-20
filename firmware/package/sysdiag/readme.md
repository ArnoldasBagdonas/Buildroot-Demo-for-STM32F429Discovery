# System diagnostics

`BR2_PACKAGE_SYSDIAG` adds the broader kernel and BusyBox configuration used
for debugging. It is independent of the examples and may be selected beside
the package being investigated.

Run the report with:

```sh
sysdiag
```

The report covers system and memory information, interrupts, clocks, device
nodes, platform devices, kernel messages, networking, GPIO, PWM, RTC, and bus
state. When the LTDC driver is active, it also reads the LCD clock and register
state.

Disable Sysdiag for normal images; other packages use the smaller common
kernel and BusyBox profiles and enable only their required facilities.
