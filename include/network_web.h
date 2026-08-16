#pragma once

// Starts Wi-Fi once and registers the storage web interface.
void beginNetworkAndWeb();

// Advances the initial Wi-Fi connection attempt and serves HTTP requests.
// Call frequently from loop().
void serviceNetworkAndWeb();
