leitshow
========

Hardware and software for making LED strips glow in interesting ways while music is playing.

Mainly created by Matt Peddie and Henry Hallam; inspired by the world-famous tEp leit show.

### Principles of operation:

Hardware:
 - STM32F4 microcontroller (168 MHz 32-bit ARM Cortex-M4)
 - N output channels (4 in prototype), each provides PWMed 12 volts at up to several amps, using low-cost MOSFETs
 - Stereo jack audio input at line level with pass-through for connection to speakers.  AC-coupled to microcontroller
   ADC pins, resistor-biased at half of the 3V microcontroller supply.
 - USB I/O to PC for firmware updates, tuning, and advanced shit.

Software:
 - ADC is sampled at 11 kHz, feeding a bank of IIR filters from which several bandpass bins are constructed across
   a range of audio frequencies.
 - Bins are low-pass-filtered, and a set of gains and cutoff thresholds are applied to form the LED PWM duty cycles.
 - The gains and thresholds are adaptively adjusted to maintain a certain goal activity level and mean brightness.
