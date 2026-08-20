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
