# stm32c0-raw-PDM
This STM32C071 project drives a [SPH0641LU4H-1](https://www.mouser.com/datasheet/2/218/-746191.pdf) PDM MEMS microphone with a clock frequency of 4.8MHz in **Ultrasonic Mode**, captures the PDM data through SPI (with DMA) and forwards the raw PDM bitstream to the host through USB. All further processing is done on the host.

Tools for reading the data from USB and converting the raw PDM bitstream to PCM audio using [GNU Radio](https://www.gnuradio.org/) and [SoX](https://sourceforge.net/projects/sox/) can be found in the `host/` directory. [Baudline](https://www.baudline.com/) or GNURadio can be used to view the live spectrogram up to 80kHz.

**Note:** All of this has been tested on Linux only. Might work on *BSDs with minor modifications.

## Why?
I mainly made this to record bat calls. (See below.) While 80kHz is not sufficient to capture the calls of all bats, a lot of types can still be recorded with a very low cost setup - the SPH0641LU4H costs less than 4€.

TBD: Bat call example

Apart from this, several other ultrasonic emitters such as inductors of power supplies, acoustic marten deterrents, etc., etc. can be found/identified with this microphone. It's quite interesting to "see" some of these things we cannot hear.

TBD: example electric fly swatter
