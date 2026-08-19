Runs on the Waveshare esp32-p4-wifi6-touch-lcd-3.4c board.

Extensive work done to optimize GUI performance as this is an analog MPF/RPM dial.  This causes large portions of the screen to invalidate and drives the hardware very hard.  

Uses double buffering in SRAM, and may other tweaks.  Can get ~50-60 fps at ~50-60% CPU, results in nice needle movement.

Significantly better than the stock driver setup provided by Waveshare in their demos.
