# Introduction

The Gatery Bit Bang Engine can be used to interface to various pereferals including UART, SPI, I2C, JTAG and GPIO. It is inspired by FTDI's MPSSE and is in many aspects fully compatible to the public documentation from FTDI. There is a comparison chapter, in case you are familar with the MPSSE.

## Internal interface
The device has one 8bit command stream that contains instructions and one 8bit result stream.
Each command starts with one instruction byte, followed by parameter bytes depending on command. A command by send back data to the result stream.

## External interface
The Bit Bang Engine can be configured for up to 16 IO's. Each IO can be configured to be an input, output or open drain. See Feature/Open-Drain or Feature/DirectIO for details.
Some commands make use of dedicated pins. These pins are 0=clock, 1=dout, 2=din, 3=tms, 5=wait.

# Feature
## DirectIO

It is possible to directly get and set upto 16 pins. The commands are:
```
0x80 0xVV 0xDD  # set pins 0-7
0x81            # get pins 0-7
0x82 0xVV 0xDD  # set pins 8-15
0x83            # get pins 8-15
```
- bit 0 addresses pin 0 and so on
- a '1' in V lets the pin drive high
- a '1' in D configures the pin to be an output
- set commands do not preduce anything on the result stream
- get commands output the current pin reading to the result stream

*Example:* Configure pin 0-1 as outputs and set pin 0 to low and pin 1 to high. Pin 2-7 are configured as inputs. `0x80 0x02 0x3`.  

## Fast DirectIO

Since bandwidth is very limited we added a set of commands to quickly toggle 4 pins and read 8 pins. This can be used in cases where the protocol or parts of a protocol require more sufisticated bit banging. The changes are timed to be at least one half clock period of the configured clock divider apart from each other and any serial engine change. Such that a sequence of changes can be used to toggle clock and data pins with guaranteed minimum period.

```
0xCS # sets pin 0-3 to S
0xDS # sets pin 0-3 to S and read pin 0-7
```

- Bit 0 addresses pin 0 and so on.
- The pin is first read and than changed. So you will not read S directly.
- The pin directions needs to be configured using DirectIO. 

## Serial Engine

The serial engine can serialize data bits out on the dout pin, while serialize data bits in on the din pin or dout pin, while toggeling the clock pin. The features external loopback, clock divider, three phase clocking and clock stratching have direct influence on the serialization process.

The instruction can be any combination of the following bits:
```
0 - setup/output/write clock edge   # '0' = rising edge, '1' = falling edge
1 - lenght in bits or bytes         # '0' = bytes, '1' = bits
2 - capture/input/read clock edge   # '0' = rising edge, '1' = falling edge
3 - bit order for input and output  # '0' = MSB first, '1' = LSB first
4 - enable output on dout pin
5 - enable input on din pin
6 - TMS mode                        # see TMS section
7 # '0' = serialize instruction 
```

- The idle clock level is configured using the DirectIO command `0x80`.
- The clock is deffered by 1/2 cycle in case the first clock edge is not a setup edge, to be compliant with common SPI Modes.
- See examples in the SPI application notes for common clock level and edge configurations

- In bit mode the instruction byte is followed by one byte containing the number of clock cycles - 1. (0x00 = 1 cycle, 0xFF = 256 cycles)
- In byte mode the instruction byte is followed by two length bytes, containing the number of clock cycles / 8 - 1. (0x00 0x00 = 8 cycles, 0xFF 0xFF = 524288 cycles)

- If output is enabled the length is followed by the data to send.
- If input is enabled the command generated read bytes to the result stream containing up to 8 bits per byte.
- If partial bytes are read in input mode, data is shifted in on the lsb in msb mode and msb in lsb mode.

## TMS mode

This mode can be used for more efficient JTAG operations. It is also useful if you need to control two output pins at the same time with serial output.
In this mode the following changes are made:
- A full byte is 7 instead of 8 bits for input and output data.
- Data is serialized out on the tms pin instead of dout pin.
- The dout pin is driven by the msb of the input data byte.

## external loopback

If enabled, din reads the pin state of dout. This can be used for diagnostics or for I2C where din and dout is on the same wire. In that case pin 2 (din) can be reporposed as output.

```
0x84 # enable loopback mode (connect input of din to input of dout)
0x85 # disable loopback mode
```

## clock divisor

The clock speed can be configured from 6 MHz to 91 Hz by clock division. $F = 12 MHz / ((1 + divider) * 2)$

```
0x86 0xLL 0xHH # divider = 0xLL + 0xHH * 256
```

Use the following formula to calculate the divider for a given target frequency $F$. $divider = 6000000/F - 1$.

*Example:* Configure the clock for 6 MHz `0x86 0x00 0x00`.

## wait for pin high/low

There are several ways to wait for the dedicated wait pin to be high or low. The instruction execution is paused until the wait command has finished. In the case that the wait pin is already in the desired state the command will finish after one clock cycle.

