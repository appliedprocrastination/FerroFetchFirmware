/*
  Test.h - Test library for Wiring - description
  Copyright (c) 2006 John Doe.  All right reserved.
*/

// ensure this library description is only included once
#ifndef MagnetMatrix_h
#define MagnetMatrix_h

// include types & constants of Wiring core API
#include <Arduino.h>

// library interface description
class MagnetMatrix
{
  // user-accessible "public" interface
  public:
    MagnetMatrix(int shiftEnablePins[], int shiftClkPins[], int shiftDataPins[], int numRegisters);
    void printPinValues(void);

  // library-accessible "private" interface
  private:
    //Pin connected to RCLK of SN74HC595
    int *_shiftEnablePins;   //RCLK
    //Pin connected to SRCLK of SN74HC595
    int *_shiftClkPins;      //SRCLK
    ////Pin connected to SER of SN74HC595
    int *_shiftDataPins;     //SER

    int _numRegisters;
    void doSomethingSecret(void);
};

#endif
