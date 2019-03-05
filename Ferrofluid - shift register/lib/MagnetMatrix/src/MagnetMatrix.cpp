#include <Arduino.h>
#include "MagnetMatrix.h"

//Constructor
MagnetMatrix::MagnetMatrix(int shiftEnablePins[], int shiftClkPins[], int shiftDataPins[], int numRegisters)
{
  //Pin connected to RCLK of SN74HC595
  _shiftEnablePins = shiftEnablePins;//{2,5,8,11,14,17,20,23,26,29};   //RCLK
  //Pin connected to SRCLK of SN74HC595
  _shiftClkPins = shiftClkPins;//{3,6,9,12,15,18,21,24,27,30};      //SRCLK
  //Pin connected to SER of SN74HC595
  _shiftDataPins = shiftDataPins;//{4,7,10,13,16,19,22,25,28,31};    //SER
  _numRegisters = numRegisters;
  Serial.begin(9600);
}

// Public Methods //////////////////////////////////////////////////////////////
// Functions available in Wiring sketches, this library, and other libraries

void MagnetMatrix::printPinValues(void)
{
  // eventhough this function is public, it can access
  // and modify this library's private variables
  Serial.println("Pin arrays:");
  Serial.println("Enable,\tClk,\tData");
  for(unsigned int i = 0; i<_numRegisters; i++){
    Serial.print(_shiftEnablePins[i]);
    Serial.print(",\t");
    Serial.print(_shiftClkPins[i]);
    Serial.print(",\t");
    Serial.println(_shiftDataPins[i]);
  }

  // it can also call private functions of this library
  doSomethingSecret();
}

// Private Methods /////////////////////////////////////////////////////////////
// Functions only available to other functions in this library

void MagnetMatrix::doSomethingSecret(void)
{
  Serial.println("Private");
}