```
0x88 # wait for high
0x89 # wait for low
0x94 # wait for high and toggle clock pin
0x95 # wait for low and toggle clock pin
0x9C 0xLL 0xHH # wait for timeout or high and toggle clock pin
0x9D 0xLL 0xHH # wait for timeout or low and toggle clock pin
```

- `timout = (1 + 0xLL + 0xHH * 256) * 8`
- Waiting without timeout is not recomended. There is no way out of the wait state other than a full reset.

*Example:* Toggle clock pin and wait until the wait pin is high or timeout after 256 cycles. Followed by read pin state to check if a timout occured `0x9C 0x1F 0x00 0x81`.

## three phase clocking

For protocols that define the data to be stable during the rising and falling edge. If enabled, an additional phase is added to the clock signal. Instead of low-high a full cycle consists of low-high-low and data is changed between the two low phases only. Note that the capture edge is still configurable and shout be set to falling edge in most cases.

```
0x8C # enable three phase clocking
0x8D # disable three phase clocking
```

## Clock only Output

There are dedicated commands to toggle the clock without reading and writing data. These commands are for compatibility only and are identical to the normal serialize commands with input and output bits set to '0'.

```
0x8E 0xLL # length in bits
0x8F 0xLL 0xHH # length in bytes 
```

## clock stratching

After setting the clock pin to high, the curcuit waits for the clock pin to become high before continuing. Make sure to put your pin into open drain mode if you plan to use this feature as otherwise you will have conflicting drivers and may damage connected chips. The main application is I2C, where perepheral devices are allowed to stall the controller.

*Note:* Clock stratching cannot be combined with delayed clock edges. It produces undefined behavior if the first edge of a serial transfer is not a capture edge, such as spi mode 0 and 2. While it can be used in spi mode 1, 3 and all three phase clock modes. Consider using three phase clocking when clock stratiching is required in spi mode 0 and 2.

## Open-Drain Mode

Each pin can be configured in open-drain mode. In open-drain mode, the output buffer is enabled only to drive a low signal. For high signals the output buffer is disabled and the wire needs to pulled high using an external pull-up resistor. This allows for wired and bus configurations like I2C, where multiple masters may drive the bus at any time and also perepherals may drive the clock low for clock stretching.

```
0x9E 0xLL 0xHH # set open-drain mode for each pin
```

- pin 0 is bit 0 of 0xLL and pin 8 is bit 0 of 0xHH
- a value of '1' enables open drain mode
- the pin still needs to be configured as output for open drain to work

*Example:* Configure pin 0-1 as open drain bidirectional pins `0x9E 0x03 0x00 0x80 0x00 0x03`. Note instruction `0x80` is used to configure both pins as outputs.

## invalid command codes

In case the command stream contains an unknown instruction, the engine responds with `0xFA 0xDD`. Where 0xDD is the unknown instruction byte. Two instructions are reserved to trigger this. An application can make sure the command queue is empty by sending these and waiting for the response.

```
0xAA
0xAB
```

*Example:* Sending `0xAA` will send back `0xFA 0xAA`.

# Comparison to FTDI MPSSE

The goal was to be byte compatible to the MPSSE for the most common tasks. Many MPSSE features are not fully specified, which opens the possibility for unintentional differences.

Missing features are:
 - MCU Host emulation modes.
 - Adaptive clocking mode (look at clock stratching if you need this).
 - Setting a high value to pin 0 while it already is in a high state, toggles pin 0 to low for half a clock period on FTDI devices.

Other Changes:
 - All bit length commands support up to 256 bits
 - Loopback mode is looping back the external pin state of the dout pin. Thus, can be used for arbitration and short checking.
 - `0x87` (flush buffer command) is a noop. Data is alway sent as soon as possible. It is implemented as a no-operation to improve compatibility to lagacy applications.
 - We added Fast DirectIO commands to make bit banging transfer less command data
 - Multiple bytes of the same instruction are clocked out/in back to back without clock line gaps. Given that the next byte is ready on the command stream in time.

# Application Notes
## GPIO

Pins can be sampled and set using DirectIO. It is possible to sample pins at fixed sample rate by using the fast directio command and a clock divisor.

The command `0xD0` in a loop will sample at 1/2 clock period rate.

Note that this method depends on the speed of the command and result stream. USB applications need to be in interrupt mode for reliable bandwidth allocation. Otherwise the resulting sample frequency might be lower than expected.

## SPI

- Use DirectIO to control chip select.
- Configure idel clock (CPOL) in a seperate DirectIO command prior to chip selection to avoid clock glitching.
  - CPOL0 `0x80 0x00 0x03`
  - CPOL1 `0x80 0x01 0x03`
- The clock line will always return to its idle state.
- Use dout pin for MOSI.
- Use din pin for MISO.

Instruction Byte for Serial Engine in byte mode with msb first:
|Mode|CPOL|CPHA|In    |Out   |InOut |
|----|----|----|------|------|------|
| 0  | 0  | 0  |`0x21`|`0x11`|`0x31`|
| 1  | 0  | 1  |`0x24`|`0x14`|`0x34`|
| 2  | 1  | 0  |`0x24`|`0x14`|`0x34`|
| 3  | 1  | 1  |`0x21`|`0x11`|`0x31`|

