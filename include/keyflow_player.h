#pragma once

// Resets to root and starts its ini/main file.
void beginKeyflowSession();

// Stops playback and resets the next session to root.
void endKeyflowSession();

// Handles a debounced key. Unmapped keys leave current playback untouched.
void handleKeyflowKey(char key);

// Handles audio end events and delayed no_key behavior.
void serviceKeyflowPlayer();

const char *activeKeyflowDirectory();
