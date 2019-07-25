/*
Code by Jacob.Schultz 2019 Teensy forum
https://forum.pjrc.com/threads/54869-Reboot-out-of-control-in-hardfault-handler
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <Arduino.h>

namespace debug
{

void init(Stream &stream);
void dumpHex(uint8_t *data, int len);

// Causes all BusFaults to be precise, but cost performance.
void disableWriteBuffer(bool disabled);

} // namespace debug

#endif /* DEBUG_H_ */