#### Example LM71CIMF temperature sensor

This sensor is using a singe bidirectional line for MISO and MOSI. The first 16 cycles are MISO, followed by 16 cycles MOSI and so on.

set mode: continuous conversion `80 08 09 84 C0 03 0F 80 00 0B 13 0F 00 00 80 08 09`
1. `80 08 09` set cs high and configure clock and cs as output.
2. `84` enable external loopback mode to read from dout.
2. `C0` set cs low
3. `03 0F` read and ignore 16bit in SPI Mode 0
4. `80 00 0B` switch dou to output mode
5. `13 0F FF FF` send shutdown mode command. replace `FF FF` by `00 00` continuous conversion mode
6. `80 08 09` switch back to input mode and release CS

read temperature: `C0 23 0F C8`
1. `C0` cs low
2. `23 0F` read 16 bit
3. `C8` cs high

In power down mode it should read `80 0F`, otherwise (0.03125°C * value / 4) is the temperature.

See [example/LIS2DH12.cpp](example/LIS2DH12.cpp) for a complete example in C++.

## 3-wire

3-wire is a version on SPI where MISO and MOSI is sharing one wire. Use external loopback mode to read from dout instead of din.

See [example/lm71.cpp](example/lm71.cpp) for a complete 3-wire example in C++. This chip uses a protocol where it sends 16 bit to the controller before accepting 16 bits from the controller for configuration.

See [example/LIS2DH12.cpp](example/LIS2DH12.cpp) for a complete 3-wire example in C++. This chip uses a command byte from the controller to decide wether to read or write and to which register.

## I2C

- The Serial Engine supports clock stratching
- Three phase clocking should be used to be fully compliant. It may be turned off if the setup and capture edge of the perepheral is known and equal for all devices on the bus, to speed up operations.
- The clock and data pin should be operated in open drain mode to prevent bus conflicts. Note hat external pullup resistors are required in that mode.
- External loopback mode can be used to capture the input on the dout pin instead of din pin.
- Wire SCL to pin0 and SDA to pin1. Pin2 (din) is not required in external loopback mode.
- Multi master arbitration is not supported, but can be implemented to some extend by reading back the sent data. Note that this method may cause the data of both controllers to be changed and can therefore result in long sequences of retries. It also only works in the address phase and may be dangerous in cases where two controllers access the same address.

See [example/hdc1000.cpp](example/hdc1000.cpp) or [example/LIS2DH12.cpp](example/LIS2DH12.cpp) for a complete example in C++.

### setup sequence
1. `0x9E 0x03 0x00` enable open drain mode on pin0 (clock) and pin1 (dout).
2. `0x80 0x03 0x03` put pin0 and pin1 into output mode and high level (idle state).
3. `0x8C` enable three phase clocking.
4. `0x84` enable external loopback.
5. `0x86 0x27 0x00` set clock frequency to 100 KHz (12 MHz / (3 * 100 KHz) - 1).

### access pattern
1. `0xC1 0xC0` set start condition by first set SDA low than SDA and SCL low.
2. `0x33 0x08 0xPP 0x80` send 8 bits of address and read/write bit in PP and one extra high bit for ACK/NACK. The response contains two bytes. The first can be checked to see if we won arbitration. The second is the ACK/NACK from the perepheral.
3. send/receive bytes and ACK/NACK according to access
4. `0xC0 0xC2 0xC3` make sure to be in active idle state then set stop condition by releasing SDA followed by SCL to high.

### Example HDC1000 humidity and temperature sensor

1. setup i2c: `9E 03 00` `80 03 03` `8C` `84` `86 87 00`
2. check ack of address: `C1` `C0` `33 08 80 80` `C0` `C2` `C3` should return `80 00`, in case of `80 01` the perepheral did not respond.
3. set register id to manufacturer ID (0xFE): `C1` `C0` `33 08 80 80` `33 08 FE 80` `C0` `C2` `C3` 
4. read current register: `C1` `C0` `33 08 81 80` `33 08 FF 00` `33 08 FF 80` `C0` `C2` `C3` should read `54 00 49 01` where `54 49` is the ID of TI and `00 01` are the read backs of our ACK/NACK.
5. set register id to temperature (0x00): `C1` `C0` `33 08 80 80` `33 08 00 80` `C0` `C2` `C3` 
6. read temperature and humidity: `C1` `C0` `33 08 81 80` `33 08 FF 00` `33 08 FF 00` `33 08 FF 00` `33 08 FF 80` `C0` `C2` `C3`. Each output byte returns two bytes. One value byte and one handshake byte.

$temperature = value / 2^{16} * 165°C - 40°C$ \
$humidity = value / 2^{16} * 100$

See [example/hdc1000.cpp](example/hdc1000.cpp) for a complete example in C++.

## JTAG

- Use TMS mode to navigate the state machine. 
- The last value set to tms and dout is held stable until actively changed by another command. Making it possible to use the normal Serial Engine instruction to send large amounts of data.
