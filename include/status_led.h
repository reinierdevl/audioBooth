#pragma once

// Initializes the onboard WS2812 and displays the current storage status.
// Pass false when a required hardware subsystem failed during startup.
void beginStatusLed(bool hardwareHealthy);

// Rechecks content after a successful web upload or deletion.
void refreshStatusLed();

// Latches the hardware-error state and changes the LED to red.
void setStatusHardwareError();
