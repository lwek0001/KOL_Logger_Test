
## Current Functionality
* ESP32 acts as a wifi access point (AP)
* Task that creates dummy current (A) data
* ESP32 hosts dynamic website displaying dummy current (A) data
* LCD screen (ili9341) displays a rough graph using SPI

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


