#include <Arduino.h>

#define SHIFT_INDICATOR_PIN 50

const bool DEBUG = false;
int counter = 0;                //debugvariabler
unsigned long startTime = 0;    //debugvariabler
//Pin connected to SRCLK of SN74HC595
int shiftClkPins[] = {2,5,8,11,14,17,20,23,26,29};      //SRCLK, NOTE: the variable "ALL_ROWS" will change how many of these pinsare initiated
//Pin connected to RCLK of SN74HC595
int shiftEnablePins[] = {3,6,9,12,15,18,21,24,27,30};   //RCLK, NOTE: the variable "ALL_ROWS" will change how many of these pinsare initiated
////Pin connected to SER of SN74HC595
int shiftDataPins[] = {4,7,10,13,16,19,22,25,28,31};    //SER, NOTE: the variable "ALL_ROWS" will change how many of these pinsare initiated



//holders for infromation you're going to pass to shifting function
const int ALL_ROWS = 10; //The total number of rows in the actual hardware
const int ALL_COLS = 19; //The total number of columns in the actual hardware
const int ROWS = 8;// 5;//10;//12 //The number of rows that are in use in the current program (different from ALL_ROWS in order to scale down the number of bits shifted out)
const int COLS = 19; // 5;//19;//21 //The number of cols that are in use in the current program (different from ALL_COLS in order to scale down the number of bits shifted out)

const int REGISTERS = ROWS; // no of register series (indicating no of magnet-driver-PCBs connected to the Arduino)
const int BYTES_PER_REGISTER = 4; // no of 8-bit shift registers in series per line (4 = 32 bits(/magnets))

//Timekeeping variables:
float timeTilStart[COLS][ROWS]; //The time in ms until a magnet should start. Used to make movement patterns before the movement should happen
float timeAtStart[COLS][ROWS]; //The time in ms when a magnet was started. Used as a safety feature so that a magnet doesn't stay turned on too long.
float timeAtEnd[COLS][ROWS]; //The duration a magnet should stay turned on once it's been turned on.
unsigned long timeThisRefresh = 0;
unsigned long timeForNewRefresh = 0;

//Other movement related variables
uint8_t dutyCycle[COLS][ROWS]; //Probably not needed, just creating it to remember thinking more about it later
uint8_t dutyCycleCounter = 0;
uint8_t dutyCycleResolution = 20;
int shortDelay = 500; //ms
int longDelay = 1000; //ms
unsigned long timeBetweenRefreshes = 6000;//12*shortDelay; //ms

int frame = 1;

//States
enum MagState{OFF, ON};
MagState prevMagnetState[COLS][ROWS];
MagState currMagnetState[COLS][ROWS];

