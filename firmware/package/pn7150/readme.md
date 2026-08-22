# NFC / NXP PN7150 demo

`BR2_PACKAGE_PN7150` installs `pn7150-demo`, a Linux port of NXP's SW4325
Cortex-M NCI/NDEF example for a PN7150 controller board. It resets and
initializes the controller, configures reader/writer discovery, reports the
first NFC-A, NFC-B, NFC-F, or NFC-V tag it sees, and decodes NDEF records.

PN7150 uses an I2C host interface, not SPI. There is no chip-select signal.
The additional signals are the active-high interrupt output `IRQ` and the
active-high enable/reset input `VEN`.

## Wiring

The assignments below avoid all onboard peripherals and all external devices
used by the other examples. Signal levels are 3.3 V.

| PN7150 signal | OM5578 terminal | STM32F429Discovery signal | Expansion header |
|---|---|---|---|
| I2CSCL / SCL | TB3 pin 2 | PA8 / I2C3_SCL | P1 pin 53 |
| I2CSDA / SDA | TB3 pin 1 | PC9 / I2C3_SDA | P1 pin 54 |
| VEN | TB2 pin 6 | PC8 / GPIOC line 8 | P1 pin 55 |
| IRQ | TB2 pin 5 | PB7 / GPIOB line 7 | P1 pin 24 |
| VBAT/VDD(PAD), 3.3 V | TB2 pin 1 | 3V | P2 pin 1 |
| VANT, 5 V | TB2 pin 2 | 5V | P1 pin 1 or 2 |
| GND | TB2 pin 4 | GND | P1 pin 63 or 64 |

The OM5578/OM5579 controller board defaults to 7-bit I2C address `0x28`.
Connect both 3.3 V and 5 V: 3.3 V powers the controller and its I/O, while the
standard kit configuration uses 5 V on VANT to generate the RF transmitter
supply. Do not apply 5 V to any STM32 GPIO or I2C signal.

On a bare PN7150 controller board, these signals are on TB2/TB3. When using an
Arduino, Raspberry Pi, or BeagleBone adapter board, use the adapter labels for
SCL, SDA, VEN, IRQ, 3.3 V, 5 V, and GND rather than treating it as an SPI shield.

Do not connect or disconnect the module while either board is powered. Check
continuity and supply polarity before power-up.

Do **not** use PB6 / P1 pin 23 for VEN. PB6 is the `SDNE1` chip-select for the
Discovery board's external SDRAM; changing it while Linux is running causes an
immediate memory fault.

## Why the Linux patches are required

Two Linux 6.6 integration faults were isolated during pure interrupt bring-up.
Neither is a change to NXP's NCI/NDEF library.

### STM32F4 I2C STOP after a read-address NACK

**Issue and root cause:** PN7150 legitimately does not acknowledge its I2C
read address when it has no meaningful data. Linux 6.6.151's STM32F4 error
handler generated `STOP` for a write acknowledge failure only. A read-address
NACK therefore left `SR2.BUSY` and master mode set even with SCL and SDA high;
the first read returned `EIO`, and every later write returned `EBUSY`.

**Solution:**
[`0006-i2c-stm32f4-generate-stop-on-read-address-nack.patch`](../../board/stm32f429disco/linux-patches/linux-6.6.151/0006-i2c-stm32f4-generate-stop-on-read-address-nack.patch)
generates `STOP` for any acknowledge failure before clearing `AF` and returning
the error. Normal IRQ-driven operation avoids speculative reads, while this
error path guarantees that one NACK cannot wedge the controller.

**Manufacturer evidence:** NXP's
[PN7150 Hardware Design Guide, AN11756, section 4.5](https://www.nxp.com/docs/en/application-note/AN11756.pdf)
says that the I2C address is not acknowledged when no meaningful data is
available and recommends the external-IRQ implementation. NXP's
[PN7150 User Manual, UM10936, section 3.4](https://www.nxp.com/docs/en/user-guide/UM10936.pdf)
shows IRQ held active while data awaits a host read. ST's
[I2C CPAL manual, UM1029](https://www.st.com/resource/en/user_manual/um1029-stm32f10xx-stm32f2xx-stm32f4xx-and-stm32l1xx-i2c-communication-peripheral-application-library-cpal-stmicroelectronics.pdf)
requires an I2C master that receives NACK to generate `STOP` or a repeated
`START`.

### SYSCFG clock while routing PB7 to EXTI7

**Issue and root cause:** requesting PB7 rising-edge events installed EXTI7,
but no interrupt arrived. Inspection showed the EXTI7 mux still at its reset
value, PA7: `SYSCFG_EXTICR2=0x0000` instead of the PB7 selection `0x1000`, and
the RCC SYSCFG clock was disabled. The pinctrl regmap write was consequently
ignored.

**Solution:**
[`0007-pinctrl-stm32-enable-optional-irqmux-clock.patch`](../../board/stm32f429disco/linux-patches/linux-6.6.151/0007-pinctrl-stm32-enable-optional-irqmux-clock.patch)
lets STM32 pinctrl acquire and enable an optional `irqmux` clock. The board DT
provides the SYSCFG APB2 clock, so pinctrl can program PB7-to-EXTI7 regardless
of AFBOOT's clock state.

**Manufacturer evidence:** the
[STM32F429 reference manual, RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-407-415-417-437-455-469-application-note-stmicroelectronics.pdf)
defines `SYSCFG_EXTICR2.EXTI7=0000` as PA7 and `0001` as PB7. ST's
[migration application note AN3427](https://www.st.com/resource/en/application_note/an3427-migrating-a-microcontroller-application-from-stm32f1-to-stm32f2-series-stmicroelectronics.pdf)
shows enabling the SYSCFG APB clock before calling the EXTI-line routing
operation.

### Local validation

With the 10 ms polling fallback disabled, `pn7150-demo info` repeatedly
completed NCI initialization, PB7's interrupt count increased in
`/proc/interrupts`, and `pn7150-demo poll` detected a Type 2 tag and decoded
its NDEF Text record (`hello`). This verifies that the fixed path is genuinely
IRQ-driven; the menu fallback is diagnostic and is not required on this board.

## Build and run

Select **External options -> NFC -> NXP PN7150 demo**, build, and
flash the image. The nested **10 ms IRQ polling fallback** option keeps Linux
GPIO rising-edge events as the primary path and checks the IRQ level after each
10 ms event timeout. It is disabled by default because pure interrupt operation
has been verified on this board; enable it only when diagnosing a platform that
does not deliver GPIO edge events. On the target:

```sh
pn7150-demo info
pn7150-demo poll
```

`info` verifies VEN, IRQ, I2C, and the basic NCI exchange. `poll` waits for one
tag, reads and decodes its NDEF message, and then exits. Every NCI frame is
printed as hexadecimal so wiring, controller status, and discovery failures can
be diagnosed from the serial console. Press Ctrl-C to stop waiting.

Expected bring-up begins like this (firmware bytes vary):

```text
TML: VEN=gpiochip2:8 (PC8) IRQ=gpiochip1:7 (PB7)
NCI >> (4): 20 00 01 00
NCI << (...): 40 00 ... 00 ...
NCI >> (3): 20 01 00
NCI << (...): 40 01 ... 00 ...
NCI initialization succeeded.
```

If it stops before the first response, check 3.3 V, GND, VEN, SDA/SCL order,
and the I2C pull-ups on the selected adapter. If initialization succeeds but
no tag is found, check the 5 V VANT supply and keep metal away from the antenna.
