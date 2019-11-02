/**
 * This is a quick demo of the keyscan function of the controller, using the ROW/INT pin as interrupt, connected to an
 * interrupt pin on Arduino.
 */
#include "HT16K33.h"

HT16K33 matrix = HT16K33();

uint16_t keybuffer[3];

volatile bool has_key_data = false;

const byte interruptPin = 2;

void setup()
{
    //Serial port for debugging in Arduino IDE
    Serial.begin(115200);

    // wait a few ms to give the controller time to initialize
    delay(10);

    //Connect to I2C interface (7 bit address, ignore R/W bit in datasheet and shift it one to the right)
    matrix.init(0x70);

    //Turn on oscillator
    matrix.wakeUp();

    //Instruct HT16K33 to use interrupt pin, high active
    matrix.setRowIntPin(true, true);

    //Configure PIN 2 as interrupt pin - as HT16K33 is instructed to send an active high interrupt, watch for rising edge
    attachInterrupt(digitalPinToInterrupt(interruptPin), handle_int, RISING);

    //Zerosize the key buffer, not necessarily needed though
    memset(keybuffer, 0, sizeof(keybuffer));
}

/*
 * Interrupt handler, will be called when the hardware interrupt condition is triggered
 */
void handle_int()
{
    has_key_data = true;
}

void loop()
{
    // If interrupt has beenm called, fetch key data
    if (has_key_data) {

        // Reset flag to false
        has_key_data = false;

        // Fetch key data matrix
        matrix.getKeyData(keybuffer);

        //Debug output to Arduino IDE serial connection to understand data format
        for (int i = 0; i < 2; i++) {
            if (keybuffer[i] != 0) {
                Serial.print("Key has been pressed in row ");
                Serial.print(i);
                Serial.print(": ");
                Serial.println(keybuffer[i], BIN);
            }
        }
    }
}