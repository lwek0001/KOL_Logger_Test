
## Current Functionality
* ESP32 acts as a wifi access point (AP)
* ESP32 hosts dynamic website displaying dummy current (A) data
* LCD screen (ili9341) displays a rough dynamic graph of the dummy current data using SPI

## TO-DO
* Polish graph display on LCD screen
* Store data in SD card
* Obtain real current readings
* Design Printed Circuit Board
* Test Printed Circuit Board

## Configure the project (just wifi settings)

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.


