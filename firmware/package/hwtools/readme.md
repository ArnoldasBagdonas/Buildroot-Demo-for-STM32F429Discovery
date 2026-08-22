# Hardware tools

`BR2_PACKAGE_HWTOOLS` builds selected hardware examples into the multicall
executable `/usr/bin/hw`. Each selected applet also receives its own command
symlink, so `adc-read` and `hw adc-read` are equivalent.

| Applet | Purpose |
|---|---|
| `gpio-led` | Control the red PG14 LED. |
| `button-led` | Mirror the PA0 USER button to the green PG13 LED. |
| `boot-button` | Print the PA0 state sampled and passed by AFBOOT. |
| `adc-read` | Read ADC1_IN13 on PC3, P2 pin 13. |
| `i2c-scan` | Scan the I2C3 bus. |
| `spi-gyro` | Read the onboard SPI gyroscope. |
| `rcc-clock` | Report STM32 clock settings. |
| `pwm-led` | Drive TIM3_CH1 on PB4 through PWM sysfs. |
| `uart-send` | Send data through UART5. |
| `rs485-chat` | Exercise UART5 RS-485 mode. |
| `timers` | Run timer, sleep, clock, and RTC checks. |

Useful argument forms are:

```sh
adc-read [sample-count [interval-ms]]
spi-gyro [delay-ms]
pwm-led [pwmchip-number pwm-channel-number]
uart-send [tty-device]
rs485-chat [tty-device]
```

ADC input must remain between 0 and 3.3 V. The ADC uses an analog input, not a
chip select. `boot-button` reads
`/sys/firmware/devicetree/base/chosen/bootloader,user-button`; hold USER during
reset to test the `pressed` state.

Applets select only the kernel interfaces they need. Applets using
c-periphery select the hidden `periphery` package automatically.

## Why `boot-button` needs an AFBOOT patch

The USER button is a transient boot condition: sampling PA0 after Linux and
userspace start cannot tell whether it was held at reset because it may already
have been released. AFBOOT therefore samples PA0, copies the flash DTB into a
reserved 32 KiB region at the top of SDRAM, updates the preallocated
`/chosen/bootloader,user-button` property, and passes the RAM DTB address to
Linux in ARM register `r2`. See
[`0005-stm32f429-pass-user-button-in-device-tree.patch`](../../board/stm32f429disco/patches/afboot-stm32/0005-stm32f429-pass-user-button-in-device-tree.patch).

The [STM32F429I-DISC1 board manual, UM1670](https://www.st.com/resource/en/user_manual/um1670-discovery-kit-with-stm32f429zi-mcu-stmicroelectronics.pdf)
identifies B1 USER on PA0. The
[Devicetree specification](https://devicetree-specification.readthedocs.io/en/latest/chapter3-devicenodes.html#chosen-node)
defines `/chosen` as runtime information selected by system firmware, and the
[Linux ARM boot protocol](https://docs.kernel.org/arch/arm/booting.html)
requires the bootloader to place the initialized DTB in RAM and pass its
physical address in `r2`. This is a boot-time handoff feature, not a workaround
for a manufacturer defect.
