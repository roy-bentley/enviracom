# Honeywell Enviracom Control Application for Unix 

## About

Enviracom is a protocol spoken by a subset of Honeywell thermostats and other HVAC hardware. This software provides a unix daemon which monitors Enviracom communications, allow the inspection of thermostat information, and provide a simple command line interface for controlling the operation of the thermostat.

The code was developed for NetBSD, and was untested under Linux. 

## Documentation
The [API](./docs/API_README.md) and [protocol](./docs/PROTOCOL.md) are well documented. There is also a [spec document](./docs/reference/W8735A1005_SPECS.pdf) for the required serial controller.

## Requirements

* A Thermostat and Contol module that speaks Enviracom. This code was developed against a VisionPro IAQ Touchscreen programmable thermostat (TH9421C1004), and equipment inteface module (THM5421C1008) bought as a kit (YTH9421C1002)

* A serial port or USB-serial adaptor.

* An Enviracom Serial Adaptor model W8735A/W8735A1005. These were discontinued in 2013 and may not even be available. The documentation for this module is included in this repository.


## Eratta

This repo is forked from the [Enviracom](https://sourceforge.net/projects/Enviracom/) project found on SourceForge by Scott Presnell. It was last updated 2013-03-25, besides updating the documentation to markdown.
