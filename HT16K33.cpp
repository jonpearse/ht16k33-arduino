/**********************************************************************
 *
 * This is a quick library for the Holtek HT16K33 LED driver + keyscan
 * IC. It’s not functionally exhaustive by any stretch, but it should
 * at least work reasonably well.
 *
 **********************************************************************/
#include <Wire.h>
#include "HT16K33.h"

/**
 * Utility function to flip a 16-bit integer. There may be better ways of doing this—let me know!
 */
uint16_t _flip_uint16(uint16_t in)
{
    uint16_t out = 0;

    for (uint8_t i = 0; i < 16; i++) {
        out <<= 1;
        out |= in & 1;
        in >>= 1;
    }

    return out;
}

/**
 * Constructor
 * Create a new instance of a HT16K33 chipset
 *
 * @param addr I2C address of the chip. It follows the format (binary) 1110<A2><A1><A0><RW> with the following notation:
 * A2-A0 can be controlled using 39k resistors and DIP switches (see datasheet). If not connected, they default to 0.
 *
 * For 28 PIN package, all three can be controlled
 * For 24 PIN package, A2 is always set to 0
 * For 20 PIN package, A2-A0 are set to 0
 *
 * The Arduino wire library handles the R/W bit for us. Looking at the datasheet, ignore the R/W bit and shift
 * the address one to the right to obtain the 7bit address required for this library.
 */
void HT16K33::init(uint8_t addr)
{
    // orientation flags
    resetOrientation();

    // set the I2C address
    _i2c_addr = addr;

    // assign + zero some buffer data
    _buffer = (uint16_t *) calloc(8, sizeof(uint16_t));

    // start everything
    Wire.begin();
    wakeUp();

    // set blink off + brightness all the way up
    setBlink(HT16K33_BLINK_OFF);
    setBrightness(15);

    // write the matrix, just in case
    write();
}

/**
 * Reads the entire key buffer from the chip and returns it in an 3x uint16_t array
 * Every array element represents a row (bits 0-12 are used, the rest can be ignored).
 * 1 means the key has been pressed.
 *
 * Note that the buffer is cleared after calling this function and that the interrupt PIN and register is also reset.
 *
 * @param _key_buffer
 */
void HT16K33::getKeyData(uint16_t _key_buffer[])
{

    // Write the register address we want to read
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_KEYS);
    Wire.endTransmission();

    //Start to read the entire register
    Wire.requestFrom(_i2c_addr, (uint8_t)6);
    for (uint8_t row = 0; row < 2; row++) {
        uint16_t lsb = Wire.read();
        _key_buffer[row] = (Wire.read() << 8 | lsb);
    }
}

/**
 * Configure ROW/INT PIN
 *
 * The ROW/INT PIN can either be used as an additional ROW pin for the display, or can act as interrupt pin for
 * the keyscan functionality. By default, it acts as a row pin.
 *
 * @var row_int: defines whether the pin should act as row (false) or interrupt (true)
 * @var act: Defines whether the INT should be active low (0) or active high (1). Ignored if `row_int` is set to false.
 */
void HT16K33::setRowIntPin(bool row_int, bool act)
{
    // send the command
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_ROWINT | (act & row_int) << 1 | row_int);
    Wire.endTransmission();
}

/**
 * Puts the HT16K33 to sleep. It will wake up if a key is pressed during the scan interval or when calling the
 * wakeUp() function again. This can greatly reduce power consumption if mainly used for key scanning.
 * The datasheet mentions a strong recommendation to read key data before putting the chip to sleep.
 */
void HT16K33::sleep()
{
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_SYSTEM & ~(1UL));
    Wire.endTransmission();
}

/**
 * Wakes up the HT16K33 from sleep. Wait at least 1ms after waking up for proper initialization before trying to
 * use the chip or to read data.
 */
void HT16K33::wakeUp()
{
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_SYSTEM | 0x01);
    Wire.endTransmission();
}