//Function headers:
void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent
void turnMagnetsOnIn(int* xArr, int y,int xLength, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent

//Functions:
boolean magnetIsToggledThisIteration(int x, int y){
  if(prevMagnetState[x][y] == OFF && currMagnetState[x][y] == ON){
    return true;
  }
  return false;
}

void updateTimeAtStart(int x, int y){
  if(magnetIsToggledThisIteration(x,y)){
    timeAtStart[x][y] = timeThisRefresh;
  }
}

void updateAllStates(){
  for (int x = 0; x < COLS; x++) {
    for (int y = 0; y < ROWS; y++) {
      if(timeTilStart[x][y] <= timeThisRefresh && timeAtEnd[x][y]>timeThisRefresh){
        currMagnetState[x][y] = ON;

        if(dutyCycle[x][y] < dutyCycleCounter){
          currMagnetState[x][y] = OFF;
        }

        if(DEBUG && false){ //Using &&false when I don't want this output
          Serial.print("Magnet (");
          Serial.print(x);
          Serial.print(",");
          Serial.print(y);
          Serial.println(") will turn ON this iteration: ");
        }

      }else if (timeAtEnd[x][y] <= timeThisRefresh ) {
        currMagnetState[x][y] = OFF; //unnecessary? These are turned OFF in movementAlgorithm()

      } else {

      }
    }
  }
  dutyCycleCounter++;
  if(dutyCycleCounter>=dutyCycleResolution) dutyCycleCounter=0;
}

void shiftOut(int registerIndex){
  //This shifts (8*BitsPerRegister) bits out (TODO: was MSB first when the loops counted down from 7, how is this now?),
  //on the rising edge of the clock,
  //clock idles low

  //Register-index must be translated to display-coordinates
  //For bit n < COLS: y=registerIndex (The m=21 MSB represents the y'th row)
  //For bit n >=COLS: TODO: Yet to be decided (spread out on the ten PCBs somehow)
  int x;
  int y = registerIndex;
  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(shiftDataPins[registerIndex], LOW);
  digitalWrite(shiftClkPins[registerIndex], LOW);
  //Write out the COLS = 21 first values:
  for(x=0; x<COLS; x++){
    digitalWrite(shiftClkPins[registerIndex], LOW);

    //Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(shiftDataPins[registerIndex], currMagnetState[x][y]);
    updateTimeAtStart(x,y);
    //register shifts bits on upstroke of clock pin
    digitalWrite(shiftClkPins[registerIndex], HIGH);
    //zero the data pin after shift to prevent bleed through
    digitalWrite(shiftDataPins[registerIndex], LOW);
  }
  //Write out the remaining bits:
  y = 0; //TODO: Translate this when decided
  for(x = COLS; x<(8*BYTES_PER_REGISTER); x++){
      //Translate pattern for remaining bits
      digitalWrite(shiftClkPins[registerIndex], LOW);

      digitalWrite(shiftDataPins[registerIndex], 0); //TODO: This is temporarily ignored magnetState needs to be set
      updateTimeAtStart(x,y);
      digitalWrite(shiftClkPins[registerIndex], HIGH);
      digitalWrite(shiftDataPins[registerIndex], LOW);
  }
    //stop shifting
    digitalWrite(shiftClkPins[registerIndex], LOW);
}

void shiftOut_one_PCB_per_row(int registerIndex){
  // This function does the same job as shiftOut() except that it doesnt have a special case routine for handling the two leftover rows that exist in the planned 12x23 matrix with 10 driver-PCBs
  // This function is intended for smaller matrices (like the prototype, which has 5x6 magnets), where there are several rows controlled by the same PCB

  //This shifts (COLS*ROWS) bits out to the register at registerIndex,
  //on the rising edge of the clock,
  //clock idles low

  //Register-index must be translated to display-coordinates:
  //The following algorithm assumes that the magnets are connected row by row, meaning that
  //The n=[0,...,COLS)      first bits corresponds to row[0],
  //The n=[COLS,...,2*COLS) first bits corresponds to row[1], etc.
  int x;
  int y = registerIndex;

  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(shiftDataPins[registerIndex], LOW);
  digitalWrite(shiftClkPins[registerIndex], LOW);

  //Write out the ROWS*COLS first values:
  for(x=COLS-1; x>=0; x--){
      //NOTE: The first thing written will be pushed to the very end of the register.
      digitalWrite(shiftClkPins[registerIndex], LOW);
      //Sets the pin to HIGH or LOW depending on pinState

      digitalWrite(shiftDataPins[registerIndex], currMagnetState[x][y]);
      if(DEBUG && timeThisRefresh >= timeForNewRefresh){
        Serial.print("\t(");
        Serial.print(x);
        Serial.print(",");
        Serial.print(y);
        Serial.print(") = ");
        Serial.print(currMagnetState[x][y]);
      }

      updateTimeAtStart(x,y);

      //register shifts bits on upstroke of clock pin
      digitalWrite(shiftClkPins[registerIndex], HIGH);
      //zero the data pin after shift to prevent bleed through
      digitalWrite(shiftDataPins[registerIndex], LOW);
    }
    if(DEBUG && timeThisRefresh >= timeForNewRefresh)Serial.println();

  //stop shifting
  digitalWrite(shiftClkPins[registerIndex], LOW);
}

void shiftOut_zeros_to_full_screen(void)
{
  // Shifts out zeros to all connected shift registers, regardless of how many are supposed to be used in the rest of the code.
  // This is in order to avoid registers with unknown states in pixels that are outside the current "online matrix".
  // What is meant by "online matrix" is the matrix that is refreshed on every "refresh screen", indicated by the ROWS and COLS constants.
  int x;
  int y;

  //clear everything out just in case to
  //prepare shift register for bit shifting
  for (int registerIndex = 0; registerIndex < ALL_ROWS; registerIndex++)
  {
    digitalWrite(shiftDataPins[registerIndex], LOW);
    digitalWrite(shiftClkPins[registerIndex], LOW);

    //Write out the ROWS*COLS first values:
    for ( y = ALL_ROWS - 1; y >= 0; y--)
    {
      for (x = ALL_COLS - 1; x >= 0; x--)
      {
        //NOTE: The first thing written will be pushed to the very end of the register.
        digitalWrite(shiftClkPins[registerIndex], LOW);
        //Sets the pin to HIGH or LOW depending on pinState

        digitalWrite(shiftDataPins[registerIndex], 0);

        //updateTimeAtStart(x, y);

        //register shifts bits on upstroke of clock pin
        digitalWrite(shiftClkPins[registerIndex], HIGH);
        //zero the data pin after shift to prevent bleed through
        digitalWrite(shiftDataPins[registerIndex], LOW);
      }
    }
    //stop shifting
    digitalWrite(shiftClkPins[registerIndex], LOW);
  }
  
}

void refreshScreen(){
  updateAllStates();

  //indicate to logic analyzer that the shift is starting 
  //(The latch pin could be used to achieve this functionality, 
  //but the current version of the software latches through each register separately )
  digitalWrite(SHIFT_INDICATOR_PIN,HIGH);

  for(int reg = 0; reg < REGISTERS; reg++){
    //ground latchPin and hold low for as long as you are transmitting
    digitalWrite(shiftEnablePins[reg], LOW);
    //move 'em out
    //shiftOut()
    shiftOut_one_PCB_per_row(reg);
    //return the latch pin high to signal chip that it
    //no longer needs to listen for information
    digitalWrite(shiftEnablePins[reg], HIGH);
  }
  
  //Indicate to logic analyzer that the shift is done
  digitalWrite(SHIFT_INDICATOR_PIN, LOW);

}

void sefMovement(){
  if(frame == 1){
    int t = 0;
    int distanceBetween = 2;
    turnMagnetOnIn(4,0,t+0,500);
    turnMagnetOnIn(4,1,t+shortDelay,longDelay);
    turnMagnetOnIn(4,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(4,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(4,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t = shortDelay*distanceBetween;
    turnMagnetOnIn(3,0,t+0,500);
    turnMagnetOnIn(3,1,t+shortDelay,longDelay);
    turnMagnetOnIn(3,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(3,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(3,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t += shortDelay*distanceBetween;
    turnMagnetOnIn(2,0,t+0,500);
    turnMagnetOnIn(2,1,t+shortDelay,longDelay);
    turnMagnetOnIn(2,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(2,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(2,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t += shortDelay*distanceBetween;
    turnMagnetOnIn(1,0,t+0,500);
    turnMagnetOnIn(1,1,t+shortDelay,longDelay);
    turnMagnetOnIn(1,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(1,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(1,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t += shortDelay*distanceBetween;
    turnMagnetOnIn(0,0,t+0,500);
    turnMagnetOnIn(0,1,t+shortDelay,longDelay);
    turnMagnetOnIn(0,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(0,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(0,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));

    frame++;
  }else if(frame == 2){
    // Plot S
    turnMagnetOnIn(1,0,0,timeBetweenRefreshes,45);
    turnMagnetOnIn(2,0,0,timeBetweenRefreshes,45);
    turnMagnetOnIn(3,0,0,timeBetweenRefreshes,45);

    turnMagnetOnIn(3,1,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,2,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,3,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  }else if(frame == 3){
    //plot E

    turnMagnetOnIn(1,0,shortDelay,timeBetweenRefreshes-shortDelay,45);
    turnMagnetOnIn(2,0,shortDelay,timeBetweenRefreshes-shortDelay,45);
    turnMagnetOnIn(3,0,shortDelay,timeBetweenRefreshes-shortDelay,45);

    turnMagnetOnIn(1,1,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,2,0,timeBetweenRefreshes,75);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,2,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,3,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  }else if(frame == 4){
    //Plot F
    //Row 0
    turnMagnetOnIn(1,0,longDelay,timeBetweenRefreshes,45);
    //turnMagnetOnIn(2,0,shortDelay,shortDelay);
    //turnMagnetOnIn(3,0,0,longDelay);

    //Row 1
    turnMagnetOnIn(1,1,longDelay,timeBetweenRefreshes,75);
    //turnMagnetOnIn(2,1,0,longDelay+shortDelay);
    //turnMagnetOnIn(3,1,0,longDelay+shortDelay);
    //Row 2
    turnMagnetOnIn(1,2,shortDelay,timeBetweenRefreshes);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,2,0,timeBetweenRefreshes);
    //Row 3
    turnMagnetOnIn(1,3,0,timeBetweenRefreshes);
    //Row 4
    turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  } else if(frame == 5){
    //Drag everything down
    //Row 0
    turnMagnetOnIn(1,0,0,shortDelay*5,45);
    turnMagnetOnIn(2,0,0,shortDelay*5,45);
    turnMagnetOnIn(3,0,0,shortDelay*5,45);

    //Row 1
    turnMagnetOnIn(1,1,0,shortDelay*4,75);
    turnMagnetOnIn(2,1,0,shortDelay*4);
    turnMagnetOnIn(3,1,0,shortDelay*4,75);
    //Row 2
    turnMagnetOnIn(1,2,0,shortDelay*3);
    turnMagnetOnIn(2,2,0,shortDelay*3);
    turnMagnetOnIn(3,2,0,shortDelay*3);
    //Row 3
    turnMagnetOnIn(1,3,0,shortDelay*2);
    turnMagnetOnIn(2,3,0,shortDelay*2);
    turnMagnetOnIn(3,3,0,shortDelay*2);
    //Row 4
    turnMagnetOnIn(1,4,0,shortDelay);
    turnMagnetOnIn(2,4,0,shortDelay);
    turnMagnetOnIn(3,4,0,shortDelay);

    frame++;
  }    else{



    frame = 1;
  }

}

void heiMovement(){
  if(frame == 1){
    int t = 0;
    int distanceBetween = 2;
    turnMagnetOnIn(4,0,t+0,500);
    turnMagnetOnIn(4,1,t+shortDelay,longDelay);
    turnMagnetOnIn(4,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(4,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(4,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t = shortDelay*distanceBetween;
    turnMagnetOnIn(3,0,t+0,500);
    turnMagnetOnIn(3,1,t+shortDelay,longDelay);
    turnMagnetOnIn(3,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(3,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(3,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t += shortDelay*distanceBetween;
    turnMagnetOnIn(2,0,t+0,500);
    turnMagnetOnIn(2,1,t+shortDelay,longDelay);
    turnMagnetOnIn(2,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(2,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(2,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t += shortDelay*distanceBetween;
    turnMagnetOnIn(1,0,t+0,500);
    turnMagnetOnIn(1,1,t+shortDelay,longDelay);
    turnMagnetOnIn(1,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(1,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(1,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    t += shortDelay*distanceBetween;
    turnMagnetOnIn(0,0,t+0,500);
    turnMagnetOnIn(0,1,t+shortDelay,longDelay);
    turnMagnetOnIn(0,2,t+shortDelay*2,longDelay);
    turnMagnetOnIn(0,3,t+shortDelay*3,longDelay);
    turnMagnetOnIn(0,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));

    frame++;
  }else if(frame == 2){
    // Plot H
    turnMagnetOnIn(0,0,0,longDelay);
    turnMagnetOnIn(1,0,0,timeBetweenRefreshes,35);
    turnMagnetOnIn(2,0,0,shortDelay);
    turnMagnetOnIn(3,0,0,timeBetweenRefreshes,35);
    turnMagnetOnIn(4,0,0,longDelay);

    turnMagnetOnIn(0,1,0,longDelay);
    turnMagnetOnIn(1,1,0,timeBetweenRefreshes,90);
    turnMagnetOnIn(2,1,0,longDelay);
    turnMagnetOnIn(3,1,0,timeBetweenRefreshes,90);
    turnMagnetOnIn(4,1,0,longDelay);

    turnMagnetOnIn(1,2,0,timeBetweenRefreshes,95);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,2,0,timeBetweenRefreshes,95);

    turnMagnetOnIn(1,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,3,9,timeBetweenRefreshes);

    turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    //turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  }else if(frame == 3){
    //plot E

    turnMagnetOnIn(1,0,shortDelay,timeBetweenRefreshes-shortDelay,35);
    turnMagnetOnIn(2,0,0,timeBetweenRefreshes,35);
    turnMagnetOnIn(3,0,shortDelay,timeBetweenRefreshes-shortDelay,35);

    turnMagnetOnIn(1,1,0,timeBetweenRefreshes,95);

    turnMagnetOnIn(1,2,0,timeBetweenRefreshes,75);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,2,longDelay,timeBetweenRefreshes-longDelay);

    turnMagnetOnIn(1,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,3,0,longDelay);

    turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  }else if(frame == 4){
    //Plot I
    //Row 0
    //turnMagnetOnIn(1,0,longDelay,timeBetweenRefreshes,45);
    //turnMagnetOnIn(2,0,0,timeBetweenRefreshes,20);
    //turnMagnetOnIn(3,0,0,longDelay);

    //Row 1
    //turnMagnetOnIn(1,1,longDelay,timeBetweenRefreshes,75);
    turnMagnetOnIn(2,1,shortDelay*3,timeBetweenRefreshes,65);
    //turnMagnetOnIn(3,1,0,longDelay+shortDelay);
    //Row 2
    //turnMagnetOnIn(1,2,shortDelay,timeBetweenRefreshes);
    turnMagnetOnIn(2,2,shortDelay*2,timeBetweenRefreshes,90);
    //turnMagnetOnIn(3,2,0,timeBetweenRefreshes);
    //Row 3
    turnMagnetOnIn(2,3,shortDelay,timeBetweenRefreshes);
    //Row 4
    //turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    //turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  } else if(frame == 5){
    //Drag everything down
    //Row 0
    turnMagnetOnIn(1,0,0,shortDelay*5,45);
    turnMagnetOnIn(2,0,0,shortDelay*5,45);
    turnMagnetOnIn(3,0,0,shortDelay*5,45);

    //Row 1
    turnMagnetOnIn(1,1,0,shortDelay*4,75);
    turnMagnetOnIn(2,1,0,shortDelay*4);
    turnMagnetOnIn(3,1,0,shortDelay*4,75);
    //Row 2
    turnMagnetOnIn(1,2,0,shortDelay*3);
    turnMagnetOnIn(2,2,0,shortDelay*3);
    turnMagnetOnIn(3,2,0,shortDelay*3);
    //Row 3
    turnMagnetOnIn(1,3,0,shortDelay*2);
    turnMagnetOnIn(2,3,0,shortDelay*2);
    turnMagnetOnIn(3,3,0,shortDelay*2);
    //Row 4
    turnMagnetOnIn(1,4,0,shortDelay);
    turnMagnetOnIn(2,4,0,shortDelay);
    turnMagnetOnIn(3,4,0,shortDelay);

    frame++;
  }    else{



    frame = 1;
  }


}

void tMovement(){
  if(frame == 1){
    int t = 0;
    int distanceBetween = 2;
    turnMagnetOnIn(3, 0, t + 0, 500);
    turnMagnetOnIn(3, 1, t + shortDelay, longDelay);
    turnMagnetOnIn(3, 2, t + shortDelay * 2, longDelay);
    turnMagnetOnIn(3, 3, t + shortDelay * 3, longDelay);
    turnMagnetOnIn(3, 4, t + shortDelay * 4, longDelay);
    turnMagnetOnIn(3, 5, t + shortDelay * 5, timeBetweenRefreshes - (t + shortDelay * 5));
    t = shortDelay * distanceBetween;
    turnMagnetOnIn(2, 0, t + 0, 500);
    turnMagnetOnIn(2, 1, t + shortDelay, longDelay);
    turnMagnetOnIn(2, 2, t + shortDelay * 2, longDelay);
    turnMagnetOnIn(2, 3, t + shortDelay * 3, longDelay);
    turnMagnetOnIn(2, 4, t + shortDelay * 4, longDelay);
    turnMagnetOnIn(2, 5, t + shortDelay * 5, timeBetweenRefreshes - (t + shortDelay * 5));
    t += shortDelay * distanceBetween;
    turnMagnetOnIn(1, 0, t + 0, 500);
    turnMagnetOnIn(1, 1, t + shortDelay, longDelay);
    turnMagnetOnIn(1, 2, t + shortDelay * 2, longDelay);
    turnMagnetOnIn(1, 3, t + shortDelay * 3, longDelay);
    turnMagnetOnIn(1, 4, t + shortDelay * 4, longDelay);
    turnMagnetOnIn(1, 5, t + shortDelay * 5, timeBetweenRefreshes - (t + shortDelay * 5));
    //t += shortDelay*distanceBetween;
    //turnMagnetOnIn(1,0,t+0,500);
    //turnMagnetOnIn(1,1,t+shortDelay,longDelay);
    //turnMagnetOnIn(1,2,t+shortDelay*2,longDelay);
    //turnMagnetOnIn(1,3,t+shortDelay*3,longDelay);
    //turnMagnetOnIn(1,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    //t += shortDelay*distanceBetween;
    //turnMagnetOnIn(0,0,t+0,500);
    //turnMagnetOnIn(0,1,t+shortDelay,longDelay);
    //turnMagnetOnIn(0,2,t+shortDelay*2,longDelay);
    //turnMagnetOnIn(0,3,t+shortDelay*3,longDelay);
    //turnMagnetOnIn(0,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));

    frame++;
  }else if(frame == 2){
    // Plot T


    //turnMagnetOnIn(1,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(2,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(3,0,0,timeBetweenRefreshes,45);

    //turnMagnetOnIn(0,1,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(1,1,0,timeBetweenRefreshes,90);
    turnMagnetOnIn(2,1,0,shortDelay);
    //turnMagnetOnIn(3,1,0,timeBetweenRefreshes,90);
    //turnMagnetOnIn(4,1,0,timeBetweenRefreshes,45);

    //turnMagnetOnIn(0,2,0,timeBetweenRefreshes,95);
    //turnMagnetOnIn(1,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(4,2,0,timeBetweenRefreshes);

    //turnMagnetOnIn(0,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,3,0,timeBetweenRefreshes,60);
    //turnMagnetOnIn(4,3,0,timeBetweenRefreshes);

    //turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,timeBetweenRefreshes,90);
    //turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    turnMagnetOnIn(1, 5, 0, timeBetweenRefreshes);
    turnMagnetOnIn(2, 5, 0, timeBetweenRefreshes);
    turnMagnetOnIn(3, 5, 0, timeBetweenRefreshes);

    frame++;
  }else if(frame == 3){
    //Drag everything down
    //Row 0
    turnMagnetOnIn(1, 0, 0, shortDelay * 6);
    turnMagnetOnIn(2, 0, 0, shortDelay * 6);
    turnMagnetOnIn(3, 0, 0, shortDelay * 6);

    //Row 1
    turnMagnetOnIn(1, 1, 0, shortDelay * 5);
    turnMagnetOnIn(2, 1, 0, shortDelay * 5);
    turnMagnetOnIn(3, 1, 0, shortDelay * 5);
    //Row 2
    turnMagnetOnIn(1, 2, 0, shortDelay * 4);
    turnMagnetOnIn(2, 2, 0, shortDelay * 4);
    turnMagnetOnIn(3, 2, 0, shortDelay * 4);
    //Row 3
    turnMagnetOnIn(1, 3, 0, shortDelay * 3);
    turnMagnetOnIn(2, 3, 0, shortDelay * 3);
    turnMagnetOnIn(3, 3, 0, shortDelay * 3);
    //Row 4
    turnMagnetOnIn(1, 4, 0, shortDelay * 2);
    turnMagnetOnIn(2, 4, 0, shortDelay * 2);
    turnMagnetOnIn(3, 4, 0, shortDelay * 2);
    //Row 5
    turnMagnetOnIn(1, 5, 0, shortDelay);
    turnMagnetOnIn(2, 5, 0, shortDelay);
    turnMagnetOnIn(3, 5, 0, shortDelay);

    frame++;
  } else{



    frame = 1;
  }


}

void tMovementSimulation()
{
  if (frame == 1)
  {
    int t = 0;
    int distanceBetween = 2;
    turnMagnetOnIn(0, 0, t + 0, 500);
    turnMagnetOnIn(0, 1, t + shortDelay, longDelay);
    turnMagnetOnIn(0, 2, t + shortDelay * 2, longDelay);
    turnMagnetOnIn(0, 3, t + shortDelay * 3, longDelay);
    turnMagnetOnIn(0, 4, t + shortDelay * 4, longDelay);
    turnMagnetOnIn(0, 5, t + shortDelay * 5, timeBetweenRefreshes - (t + shortDelay * 5));
    t = shortDelay * distanceBetween;
    turnMagnetOnIn(0, 0, t + 0, 500);
    turnMagnetOnIn(0, 1, t + shortDelay, longDelay);
    turnMagnetOnIn(0, 2, t + shortDelay * 2, longDelay);
    turnMagnetOnIn(0, 3, t + shortDelay * 3, longDelay);
    turnMagnetOnIn(0, 4, t + shortDelay * 4, longDelay);
    turnMagnetOnIn(0, 5, t + shortDelay * 5, timeBetweenRefreshes - (t + shortDelay * 5));
    t += shortDelay * distanceBetween;
    turnMagnetOnIn(0, 0, t + 0, 500);
    turnMagnetOnIn(0, 1, t + shortDelay, longDelay);
    turnMagnetOnIn(0, 2, t + shortDelay * 2, longDelay);
    turnMagnetOnIn(0, 3, t + shortDelay * 3, longDelay);
    turnMagnetOnIn(0, 4, t + shortDelay * 4, longDelay);
    turnMagnetOnIn(0, 5, t + shortDelay * 5, timeBetweenRefreshes - (t + shortDelay * 5));
    //t += shortDelay*distanceBetween;
    //turnMagnetOnIn(1,0,t+0,500);
    //turnMagnetOnIn(1,1,t+shortDelay,longDelay);
    //turnMagnetOnIn(1,2,t+shortDelay*2,longDelay);
    //turnMagnetOnIn(1,3,t+shortDelay*3,longDelay);
    //turnMagnetOnIn(1,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));
    //t += shortDelay*distanceBetween;
    //turnMagnetOnIn(0,0,t+0,500);
    //turnMagnetOnIn(0,1,t+shortDelay,longDelay);
    //turnMagnetOnIn(0,2,t+shortDelay*2,longDelay);
    //turnMagnetOnIn(0,3,t+shortDelay*3,longDelay);
    //turnMagnetOnIn(0,4,t+shortDelay*4,timeBetweenRefreshes-(t+shortDelay*4));

    frame++;
  }
  else if (frame == 2)
  {
    // Plot T

    //turnMagnetOnIn(1,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(2,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(3,0,0,timeBetweenRefreshes,45);

    //turnMagnetOnIn(0,1,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(1,1,0,timeBetweenRefreshes,90);
    //turnMagnetOnIn(2,1,0,timeBetweenRefreshes,90);
    //turnMagnetOnIn(3,1,0,timeBetweenRefreshes,90);
    //turnMagnetOnIn(4,1,0,timeBetweenRefreshes,45);

    //turnMagnetOnIn(0,2,0,timeBetweenRefreshes,95);
    //turnMagnetOnIn(1,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(0, 2, 0, timeBetweenRefreshes, 90);
    //turnMagnetOnIn(4,2,0,timeBetweenRefreshes);

    //turnMagnetOnIn(0,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(0, 3, 0, timeBetweenRefreshes, 95);
    //turnMagnetOnIn(4,3,0,timeBetweenRefreshes);

    //turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(0, 4, 0, timeBetweenRefreshes, 95);
    //turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    turnMagnetOnIn(0, 5, 0, timeBetweenRefreshes);
    turnMagnetOnIn(0, 5, 0, timeBetweenRefreshes);
    turnMagnetOnIn(0, 5, 0, timeBetweenRefreshes);

    frame++;
  }
  else if (frame == 3)
  {
    //Drag everything down
    //Row 0
    turnMagnetOnIn(0, 0, 0, shortDelay * 6);
    turnMagnetOnIn(0, 0, 0, shortDelay * 6);
    turnMagnetOnIn(0, 0, 0, shortDelay * 6);

    //Row 1
    turnMagnetOnIn(0, 1, 0, shortDelay * 5);
    turnMagnetOnIn(0, 1, 0, shortDelay * 5);
    turnMagnetOnIn(0, 1, 0, shortDelay * 5);
    //Row 2
    turnMagnetOnIn(0, 2, 0, shortDelay * 4);
    turnMagnetOnIn(0, 2, 0, shortDelay * 4);
    turnMagnetOnIn(0, 2, 0, shortDelay * 4);
    //Row 3
    turnMagnetOnIn(0, 3, 0, shortDelay * 3);
    turnMagnetOnIn(0, 3, 0, shortDelay * 3);
    turnMagnetOnIn(0, 3, 0, shortDelay * 3);
    //Row 4
    turnMagnetOnIn(0, 4, 0, shortDelay * 2);
    turnMagnetOnIn(0, 4, 0, shortDelay * 2);
    turnMagnetOnIn(0, 4, 0, shortDelay * 2);
    //Row 5
    turnMagnetOnIn(0, 5, 0, shortDelay);
    turnMagnetOnIn(0, 5, 0, shortDelay);
    turnMagnetOnIn(0, 5, 0, shortDelay);

    frame++;
  }
  else
  {

    frame = 1;
  }
}

void fetchMovement()
{
  int row8[14] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16, 18};
  int row7[6] = {0, 4, 9, 12, 16, 18};
  int row6[6] = {0, 4, 9, 12, 16, 18};
  int row5[9] = {0, 1, 4, 5, 9, 12, 16, 17, 18};
  int row4[6] = {0, 4, 9, 12, 16, 18};
  int row3[6] = {0, 4, 9, 12, 16, 18};
  int row2[10] = {0, 4, 5, 6, 9, 12, 13, 14, 16, 18};
  if (frame == 1)
  {
    turnMagnetsOnIn(row8, 0, 14, 0, longDelay);
    turnMagnetsOnIn(row8, 1, 14, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 2)
  {
    turnMagnetsOnIn(row8, 1, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 2, 14, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 3)
  {
    turnMagnetsOnIn(row8, 2, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 3, 14, shortDelay, shortDelay);

    turnMagnetsOnIn(row7, 0, 6, 0, longDelay);
    turnMagnetsOnIn(row7, 1, 6, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 4)
  {
    turnMagnetsOnIn(row8, 3, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 4, 14, shortDelay, shortDelay);

    turnMagnetsOnIn(row7, 1, 6, 0, shortDelay);
    turnMagnetsOnIn(row7, 2, 6, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 5)
  {
    turnMagnetsOnIn(row8, 4, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 5, 14, shortDelay, shortDelay);

    turnMagnetsOnIn(row7, 2, 6, 0, shortDelay);
    turnMagnetsOnIn(row7, 3, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row6, 0, 6, 0, longDelay);
    turnMagnetsOnIn(row6, 1, 6, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 6)
  {
    turnMagnetsOnIn(row8, 5, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 6, 14, shortDelay, shortDelay);

    turnMagnetsOnIn(row7, 3, 6, 0, shortDelay);
    turnMagnetsOnIn(row7, 4, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row6, 1, 6, 0, shortDelay);
    turnMagnetsOnIn(row6, 2, 6, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 7)
  {
    turnMagnetsOnIn(row8, 6, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 7, 14, shortDelay, shortDelay);

    turnMagnetsOnIn(row7, 4, 6, 0, shortDelay);
    turnMagnetsOnIn(row7, 5, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row6, 2, 6, 0, shortDelay);
    turnMagnetsOnIn(row6, 3, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row5, 0, 9, 0, longDelay);
    turnMagnetsOnIn(row5, 1, 9, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 8)
  {
    turnMagnetsOnIn(row8, 7, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 8, 14, shortDelay, shortDelay);

    turnMagnetsOnIn(row7, 5, 6, 0, shortDelay);
    turnMagnetsOnIn(row7, 6, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row6, 3, 6, 0, shortDelay);
    turnMagnetsOnIn(row6, 4, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row5, 1, 9, 0, shortDelay);
    turnMagnetsOnIn(row5, 2, 9, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 9)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 6, 6, 0, shortDelay);
    turnMagnetsOnIn(row7, 7, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row6, 4, 6, 0, shortDelay);
    turnMagnetsOnIn(row6, 5, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row5, 2, 9, 0, shortDelay);
    turnMagnetsOnIn(row5, 3, 9, shortDelay, shortDelay);

    turnMagnetsOnIn(row4, 0, 6, 0, longDelay);
    turnMagnetsOnIn(row4, 1, 6, shortDelay, shortDelay);
    frame++;
  }
  else if (frame == 10)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 5, 6, 0, shortDelay);
    turnMagnetsOnIn(row6, 6, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row5, 3, 9, 0, shortDelay);
    turnMagnetsOnIn(row5, 4, 9, shortDelay, shortDelay);

    turnMagnetsOnIn(row4, 1, 6, 0, shortDelay);
    turnMagnetsOnIn(row4, 2, 6, shortDelay, shortDelay);
    frame++;
  }
  else if (frame == 11)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 4, 9, 0, shortDelay);
    turnMagnetsOnIn(row5, 5, 9, shortDelay, shortDelay);

    turnMagnetsOnIn(row4, 2, 6, 0, shortDelay);
    turnMagnetsOnIn(row4, 3, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row3, 0, 6, 0, longDelay);
    turnMagnetsOnIn(row3, 1, 6, shortDelay, shortDelay);
    frame++;
  }
  else if (frame == 12)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 5, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 3, 6, 0, shortDelay);
    turnMagnetsOnIn(row4, 4, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row3, 1, 6, 0, shortDelay);
    turnMagnetsOnIn(row3, 2, 6, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 13)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 5, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 2, 6, 0, shortDelay);
    turnMagnetsOnIn(row3, 3, 6, shortDelay, shortDelay);

    turnMagnetsOnIn(row2, 0, 10, 0, shortDelay);
    turnMagnetsOnIn(row2, 1, 10, shortDelay, shortDelay);
    frame++;
  }
  else if (frame == 14)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 5, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row2, 1, 10, 0, shortDelay);
    turnMagnetsOnIn(row2, 2, 10, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 15)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 5, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row2, 2, 10, 0, longDelay);

    frame++;
  }
  else if (frame == 16)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 5, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row2, 2, 10, 0, longDelay);
    frame++;
  }
  else if (frame == 17)
  {
    turnMagnetsOnIn(row8, 8, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 7, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 5, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row2, 2, 10, 0, longDelay);
    frame++;
  }
  else if (frame == 18)
  {
    turnMagnetsOnIn(row8, 7, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 6, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 5, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 4, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 2, 6, 0, longDelay);

    turnMagnetsOnIn(row2, 1, 10, 0, longDelay);

    frame++;
  }
  else if (frame == 19)
  {
    turnMagnetsOnIn(row8, 6, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 5, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 3, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 2, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 1, 6, 0, longDelay);

    turnMagnetsOnIn(row2, 0, 10, 0, longDelay);

    frame++;
  }
  else if (frame == 20)
  {
    turnMagnetsOnIn(row8, 5, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 4, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 2, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 1, 6, 0, longDelay);

    turnMagnetsOnIn(row3, 0, 6, 0, longDelay);
    frame++;
  }
  else if (frame == 21)
  {

    turnMagnetsOnIn(row8, 4, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 3, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 2, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 1, 9, 0, longDelay);

    turnMagnetsOnIn(row4, 0, 6, 0, longDelay);
    frame++;
  }
  else if (frame == 22)
  {
    turnMagnetsOnIn(row8, 3, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 2, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 1, 6, 0, longDelay);

    turnMagnetsOnIn(row5, 0, 9, 0, longDelay);
    frame++;
  }
  else if (frame == 23)
  {
    turnMagnetsOnIn(row8, 2, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 1, 6, 0, longDelay);

    turnMagnetsOnIn(row6, 0, 6, 0, longDelay);
    frame++;
  }
  else if (frame == 24)
  {
    turnMagnetsOnIn(row8, 1, 14, 0, longDelay);

    turnMagnetsOnIn(row7, 0, 6, 0, longDelay);
    frame++;
  }
  else if (frame == 25)
  {
    turnMagnetsOnIn(row8, 0, 14, 0, longDelay);
    frame++;
  }
  else if (frame == 26)
  {

    frame++;
  }
  else if (frame == 27)
  {

    frame++;
  }
  else
  {

    frame = 1;
  }
}

void simplefetchMovement()
{
  int row8[14] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16, 18};
  int row7[6] = {0, 4, 9, 12, 16, 18};
  int row6[6] = {0, 4, 9, 12, 16, 18};
  int row5[9] = {0, 1, 4, 5, 9, 12, 16, 17, 18};
  int row4[6] = {0, 4, 9, 12, 16, 18};
  int row3[6] = {0, 4, 9, 12, 16, 18};
  int row2[10] = {0, 4, 5, 6, 9, 12, 13, 14, 16, 18};
  if (frame == 1)
  {
    turnMagnetsOnIn(row8, 0, 14, 0, longDelay);
    turnMagnetsOnIn(row8, 1, 14, shortDelay, shortDelay);

    frame++;
  }
  else if (frame == 2)
  {
    turnMagnetsOnIn(row8, 1, 14, 0, shortDelay);
    turnMagnetsOnIn(row8, 2, 14, shortDelay, shortDelay);

    frame++;
  }
  else
  {

    frame = 1;
  }
}

void turnMagnetsOnIn(int* xArr, int y, int xLength, int inMillis, int forMillis, uint8_t uptime){
  //TODO: Make a FIFO buffer that can hold future (inMillis,forMillis) tuples. (Maybe: https://github.com/rlogiacco/CircularBuffer)
  //The future inMillis must then be modified by subtracting the current inMillis, so that it can be placed in the timeTilStart array when the current inMillis has passed and the magnet is turned on.
  //There also needs to be a check that the future inMillis will happen AFTER the current inMillis+current forMillis, so that the magnet has a pause of at least shortDelay (this minimum delay is a guess).
  int x;
  for (size_t i = 0; i < xLength; i++) {
    x = xArr[i];
    timeTilStart[x][y] = timeThisRefresh + inMillis;
    timeAtEnd[x][y] = timeThisRefresh + inMillis + forMillis; //Equal to: timeTilStart+forMillis
    if(uptime > 100) uptime = 100;
      dutyCycle[x][y] = (uint8_t)((uptime / 100.0) * (dutyCycleResolution-1));
    if(DEBUG && false){ //Using &&false when I don't want this output
      Serial.print("Magnet (");
      Serial.print(x);
      Serial.print(",");
      Serial.print(y);
      Serial.print(") will turn ON at time: ");
      Serial.print(timeTilStart[x][y]);
      Serial.print("ms, and OFF at time:");
      Serial.print(timeAtEnd[x][y]);
      Serial.print("ms. Current time (ms): ");
      Serial.println(timeThisRefresh);
    }
  }
}

void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime){
  //TODO: Make a FIFO buffer that can hold future (inMillis,forMillis) tuples. (Maybe: https://github.com/rlogiacco/CircularBuffer)
  //The future inMillis must then be modified by subtracting the current inMillis, so that it can be placed in the timeTilStart array when the current inMillis has passed and the magnet is turned on.
  //There also needs to be a check that the future inMillis will happen AFTER the current inMillis+current forMillis, so that the magnet has a pause of at least shortDelay (this minimum delay is a guess).

  timeTilStart[x][y] = timeThisRefresh + inMillis;
  timeAtEnd[x][y] = timeThisRefresh + inMillis + forMillis; //Equal to: timeTilStart+forMillis
  if(uptime > 100) uptime = 100;
  dutyCycle[x][y] = (uint8_t)((uptime / 100.0) * (dutyCycleResolution-1));
  if(DEBUG && false){ //Using &&false when I don't want this output
    Serial.print("Magnet (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.print(") will turn ON at time: ");
    Serial.print(timeTilStart[x][y]);
    Serial.print("ms, and OFF at time:");
    Serial.print(timeAtEnd[x][y]);
    Serial.print("ms. Current time (ms): ");
    Serial.println(timeThisRefresh);

  }
}

void movementAlgorithm(){
  if (DEBUG && false){
    Serial.print("timeThisRefresh: ");
    Serial.println(timeThisRefresh);
    Serial.print("timeForNewRefresh: ");
    Serial.println(timeForNewRefresh);
  }
  if(timeThisRefresh >= timeForNewRefresh){

  /*  for(int x = 0; x < COLS; x++){
      for(int y = 0; y < ROWS; y++){
        currMagnetState[x][y] = OFF;
        //turnMagnetOnIn(x,y,shortDelay*counter+longDelay*counter, longDelay)
        //counter++;
      }
    }*/

    Serial.print("Preparing Frame: ");
    Serial.print(frame);
    Serial.println();

    tMovement();


    timeForNewRefresh += timeBetweenRefreshes; //Theoretically the same as timeThisRefresh+timeBetweenRefreshes, but in case a turn(ms) is skipped for some reason this will be more accurate.
  }
}

void setup() {
  Serial.begin(9600);

  Serial.println("Serial initiated");

  //MagnetMatrix mmx = MagnetMatrix(shiftEnablePins,shiftClkPins,shiftDataPins,REGISTERS);
  //mmx.printPinValues();

  //set pins to output because they are addressed in the main loop
  pinMode(SHIFT_INDICATOR_PIN,OUTPUT);
  digitalWrite(SHIFT_INDICATOR_PIN,LOW);
  for (int i = 0; i < ALL_ROWS; i++)
  { //(sizeof(shiftEnablePins) / sizeof(shiftEnablePins[0]))
    pinMode(shiftEnablePins[i],OUTPUT);
  }
  for (int i = 0; i < ALL_ROWS; i++)
  {
    pinMode(shiftClkPins[i],OUTPUT);
  }
  for (int i = 0; i < ALL_ROWS; i++)
  {
    pinMode(shiftDataPins[i],OUTPUT);
  }
  for(int x=0; x<COLS; x++){
    for(int y=0;y<ROWS;y++){
      timeAtEnd[x][y] = 0;
      timeAtStart[x][y] = 0;
      dutyCycle[x][y] = dutyCycleResolution;
    }
  }
  shiftOut_zeros_to_full_screen();
  delay(1000);
  startTime = micros();
  timeThisRefresh = millis();
  timeForNewRefresh = timeThisRefresh+timeBetweenRefreshes;
  Serial.print("Setup was executed in: ");
  Serial.print(startTime);
  Serial.println("us");
}

void loop(){
  unsigned long ms = millis();
  timeThisRefresh = ms;
  movementAlgorithm();
  refreshScreen();
  for(int x=0;x<COLS;x++){
    for(int y=0;y<ROWS;y++){
      prevMagnetState[x][y] = currMagnetState[x][y];
    }
  }
  if(DEBUG){
    if(counter >= 1000){

      //Test1:
      //Conditions: No proper movementAlgorithm, ROWS=12, COLS=23, REGISTERS = 10, BYTES_PER_REGISTER=4 (NOTE: this test used a shiftOut_one_PCB_per_row routine that only shifted out COLS=23 bits to each register(230 total), not 32)
      //Result:     Avg execution time: 10732us(10.73ms), based on : 1000 executions, current time(ms): 21741
      //Meaning:    The absolute lowest "low time" in a PWM configuration will be around 11ms
      //Test2:
      //Conditions: No proper movementAlgorithm, ROWS=12, COLS=21, REGISTERS = 1, BYTES_PER_REGISTER=4 (NOTE: this test used a shiftOut_one_PCB_per_row routine that shifted out COLS*ROWS=252 bits to each register, not 32)
      //Result:     Avg execution time: 11569us(11.57ms), based on : 1000 executions, current time(ms): 23356
      //Meaning:    The absolute lowest "low time" in a PWM configuration will be around 12ms
      Serial.print("Avg execution time: ");
      Serial.print((micros()-startTime)/counter);
      Serial.print("us(");
      Serial.print((micros()-startTime)/(counter*1000.0));
      Serial.print("ms), based on : ");
      Serial.print(counter);
      Serial.print(" executions, current time(ms): ");
      Serial.println(millis());
      counter = 0;
      startTime = micros();
    }else{
      counter++;
    }
  }

}
