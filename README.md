# Pico Watercooling Temperature Gauge

[![Firmware Build](https://github.com/Quphoria/PicoWatercoolingTempGauge/actions/workflows/firmware_build.yml/badge.svg)](https://github.com/Quphoria/PicoWatercoolingTempGauge/actions/workflows/firmware_build.yml)

Temperature Gauge for Barrow TCWDL-V1 10k NTC Watercooling Temperature sensor.

## Firmware Update

To put the RP2040 into bootloader mode to update the firmware send the command `update` followed by a newline to the USB Serial Port.  

## Circuit

Connect the temperature sensor between GPIO26 (ADC0) and GND, and connect the resistor between GPIO26 and 3V3.  

Connect the OLED to GND, 5V, SDA (GPIO12), SCL (GPIO13).  

__Parts:__
- [RP2040-Zero](https://www.aliexpress.com/item/1005006865919374.html)
- [Temperature Sensor](https://www.aliexpress.com/item/1005004355219588.html)
- [1.3" OLED](https://www.aliexpress.com/item/1005006862732813.html)
- 10k Resistor
