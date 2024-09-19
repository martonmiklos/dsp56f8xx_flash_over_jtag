# Motorola/FreeScale/NXP DSP56f8xx flasher

JTAG flasher program originally developed by Motorola patched to use FTDI devices instead of LPT port.

I needed this utility to dump and reflash a DSP56F803 in a Life Fitness 95T threadmill VFD.

## Things to note!

The patching was optimized for time (improving the existing code was not considered at all!).

I consider this as a disposable project, not planning to maintaining it.

Code is not tested with HW yet, just compiles and detects the FT232H interface. Once it is done README.md will be updated.

Pinout with an FT232H
- RESET - TXD
- TMS - RXD
- TCK - RTS
- TDI - CRS
- TRST - DTR
- TDO - DSR

## Build system

Yes qmake. I know it is getting obsolete, but still this was the easiest way for me to hook up.

## Original sources

Original sources publised by xiangjun_rong here:

https://community.nxp.com/t5/Other-NXP-Products/Programming-DSP56F803BU80E-over-JTAG/m-p/1688265/highlight/true#M18500

## License

* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.

# p.s.

It is always fun to patch codes decades old in a manner "just gettin' it working somehow and get the job done with it".
