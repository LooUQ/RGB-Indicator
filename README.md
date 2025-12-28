# RGB Indicator
Sample application, driver and devicetree overlay for TI LP5817 RGB LED controller

## MTC.2 Host Extension
LooUQ uses an I2C bus for what the MTC.2 interface specification calls the "host extension". The host extension is intend as a supplemental set of hardware features that an MTC.2 device can use for additional functionality.

Most LooUQ MTC.2 boards include an RGB LED for a simple human interface. To allow for more meaningful signalling we have a driver to support flashing various patterns on the LED. The standard implementation uses a TI LP5817  chip to control the tri-color LED. The TI controller is I2C based (standard 100kHz only).

Other items that LooUQ places on the Host Extension interface include: 

* Bosch BMP-581 temperature/pressure/humidity (optional with Sensor-1)
* ST Microelectronics IMU (optional with Sensor-1)
* Future I2C base sensors/multiplexors

## Using the RGB Indicator
To support the RGB indicator your design you need to add a couple of parts to your PCB.
* TI LP5817
* Everlight Electronics (or equivalent) EAST1616RGBB4

To use the above infrastructure use the overlay provided in this sample to add the LP5817 to the I2C bus. All the software for controlling the LED is found in the rgb-indicator.c and rgb-indicator.h files.

In the sample, main.c demonstrates initialization and performing several indicator patterns.

#Yes... you can communicate with a single LED, the colors help too. 