The Waveshare esp32-p4-wifi6-touch-lcd-3.4c doesn't support ESPNOW.  My project need that to function so I created this C6 firmware.

It adds espnow support for my project, along with OTA update support for both the C6 and P4.  It is controlled from the screen with the matching firmware for maingauge in this project.

In telemetry mode, it connects to the ESPNOW sender reads the gaugepacket, then sends it via serial to the P4.  Where the data is displayed.

