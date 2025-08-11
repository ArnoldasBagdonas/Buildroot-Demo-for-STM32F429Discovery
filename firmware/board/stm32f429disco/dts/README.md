# STM32F429Discovery DTS and Peripheral Configuration Guide

This guide provides step-by-step instructions for configuring **Device Tree Source (DTS)** files and Linux kernel settings for `I2C`, `SPI`, `PWM`, and `Serial` interfaces on the `STM32F429I-DISC1` board.

It also includes a **troubleshooting checklist** for device initialization, driver binding, and runtime issues.

---

## Table of Contents
1. [Board Overview](#board-overview)
2. [Directory Structure](#directory-structure)
3. [Modifying Linux Device Tree in Buildroot](#modifying-linux-device-tree-in-buildroot)
4. [Peripheral Configuration](#peripheral-configuration)
    - [GPIO](#gpio)
    - [I2C](#i2c)
    - [SPI](#spi)
    - [MEM](#mem)
    - [PWM](#pwm)
    - [ADC](#adc)
    - [Serial (USART/RS-485)](#serial)
	- [Linux sleep functions](#linux-sleep-functions)
5. [Troubleshooting](#troubleshooting)
6. [References](#references)
7. [License](#license)

## Board Overview

**STM32F429I-DISC1** includes:

- ST-LINK/V2-B embedded debugger/programmer
- 2.4" QVGA TFT LCD
- External 64-Mbit SDRAM
- ST MEMS gyroscope
- USB OTG micro-AB connector
- Onboard LEDs and push-buttons

![STM32F429I-DISC1 board](../../../../docs/DISCO_F429ZI.jpg.250x250_q85.jpg)

### Pinout Legend

![STM32F429I-DISC1 board](../../../../docs/pinout_legend_2017-06-28-2.jpg)

![STM32F429I-DISC1 board](../../../../docs/disco_f429zi_2017-07-25_slide1.jpg)

![STM32F429I-DISC1 board](../../../../docs/disco_f429zi_2017-07-25_slide2.jpg)

![STM32F429I-DISC1 board](../../../../docs/disco_f429zi_2017-07-25_slide3.jpg)

## Directory Structure

Device tree and related files are typically organized as follows:

```
firmware/
├── board/stm32f429disco/
│   ├── dts/                        ← Custom Device Tree files
│   │   ├── stm32f429disco-custom.dts
│   │   ├── overlays/               ← Optional DT overlays for peripherals
│   │   └── README.md               ← This file
│   ├── linux.config                ← Kernel configuration
│   ├── defconfigs/                 ← Buildroot configurations
│   ├── linux-patches/              ← Custom kernel patches
│   └── rootfs-overlay/             ← Root filesystem overlay
└── ...
```

## Modifying Linux Device-Tree in Buildroot

After making changes to the Device Tree:
```
# Rebuild the Linux kernel
make linux-rebuild

# Rebuild the entire system image
make
```
> **Note**: run these commands from the root of your Buildroot directory.

Or use top-level automation Makefile wrapper:

```
make dtb-rebuild
```

## Peripheral Configuration

### GPIO
This example configures the on-board LEDs and user button for user-space access:
- `ioexample1` works on LED sysfs entries (`/sys/class/leds`) to control LED triggers and brightness.
- `ioexample2` opens GPIO chips (`/dev/gpiochipX`) and lines directly to read button input and control LEDs.

**Test Manually via `sysfs`**:

After boot:

```bash
# List LEDs exposed via sysfs
ls /sys/class/leds

# Expected entries (may vary per board DTS):
led-green
led-red
```

Manually control green LED:

```bash
# Clear heartbeat trigger to manually control green LED
echo none > /sys/class/leds/led-red/trigger

# Turn LED ON
echo 1 > /sys/class/leds/led-red/brightness

# Turn LED OFF
echo 0 > /sys/class/leds/led-red/brightness
```

**Device Tree** (`stm32f429disco-custom.dts`):

```dts
. . .
	leds {
		compatible = "gpio-leds";
		led-red {
			gpios = <&gpiog 14 0>;
			linux,default-trigger = "heartbeat";
		};
		// led-green {
		//	gpios = <&gpiog 13 0>;
		//	/* linux,default-trigger = "heartbeat"; */
		// };
	};
	gpio-keys {
		compatible = "gpio-keys";
		autorepeat;
		button-0 {
			label = "User";
			linux,code = <KEY_HOME>;
			gpios = <&gpioa 0 0>;
		};
	};
. . .

&gpioa {
    status = "okay";
};

&gpiog {
    status = "okay";
};
```

**Kernel Configuration** (`linux.config`)

- `CONFIG_GPIOLIB`=y: enables the GPIO subsystem allowing drivers and user-space access to GPIO lines.
- `CONFIG_GPIO_SYSFS`=y: exposes GPIO lines as files in `/sys/class/gpio` for simple user-space control.
- `CONFIG_GPIO_CDEV`=y: provides character device interface `/dev/gpiochipN` for advanced GPIO management.
- `CONFIG_NEW_LEDS`=y: enables the LED class framework.
- `CONFIG_LEDS_GPIO`=y: supports LEDs connected via GPIO lines.

Not implemented yet in this example(!):

- `CONFIG_KEYBOARD_GPIO`=y: enables input device support for buttons/switches connected via GPIO, allowing event reporting.

> **Notes**:
  - `led-green` and `led-red` are controlled via `/sys/class/leds`.
  - `ioexample1` works on LED `sysfs` entries.
  - `ioexample2` opens GPIO chips (/dev/gpiochipX) and lines directly.
  - The User button is accessible as a GPIO input line (`A0` → `/dev/gpiochip0`, line 0) and also appears as an input event device when `gpio-keys` is enabled.

### I2C

The `STM32F429Discovery` board features `I2C3`, which by default connects to the `STMPE811` touchscreen controller.
The following example configures this interface for both the touchscreen and user-space access (e.g., via the `ioexample3` application).

**Device Tree** (`stm32f429disco-custom.dts`):

```dts
&i2c3 {
	pinctrl-names = "default";
	pinctrl-0 = <&i2c3_pins>;
	clock-frequency = <100000>;
	status = "okay";

	stmpe811@41 {
		compatible = "st,stmpe811";
		reg = <0x41>;
		interrupts = <15 IRQ_TYPE_EDGE_FALLING>;
		interrupt-parent = <&gpioa>;
		/* 3.25 MHz ADC clock speed */
		st,adc-freq = <1>;
		/* 12-bit ADC */
		st,mod-12b = <1>;
		/* internal ADC reference */
		st,ref-sel = <0>;
		/* ADC converstion time: 80 clocks */
		st,sample-time = <4>;

		stmpe_touchscreen {
			compatible = "st,stmpe-ts";
			/* 8 sample average control */
			st,ave-ctrl = <3>;
			/* 7 length fractional part in z */
			st,fraction-z = <7>;
			/*
			 * 50 mA typical 80 mA max touchscreen drivers
			 * current limit value
			 */
			st,i-drive = <1>;
			/* 1 ms panel driver settling time */
			st,settling = <3>;
			/* 5 ms touch detect interrupt delay */
			st,touch-det-delay = <5>;
		};

		stmpe_adc {
			compatible = "st,stmpe-adc";
			/* forbid to use ADC channels 3-0 (touch) */
			st,norequest-mask = <0x0F>;
		};
	};
};
```

**Kernel Configuration** (`linux.config`)

- `CONFIG_I2C`=y: enables I2C bus support in the kernel, allowing communication with I2C peripherals.
- `CONFIG_I2C_CHARDEV`=y: provides character device interface (`/dev/i2c-N`) for user-space I2C communication.
- `CONFIG_I2C_STM32F4`=y: enables STM32F4-specific I2C controller driver for hardware management of `I2C3`.


### SPI

The STM32F429Discovery board supports `SPI5`, typically mapped as:

- `CS0` → Gyroscope (`L3GD20`) or fallback `spidev` device
- `CS1` → Display panel (`ILI9341`-compatible)

> **Note**:
  
  - The `"rohm,dh2228fv"` compatible string provides a `/dev/spidevX.Y` device if no dedicated driver is available.
  
  - If `"st,l3gd20-gyro"` is present and supported, the kernel will automatically bind to the `L3GD20` driver. Otherwise, `/dev/spidev*` will be exposed for testing or debugging.

This example configures `SPI5` for both the gyroscope and display, while still allowing user-space access (e.g., via the `ioexample4` application).

**Device Tree** (`stm32f429disco-custom.dts`)

```dts
&spi5 {
	status = "okay";
	pinctrl-0 = <&spi5_pins>;
	pinctrl-names = "default";
	#address-cells = <1>;
	#size-cells = <0>;
	cs-gpios = <&gpioc 1 GPIO_ACTIVE_LOW>, <&gpioc 2 GPIO_ACTIVE_LOW>;
    
	l3gd20: l3gd20@0 {
		/* Note: Order of compatible strings matters! Uses L3GD20 driver if available, otherwise spidev ("rohm,dh2228fv")  fallback */
		compatible = "st,l3gd20-gyro", "rohm,dh2228fv";
		/* The spidev driver ignores any unknown properties in the DT node. */
		spi-max-frequency = <1000000>;
		st,drdy-int-pin = <2>;
		interrupt-parent = <&gpioa>;
		interrupts = <1 IRQ_TYPE_EDGE_RISING>,
				<2 IRQ_TYPE_EDGE_RISING>;
		reg = <0>;
		status = "okay";
	};

	display: display@1{
		/* Connect panel-ilitek-9341 to ltdc */
		compatible = "st,sf-tc240t-9370-t", "ilitek,ili9341";
		reg = <1>;
		spi-3wire;
		spi-max-frequency = <10000000>;
		dc-gpios = <&gpiod 13 0>;
		port {
			panel_in_rgb: endpoint {
			remote-endpoint = <&ltdc_out_rgb>;
			};
		};
	};
};
```

**Kernel Configuration** (`linux.config`)

- `CONFIG_SPI`=y: enables SPI bus support for synchronous serial communication.
- `CONFIG_SPI_STM32`=y: provides the STM32F4 SPI controller driver.
- `CONFIG_SPI_SPIDEV`=y: exposes spidev character devices (/dev/spidevX.Y) for user-space SPI device access.

### MEM

This `ioexample5` demonstrates direct access to STM32F4 memory-mapped registers via `/dev/mem` using the periphery `MMIO` interface.

- It maps the RCC (Reset and Clock Control) peripheral registers into user space.
- Reads the PLL configuration registers to compute the system clock frequency (`SYSCLK`).
- Shows how to interpret PLL register fields to calculate CPU frequency based on the STM32F4 reference manual.

**Test Manually via Sysfs**:
This reads the RCC_CR register directly, but you’d still need to decode the register manually or with a script:

```bash
sudo dd if=/dev/mem bs=4 count=1 skip=$((0x40023800/4)) 2>/dev/null | od -t x4
```

**Device Tree** (`stm32f429disco-custom.dts`)

Not needed for /dev/mem access.

**Kernel Configuration** (`linux.config`)

- `CONFIG_DEVMEM`=y: enables user-space access to physical memory through /dev/mem, allowing direct peripheral register access.

### PWM

This example configures `TIM3_CH1` on `PB4` for PWM output and allows user-space control (e.g., via the `ioexample6` application).

**Test Manually via Sysfs**:

After boot:

```bash
# Check available PWM controllers
ls /sys/class/pwm

# You should see something like:
pwmchip0
```

Test PWM:

```bash
cd /sys/class/pwm/pwmchip0

# Export TIM3_CH1 (index 0)
echo 0 > export
echo 1000000 > pwm0/period       # Set period to 1,000,000 ns (1 ms)
echo 250000  > pwm0/duty_cycle   # 25% duty
echo 1 > pwm0/enable             # Enable

# Wait or observe output here...

# Disable PWM channels when done
echo 0 > pwm0/enable

# Unexport PWM channels
echo 0 > unexport
```

**Device Tree** (`stm32f429disco-custom.dts`)

```dts
&pwm3_pins {
    pins {
        pinmux = <STM32_PINMUX('B', 4, AF2)>; /* Keep only TIM3_CH1 (PB4) */
    };
};

&timers3 {
    status = "okay";
	pinctrl-names = "default";
	pinctrl-0 = <&pwm3_pins>;
    pwm {
        status = "okay";
    };
};
```

**Kernel Configuration** (`linux.config`)

- `CONFIG_PWM`=y: enables generic Pulse Width Modulation support in the kernel.
- `CONFIG_PWM_STM32`=y: STM32-specific PWM driver support for timers used as PWM controllers.
- `CONFIG_PWM_SYSFS`=y: provides sysfs interface under /sys/class/pwm for user-space control of PWM devices.
- `CONFIG_SYSFS`=y: enables the sysfs pseudo-filesystem exposing kernel objects and device attributes to user-space.
- `CONFIG_CONFIGFS_FS`=y: supports a userspace-driven configuration filesystem, often required for advanced device configuration.

### SERIAL

This example configures USART3 on PB10 (TX) and PB11 (RX), allowing user-space access as:

- Standard serial port → `/dev/ttySTM1`
- RS-485 mode → via `ioctl` (`TIOCSRS485`)

Example user-space apps:

- `ioexample7` → UART
- `ioexample8` → RS-485

**Test Manually**:

After boot:

```bash
# Check available devices
ls /sys/

# Expected:
/dev/ttySTM0
/dev/ttySTM1
```

Basic UART test:

```bash
stty -F /dev/ttySTM1 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw
echo "Hello SERIAL" > /dev/ttySTM1
```

`stty` options explained:

| Option     | Meaning                                      |
|------------|----------------------------------------------|
| `-F`       | Target device file                           |
| `115200`   | Baud rate                                    |
| `cs8`      | 8 data bits                                  |
| `-cstopb`  | 1 stop bit (`cstopb` for 2 stop bits)        |
| `-parenb`  | Disable parity (`parenb` enables parity)     |
| `-ixon`    | Disable software flow control (XON/XOFF)     |
| `-ixoff`   | Disable software flow control (receive side) |
| `-crtscts` | Disable hardware flow control                |
| `raw`      | Raw mode (no special character processing)   |


Parity/Stop Bits Quick Reference:

| Option    | Meaning                                 |
|-----------|-----------------------------------------|
| `parenb`  | Enable parity                           |
| `-parenb` | Disable parity                          |
| `parodd`  | Odd parity                              |
| `-parodd` | Even parity (default if parity enabled) |
| `cstopb`  | 2 stop bits                             |
| `-cstopb` | 1 stop bit                              |

If your driver supports RS485 mode, configure TX and RX delay times via Device Tree or configure RS485 parameters dynamically (user space app).

**Device Tree** (`stm32f429disco-custom.dts`)

> **Note**: if the DTS does not define a **serial alias** for `usart3`, the STM32 USART driver will not assign a valid device node (e.g., `/dev/ttySTM1`, `/dev/ttySTM2`).

RS-485 with DE GPIO control:
```dts
. . .
	aliases {
		serial0 = &usart1;
		serial1 = &usart3;
	};
. . .
&usart3 {
    pinctrl-names = "default";
    pinctrl-0 = <&usart3_pins_a>;
	status = "okay";

	/* Enable RS485 and DE GPIO control*/
	linux,rs485-enabled-at-boot-time;
	rts-gpios = <&gpiod 12 GPIO_ACTIVE_HIGH>;
	rs485-rts-active-high;
	rs485-rts-delay = <1 1>; /* max 100 ms delay before sending, max 100 ms after sending */
};
```

Alternatively, hardware flow control (RTS/CTS):

```dts
&usart3 {
    pinctrl-names = "default";
    pinctrl-0 = <&usart3_pins_a>;
	status = "okay";

	/* UART has dedicated lines for RTS/CTS hardware flow control */
	/* enabled by pinmux configuration                            */
	uart-has-rtscts;
};

/* Override usart3_pins_a */
&pinctrl {
    usart3_pins_a: usart3-0 {
        pins1 {
            pinmux = <STM32_PINMUX('B', 10, AF7)>, // USART3_TX
                     <STM32_PINMUX('B', 14, AF7)>; // USART3_RTS
            bias-disable;
            drive-push-pull;
            slew-rate = <0>;
        };
        pins2 {
            pinmux = <STM32_PINMUX('B', 11, AF7)>, // USART3_RX
                     <STM32_PINMUX('B', 13, AF7)>; // USART3_CTS
            bias-disable;
        };
    };
};
```

**Kernel Configuration** (`linux.config`)

- `CONFIG_TTY`=y: enables TTY layer support for character devices like serial ports.
- `CONFIG_SERIAL_STM32`=y: enables STM32 family serial port driver for UART/USART peripherals.
- `CONFIG_SERIAL_STM32_CONSOLE`=y: enables serial console support on STM32 serial ports, useful for kernel messages and debugging.

> **Note**: these are usually enabled if a serial console is already configured.


### ADC

This configuration demonstratesthe ADC (Analog-to-Digital Converter) on the STM32F429Discovery board features. The example focuses on ADC3 channel 8 (PF10 pin) configured for 12-bit resolution.

**Test Manually via Command Line**:

After booting your system with ADC enabled in the Device Tree and kernel, you can access ADC readings via the Industrial I/O (iio) subsystem sysfs interface:

```bash
ls /sys/bus/iio/devices/
# You should see something like iio:device0 or similar

ls /sys/bus/iio/devices/iio:device0/
# Look for files like in_voltage8_raw, in_voltage8_scale, in_voltage8_offset

cat /sys/bus/iio/devices/iio:device0/in_voltage8_raw
# Returns raw ADC counts (integer between 0 and 4095 for 12-bit resolution)

cat /sys/bus/iio/devices/iio:device0/in_voltage8_scale
# Returns scale factor in volts per step (e.g., 0.000805664)
```

Calculate voltage manually:

```bash
raw=$(cat /sys/bus/iio/devices/iio:device0/in_voltage8_raw)
scale=$(cat /sys/bus/iio/devices/iio:device0/in_voltage8_scale)
echo "Voltage = $(echo "$raw * $scale" | bc -l) V"
```

Continuous reading with watch:
```bash
watch -n 1 cat /sys/bus/iio/devices/iio:device0/in_voltage8_raw
```
**Device Tree** (`stm32f429disco-custom.dts`)

```dts
/* Ensure DMA2 is enabled (ADC dmas reference &dma2 in stm32f429.dtsi). */
&dma2 {
	status = "okay";
};

&gpiof {
    status = "okay";
};

&adc3_in8_pin {
    pins {
        bias-disable;
    };
};

&adc {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&adc3_in8_pin>;
    vref-supply = <&vref>;
    vdda-supply = <&vdda>;
};

/* Enable ADC3 instance with PF10 channel 8 */
&adc3 {
    status = "okay";
    st,adc-channels = <8>;
	assigned-resolution-bits = <12>;
};
```

**Kernel Configuration** (`linux.config`)

- `CONFIG_REGULATOR`=y: enables the Linux regulator framework, which manages voltage and current regulators in the system.
- `CONFIG_REGULATOR_FIXED_VOLTAGE`=y: allows the kernel to handle fixed voltage sources such as vref or vdda used by the ADC, ensuring the device tree’s voltage supplies are recognized.
- `CONFIG_IIO`=y: enables the Industrial I/O (`IIO`) subsystem in the kernel.
- `CONFIG_STM32_ADC_CORE`=y: provides the low-level driver support that is common across STM32 ADC devices, forming the foundation for specific ADC instances.
- `CONFIG_STM32_ADC`=y: allows actual use of STM32 ADC hardware, implementing the interface between the hardware and the `IIO` subsystem.
- `CONFIG_IIO_SYSFS_TRIGGER`=y: this allows you to trigger ADC conversions and read data through the sysfs filesystem, which is how user space tools and manual testing often interact with the ADC.

### Linux sleep functions

`sleepexample` (Sleep Functions Test Utility) a simple C utility designed to demonstrate and test various Linux sleep functions on embedded systems. It is especially useful for verifying kernel timer behavior and diagnosing issues where sleep functions return prematurely due to missing or misconfigured clocksource drivers.

**Features**:
- Interactive tests over a serial console.
- Demonstrates the following sleep functions:
- `sleep(seconds)`
- `usleep(microseconds)`
- `nanosleep(timespec)`
- `clock_nanosleep(clockid_t, flags, timespec)`
- Prints system date/time before and after each sleep call to verify timer accuracy.

**Why Use This Utility?**

Embedded Linux systems often face issues with kernel timers and clocksources, especially on custom or less common hardware like STM32. This utility helps to:

- Confirm that sleep and timer APIs work as expected.
- Diagnose kernel timer and tick source problems.
- Ensure the system’s high-resolution timers and clocksources are functional.

**Device Tree** (`stm32f429disco-custom.dts`)

Not needed for Linux sleep functions.

**Kernel Configuration** (`linux.config`)

Core timekeeping (mandatory for all sleep functions):

- `CONFIG_HIGH_RES_TIMERS`=y: high-resolution timers for sub-second sleeps
- `CONFIG_POSIX_TIMERS`=y: required for nanosleep() and clock_nanosleep()
- `CONFIG_GENERIC_CLOCKEVENTS`=y: basic timer interrupt handling
- `CONFIG_TICK_ONESHOT`=y: allows precise one-shot timers
- `CONFIG_HZ`=1000: recommended for millisecond accuracy
- `CONFIG_CLKSRC_STM32`=y: STM32 clocksource driver

Required for specific tests:

- `CONFIG_TIMERFD`=y: needed for timerfd_create() test
- `CONFIG_EPOLL`=y: needed for epoll_wait() test
- `CONFIG_EVENTFD`=y: required by epoll for some event handling cases
- `CONFIG_SIGNALFD`=y: part of the epoll ecosystem, not strictly needed for current code
- `CONFIG_FUTEX`=y: required by pthread_cond_timedwait() test (POSIX thread waits depend on futexes)

Real-Time Clock (RTC), hardware clock device:

- `CONFIG_RTC_CLASS`=y: enable RTC subsystem support in the Linux kernel
- `CONFIG_RTC_DRV_STM32`=y: use the STM32-specific hardware RTC driver
- `CONFIG_RTC_HCTOSYS`=y: at boot, set system time from hardware RTC
- `CONFIG_RTC_SYSTOHC`=y: periodically or on shutdown, write system time back to RTC
- `CONFIG_RTC_HCTOSYS_DEVICE`="rtc0": RTC device used to initialize system clock at boot

Optional but recommended:

- `CONFIG_SYSFS`=y: enables /sys virtual filesystem exposing device and kernel info
- `CONFIG_PROC_FS`=y: enables /proc virtual filesystem for system info
- `NO_HZ_IDLE`=n: disables tickless idle; recommended for real-time precision in sleep and timers


## Troubleshooting

This guide provides step-by-step troubleshooting procedures and checks for device initialization issues, driver binding problems, device tree verification, and typical hardware/software misconfigurations on Linux systems, focusing on I2C and SPI buses and related peripherals.

### Pre-requisites

- Kernel configured with debug options enabled.
- Access to `/sys/kernel/debug`, `/proc/device-tree`, and `/sys/bus/*` interfaces.
- Basic Linux shell familiarity.


### Debugfs and Kernel Debug Interfaces

**Mount debugfs**:

```bash
mount -t debugfs none /sys/kernel/debug
ls /sys/kernel/debug
```

**Check Clocks**:

```bash
cat /sys/kernel/debug/clk/clk_summary
cat /sys/kernel/debug/clk/clk_summary | grep <device-name>
```

**View current pin multiplexing state**:

```bash
cat /sys/kernel/debug/pinctrl/*/pinmux-pins
cat /sys/kernel/debug/pinctrl/*/pinmux-pins | grep <device-name>
```

**Pin controller status and pin assignments**:

```bash
cat /sys/kernel/debug/pinctrl/<your-device-pinctrl>/
```

**Check IRQ mapping and usage**:

```bash
cat /sys/kernel/debug/irq/<irq-number>/irq
cat /proc/interrupts
```

**Regulator Status**:

```bash
cat /sys/class/regulator/regulator*/name
cat /sys/class/regulator/regulator*/state
```

### Device Tree Inspection

**Inspect Flattened Device Tree (FDT)**:

```bash
ls /proc/device-tree/soc
```

- Nodes correspond to hardware controllers and devices.
- Each node contains files like `compatible`, `reg`, `status`.

**Verify Device Status**:

```bash
cat /proc/device-tree/soc/<node>/<device>/status
```

- `okay` means enabled.
- `disabled` means ignored by kernel.

**Inspect Compatible Strings**:

```bash
od -c /proc/device-tree/soc/<node>/<device>/compatible
```

- Ensure the compatible string matches a kernel driver.

### Device Node and Driver Binding Checks

**Verify Device Files in `/dev`**:

```bash
ls /dev/
# Look for i2c-X, spidevX.Y, and other device nodes
```

**Verify I2C Bus and Devices**:

```bash
ls /sys/bus/i2c/devices/
# Look for 0-0041  i2c-0, and other device nodes
```

Inspect Device Properties:

```bash
ls -l /sys/bus/i2c/devices/i2c-0/
ls -l /sys/bus/i2c/devices/0-0041/
```

- Check for `driver` symlink; absence indicates no driver bound.
- Check `waiting_for_supplier` file presence — indicates deferred probing due to missing dependencies.

**Verify SPI Devices**:
```bash
ls /sys/bus/spi/devices/
# Look for spi0.0  spi0.1, and other device nodes
```

Inspect Device Properties:

```bash
ls -l /sys/bus/spi/devices/spi0.0/driver
ls -l /sys/bus/spi/devices/spi0.1/driver
```

- Check for `driver` symlink; absence indicates no driver bound.
- Check `waiting_for_supplier` file presence — indicates deferred probing due to missing dependencies.

## References

**Device Tree Bindings and Linux Kernel Documentation**

- [Multi-Function Device (MFD) Bindings](https://www.kernel.org/doc/Documentation/devicetree/bindings/mfd/)
- [Serial Device Tree Bindings - General](https://www.kernel.org/doc/Documentation/devicetree/bindings/serial/serial.txt)
- [STM32 USART Device Tree Bindings](https://www.kernel.org/doc/Documentation/devicetree/bindings/serial/st%2Cstm32-usart.txt)
- [RS485 Device Tree Bindings (YAML)](https://www.kernel.org/doc/Documentation/devicetree/bindings/serial/rs485.yaml)
- [RS485 Driver API Documentation](https://www.kernel.org/doc/Documentation/driver-api/serial/serial-rs485.rst)
- [STM32 ADC Device Tree Bindings (TXT)](https://www.kernel.org/doc/Documentation/devicetree/bindings/iio/adc/st%2Cstm32-adc.txt)
- [STM32 ADC Device Tree Bindings (YAML)](https://www.kernel.org/doc/Documentation/devicetree/bindings/iio/adc/st,stm32-adc.yaml)

**Tutorials and Practical Guides**

- [Bootlin Embedded Linux Documentation](https://bootlin.com/docs/)
- [Bootlin Blog: Timer Counters on Linux (Microchip)](https://bootlin.com/blog/timer-counters-linux-microchip/)

**Board and Platform Specific**
- [Mbed OS: ST Discovery F429ZI Board](https://os.mbed.com/platforms/ST-Discovery-F429ZI/)
- [Modifying Linux Device Tree in Buildroot (Microchip Knowledge Base)](https://microchip.my.site.com/s/article/Modifying-Linux-Device-Tree-in-Buildroot)

## License

This repository is licensed under the MIT License. See the LICENSE file for details.