bool HT16K33::getKeyInterrupt()
{

    //Tell the IC which register to read
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_INTFLAG);
    Wire.endTransmission();

    uint8_t intflags;

    //Read the INT register
    Wire.requestFrom(_i2c_addr, (uint8_t)1);
    intflags = Wire.read();
    return (intflags > 0);
}

/**
 * Sets the brightness of the display.
 */
void HT16K33::setBrightness(uint8_t brightness)
{
    // constrain the brightness to a 4-bit number (0–15)
    brightness = brightness & 0x0F;

    // send the command
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_DIMMING | brightness);
    Wire.endTransmission();
}

/**
 * Set the blink rate.
 */
void HT16K33::setBlink(uint8_t blink)
{
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_SETUP | HT16K33_DISPLAY_ON | blink);
    Wire.endTransmission();
}

/**
 * Reset the matrix orientation./
 */
void HT16K33::resetOrientation(void)
{
    _reversed = false;
    _vFlipped = false;
    _hFlipped = false;
}

/**
 * Flips the order of the two 8x8 matrices: useful if you’ve wired them backward by mistake =)
 */
void HT16K33::reverse(void)
{
    _reversed = !_reversed;
}

/**
 * Flips the vertical orientation of the matrices.
 */
void HT16K33::flipVertical(void)
{
    _vFlipped = !_vFlipped;
}

/**
 * Flips the vertical orientation of the matrices.
 */
void HT16K33::flipHorizontal(void)
{
    _hFlipped = !_hFlipped;;
}


/**
 * Clears the display buffer. Note that this doesn’t clear the display—you’ll need to call write() to do this.
 */
void HT16K33::clear(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        _buffer[i] = 0;
    }
}

/**
 * Sets the value of a particular pixel.
 */
void HT16K33::setPixel(uint8_t col, uint8_t row, uint8_t val)
{
    // bounds checking
    col = col & 0x0F;
    row = row & 0x07;
    val = val & 0x01;

    // write the buffer
    if (val == 1) {
        _buffer[row] |= 1 << col;
    } else {
        _buffer[row] &= ~(1 << col);
    }

}

/**
 * Sets the value of an entire row.
 */
void HT16K33::setRow(uint8_t row, uint16_t value)
{
    // bound check the row
    row = row & 0x07;

    // write it
    _buffer[row] = value;
}

/**
 * Set the value of an entire column. This is more fun =)
 */
void HT16K33::setColumn(uint8_t col, uint8_t value)
{
    // just do this via set pixel—waaaay easier!
    for (uint8_t row = 0; row < 8; row++) {
        setPixel(col, row, (value & (1 << row)) > 0);
    }
}

/**
 * Bulk-writes a set of row data to the display.
 */
void HT16K33::drawSprite16(Sprite16 sprite, uint8_t colOffset, uint8_t rowOffset)
{
    // iterate through data and set stuff
    for (uint8_t row = 0; row < sprite.height(); row++) {
        _buffer[(row + rowOffset) & 0x07] |= (sprite.readRow(row) << colOffset) & 0xFFFF;
    }

}

/**
 * Same as the above, just without offsets.
 */
void HT16K33::drawSprite16(Sprite16 sprite)
{
    drawSprite16(sprite, 0, 0);
}

/**
 * Write the RAM buffer to the matrix.
 */
void HT16K33::write(void)
{
    Wire.beginTransmission(_i2c_addr);
    Wire.write(HT16K33_CMD_RAM);

    for (uint8_t row = 0; row < 8; row++) {
        writeRow(row);
    }

    Wire.endTransmission();
}

/**
 * Write a row to the chip.
 */
void HT16K33::writeRow(uint8_t row)
{
    // flip vertically
    if (_vFlipped) {
        row = 7 - row;
    }

    // read out the buffer so we can flip horizontally
    uint16_t out = _buffer[row];
    if (_hFlipped) {
        out = _flip_uint16(out);
    }

    if (_reversed) {
        Wire.write(out >> 8); // second byte
        Wire.write(out & 0xFF); // first byte
    } else {
        Wire.write(out & 0xFF); // first byte
        Wire.write(out >> 8); // second byte
    }
}