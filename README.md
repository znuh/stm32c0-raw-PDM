# stm32c0-raw-PDM
This STM32C071 project drives a [SPH0641LU4H-1](https://www.mouser.com/datasheet/2/218/-746191.pdf) PDM MEMS microphone with a clock frequency of 4.8MHz in **Ultrasonic Mode**, captures the PDM data through SPI (with DMA) and forwards the raw PDM bitstream to the host through USB. All further processing is done on the host.

Tools for reading the data from USB and converting the raw PDM bitstream to PCM audio using [GNU Radio](https://www.gnuradio.org/) and [SoX](https://sourceforge.net/projects/sox/) can be found in the `host/` directory. [Baudline](https://www.baudline.com/) or GNURadio can be used to view the live spectrogram up to 80kHz.

**Note:** All of this has been tested on Linux only. Might work on *BSDs with minor modifications.

## Why?
I mainly made this to record bat calls. ([See below](#analyzing-bat-calls).) While 80kHz is not sufficient to capture the calls of all bats, a lot of types can still be recorded with a very low cost setup - the SPH0641LU4H costs less than 4€.  
Example (GNURadio):  
<img width="1062" height="676" alt="fleder2" src="https://github.com/user-attachments/assets/26f0d25c-6d71-4c27-903c-9507be5c93ab" />
Bat calls - probably [Pipistrellus pipistrellus](https://en.wikipedia.org/wiki/Common_pipistrelle) as seen in the live GNURadio waterfall.

Apart from this, several other ultrasonic emitters such as inductors/transformers of power supplies, acoustic marten deterrents, etc., etc. can be found/identified with this microphone. It's quite interesting to "see" some of these things we cannot hear.  
Example (baudline):  
<img width="1920" height="1055" alt="flyswatter" src="https://github.com/user-attachments/assets/bba3a689-0ebc-4108-9311-d725dd258d29" />
The three lines in the ultrasonic spectrum come from an electric fly swatter (which my cats hate).

## Hardware Setup
The PDM Clock is derived from the STM32 48MHz system clock with the Timer 1 Output Channel 2 and fed to the microphone through PA9. This clock is also fed back into the SPI2 SCK input of the STM32 at PB8. SPI2 is configured to Slave Mode and the PDM Data is captured through SPI2 MOSI at PA10. DMA is used for SPI2 RX in circular mode with a 16KiB ring buffer. (That's ~27ms of buffering time).
<pre>+-------------------+                    +---------+
|     STM32C0       |                    | PDM MIC |
|                   |                    |         |
|  PA9  (CLK Out) --+------------+-----> | CLK     |
|                   |            |       |         |
|  PB8  (SCK In) <--+------------+       |         |
|                   |                    |         |
|  PA10 (MOSI In) <-+------------------- | DATA    |
+-------------------+                    +---------+
</pre>  
SELECT of the microphone is connected to GND.

I wired the microphone "dead bug style" to a simple [STM32C0 Board](https://github.com/znuh/stm32c0-nano-hw) I made a while ago. It looks like this:  
<img width="400" height="400" alt="mems_faedel" src="https://github.com/user-attachments/assets/1ee36cf1-bee3-4f0f-9f3e-96211178976d" />  
You have to be careful to keep any flux away from the microphone port (the opening).  
I added 3x 10nF 0603 C0G/NP0 caps as decoupling caps to the microphone. (That's the largest C0G/NP0 value I had at hand. There's also a 4.7µF 0402 X5R a few millimeters "after" the C0Gs.)

## Analyzing Bat Calls
[Audacity](https://www.audacityteam.org/) can be used to further process WAV files for analysis.  
A sensible workflow seems to be:
* apply a high pass filter first (e.g. with f=10kHz)
* then apply a compressor to amplify weak signals

After this you can use `Change Speed and Pitch` with a multiplier of `0.1` to slow the recording down 10 times to make the calls audible for puny human ears by making the calls 10 times longer and mixing them down to 1/10th of the original frequency. This also makes the variation of frequency over time more clearly visible when you view the calls in baudline or [audioprism](https://github.com/vsergeev/audioprism).  

**Examples:**

<img width="724" height="227" alt="pipi" src="https://github.com/user-attachments/assets/401e6049-9282-468a-8c2f-123086ab6940" />

Rotated baudline screenshot with Pipistrellus pipistrellus calls at 1/10th frequency and 10x time stretch. The frequency variation over time can be seen more clearly thanks to the time stretch.

<img width="897" height="225" alt="bc_unk" src="https://github.com/user-attachments/assets/12da4687-ec28-4afa-aaee-81bed5dc994a" />

Rotated baudline screenshot of unidentified, different calls with more variety. Scales again are 1/10th of actual frequency and 10x time scale.

### Further Reading
All of these links are in German, sorry:
* [Akustische Bestimmung von Fledermausrufen](https://fledermausschutz-seligenstadt.de/akustische-bestimmung-von-fledermausrufen/)
* [Bestimmungshilfe - Lautaufnahmen](https://www.fledermaus-bayern.de/downloads.html?file=files/upload/Downloads/bestimmungshilfen/wertung-artnachweise-lautanalyse.pdf) (PDF)
* [Kurzübersicht - Ruffrequenzen der Fledermausarten](https://www.fledermaus-bayern.de/downloads.html?file=files/upload/Downloads/bestimmungshilfen/feldf_hrer_frequenzen.pdf) (PDF)
* [Kleine Übersicht über die Rufe unserer Fledermäuse](https://www.fledermaus-bayern.de/downloads.html?file=files/upload/Downloads/bestimmungshilfen/rufe_einheimischer_flederm_use.pdf) (PDF)
* [Softwaresammlung (OSX/Windows) zur Analyse von Fledermausrufen](https://www.fledermausschutz.de/forschen/analyse-von-fledermausrufen/)
