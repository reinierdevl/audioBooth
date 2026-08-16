#pragma once

#include <cstdint>

// Configures the 4x4 matrix keypad.
void beginKeypad();

// Returns the key once when it has remained pressed for the debounce period.
// Returns '\0' when there is no new key press. The connector is transposed:
// ROW0: 1 4 7 *
// ROW1: 2 5 8 0
// ROW2: 3 6 9 #
// ROW3: A B C D
char keypadPressEvent();

// Layout 1: ROW0 = 1,2,3,A. Layout 2: ROW0 = 1,4,7,* (default).
void setKeypadLayout(uint8_t layout);
