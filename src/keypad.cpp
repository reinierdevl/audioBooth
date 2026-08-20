#include "keypad.h"

#include <Arduino.h>

#include "pins.h"

namespace {

constexpr uint32_t KEY_DEBOUNCE_MS = 25;

constexpr gpio_num_t ROW_PINS[] = {
    PIN_ROW0,
    PIN_ROW1,
    PIN_ROW2,
    PIN_ROW3,
};

constexpr gpio_num_t COLUMN_PINS[] = {
    PIN_COL0,
    PIN_COL1,
    PIN_COL2,
    PIN_COL3,
};

constexpr char KEY_LABELS_LAYOUT_1[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

constexpr char KEY_LABELS_LAYOUT_2[4][4] = {
    {'1', '4', '7', '*'},
    {'2', '5', '8', '0'},
    {'3', '6', '9', '#'},
    {'A', 'B', 'C', 'D'},
};

uint8_t keypadLayout = 1;
uint8_t lastScanRow = 0;
uint8_t lastScanColumn = 0;

char scanKeypad()
{
    for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
        // Only the row currently being scanned is driven. Leaving all other
        // rows high-impedance avoids current paths through the keypad matrix
        // which can otherwise hide keys on some membrane keypad boards.
        digitalWrite(ROW_PINS[rowIndex], LOW);
        delayMicroseconds(20);

        for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
            if (digitalRead(COLUMN_PINS[columnIndex]) == LOW) {
                lastScanRow = static_cast<uint8_t>(rowIndex);
                lastScanColumn = static_cast<uint8_t>(columnIndex);
                const char key = keypadLayout == 1
                    ? KEY_LABELS_LAYOUT_1[rowIndex][columnIndex]
                    : KEY_LABELS_LAYOUT_2[rowIndex][columnIndex];
                digitalWrite(ROW_PINS[rowIndex], HIGH);
                return key;
            }
        }

        digitalWrite(ROW_PINS[rowIndex], HIGH);
    }

    return '\0';
}

} // namespace

void beginKeypad()
{
    for (gpio_num_t row : ROW_PINS) {
        // With open-drain output mode, HIGH releases the line (high
        // impedance) and LOW selects it. This avoids reconfiguring GPIOs on
        // every scan and prevents rows from driving against each other.
        pinMode(row, OUTPUT_OPEN_DRAIN);
        digitalWrite(row, HIGH);
    }

    for (gpio_num_t column : COLUMN_PINS) {
        pinMode(column, INPUT_PULLUP);
    }
}

char keypadPressEvent()
{
    static char stableKey = '\0';
    static char lastReading = '\0';
    static uint32_t changedAt = 0;

    const char reading = scanKeypad();
    const uint32_t now = millis();

    if (reading != lastReading) {
        lastReading = reading;
        changedAt = now;
    }

    if (reading != stableKey && now - changedAt >= KEY_DEBOUNCE_MS) {
        stableKey = reading;

        // A release updates the debounced state but is not a press event.
        if (stableKey != '\0') {
            Serial.printf("Key matrix: row %u (GPIO %d), column %u (GPIO %d) -> %c\n",
                          static_cast<unsigned>(lastScanRow),
                          static_cast<int>(ROW_PINS[lastScanRow]),
                          static_cast<unsigned>(lastScanColumn),
                          static_cast<int>(COLUMN_PINS[lastScanColumn]),
                          stableKey);
            return stableKey;
        }
    }

    return '\0';
}

void setKeypadLayout(uint8_t layout)
{
    // Layout 1 is the safe default. Layout 2 must be selected explicitly.
    keypadLayout = layout == 2 ? 2 : 1;
}
