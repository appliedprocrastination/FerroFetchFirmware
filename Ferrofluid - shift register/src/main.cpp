#include <Arduino.h>
#include <ArduinoUnit.h>
#include <MagnetMatrix.h>

const bool DEBUG = false;
int counter = 0;                //debugvariabler
unsigned long startTime = 0;    //debugvariabler
//Pin connected to RCLK of SN74HC595
int shiftEnablePins[] = {2,5,8,11,14,17,20,23,26,29};   //RCLK, NOTE: the variable "registers" will change how many of these pinsare initiated
//Pin connected to SRCLK of SN74HC595
int shiftClkPins[] = {3,6,9,12,15,18,21,24,27,30};      //SRCLK, NOTE: the variable "registers" will change how many of these pinsare initiated
////Pin connected to SER of SN74HC595
int shiftDataPins[] = {4,7,10,13,16,19,22,25,28,31};    //SER, NOTE: the variable "registers" will change how many of these pinsare initiated

int registers = 1; // no of register series (indicating no of magnet-driver-PCBs connected to the Arduino)
int bytesPerRegister = 4; // no of 8-bit shift registers in series per line (4 = 32 bits(/magnets))

//holders for infromation you're going to pass to shifting function
const int ROWS = 5;//12
const int COLS = 5;//21

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
int shortDelay = 400; //ms
int longDelay = 700; //ms
unsigned long timeBetweenRefreshes = 4*shortDelay*2+4*shortDelay;//12*shortDelay; //ms, meaning 60 seconds

int frame = 1;

//States
enum MagState{OFF, ON};
MagState prevMagnetState[COLS][ROWS];
MagState currMagnetState[COLS][ROWS];

//Function headers:
void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent

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
  for(x = COLS; x<(8*bytesPerRegister); x++){
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

void shiftOut_TinyMatrix(int registerIndex){
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
  int y;

  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(shiftDataPins[registerIndex], LOW);
  digitalWrite(shiftClkPins[registerIndex], LOW);

  //Write out the ROWS*COLS first values:
  for(y=ROWS-1;y>=0;y--){
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
  }

  //stop shifting
  digitalWrite(shiftClkPins[registerIndex], LOW);
}

void refreshScreen(){
  updateAllStates();
  for(int reg = 0; reg < registers; reg++){
    //ground latchPin and hold low for as long as you are transmitting
    digitalWrite(shiftEnablePins[reg], LOW);
    //move 'em out
    //shiftOut()
    shiftOut_TinyMatrix(reg);
    //return the latch pin high to signal chip that it
    //no longer needs to listen for information
    digitalWrite(shiftEnablePins[reg], HIGH);
  }
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

void hiMovement(){
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
    // Plot HI


    //turnMagnetOnIn(1,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(2,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(3,0,0,timeBetweenRefreshes,45);

    turnMagnetOnIn(0,1,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(1,1,0,timeBetweenRefreshes,90);
    turnMagnetOnIn(2,1,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(3,1,0,timeBetweenRefreshes,90);
    turnMagnetOnIn(4,1,0,timeBetweenRefreshes,45);

    turnMagnetOnIn(0,2,0,timeBetweenRefreshes,95);
    turnMagnetOnIn(1,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes,95);
    turnMagnetOnIn(4,2,0,timeBetweenRefreshes);

    turnMagnetOnIn(0,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(4,3,0,timeBetweenRefreshes);

    //turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    //turnMagnetOnIn(2,4,0,timeBetweenRefreshes);
    //turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  }else if(frame == 3){
    //plot :)

    //turnMagnetOnIn(1,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(2,0,0,timeBetweenRefreshes,45);
    //turnMagnetOnIn(3,0,0,timeBetweenRefreshes,45);

    turnMagnetOnIn(0,1,0,timeBetweenRefreshes,45);
    turnMagnetOnIn(1,1,shortDelay,timeBetweenRefreshes);
    turnMagnetOnIn(2,1,shortDelay,timeBetweenRefreshes);
    turnMagnetOnIn(3,1,shortDelay,timeBetweenRefreshes);
    turnMagnetOnIn(4,1,0,timeBetweenRefreshes,45);

    turnMagnetOnIn(0,2,0,timeBetweenRefreshes);
    //turnMagnetOnIn(1,2,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,2,0,shortDelay);
    turnMagnetOnIn(4,2,0,timeBetweenRefreshes);

    //turnMagnetOnIn(0,3,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,3,0,shortDelay*2);
    //turnMagnetOnIn(4,3,0,timeBetweenRefreshes);

    turnMagnetOnIn(1,4,0,timeBetweenRefreshes);
    turnMagnetOnIn(2,4,0,shortDelay*2);
    turnMagnetOnIn(3,4,0,timeBetweenRefreshes);

    frame++;
  } else if(frame == 4){
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
  } else{



    frame = 1;
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
    //sefMovement()
    heiMovement();
    //hiMovement();



    timeForNewRefresh += timeBetweenRefreshes; //Theoretically the same as timeThisRefresh+timeBetweenRefreshes, but in case a turn(ms) is skipped for some reason this will be more accurate.
  }
}

void setup() {
  Serial.begin(9600);

  Serial.println("Serial initiated");

  //MagnetMatrix mmx = MagnetMatrix(shiftEnablePins,shiftClkPins,shiftDataPins,registers);
  //mmx.printPinValues();

  //set pins to output because they are addressed in the main loop
  for(int i = 0; i<registers; i++){ //(sizeof(shiftEnablePins) / sizeof(shiftEnablePins[0]))
    pinMode(shiftEnablePins[i],OUTPUT);
  }
  for(int i = 0; i<registers; i++){
    pinMode(shiftClkPins[i],OUTPUT);
  }
  for(int i = 0; i<registers; i++){
    pinMode(shiftDataPins[i],OUTPUT);
  }
  for(int x=0; x<COLS; x++){
    for(int y=0;y<ROWS;y++){
      timeAtEnd[x][y] = 0;
      timeAtStart[x][y] = 0;
      dutyCycle[x][y] = dutyCycleResolution;
    }
  }
  startTime = micros();
  timeThisRefresh = millis();
  timeForNewRefresh = timeThisRefresh+timeBetweenRefreshes;
  Serial.print("Setup was executed in: ");
  Serial.print(startTime);
  Serial.println("us");
}

void loop(){
  //Serial.println("Loop begin.");
  unsigned long ms = millis();
  /*
  if (timeThisRefresh == ms)
    return;
  else
    timeThisRefresh = ms;
  */
  timeThisRefresh = ms;
  // Below this point we know that 1ms has passed and we don't have to worry about millis() overflow if we use a 1ms increase.. At least thats the intention
  movementAlgorithm();
  //Serial.println("Movement Algorithm done...");
  refreshScreen();
  //Serial.println("Refresh Screen done...");
  //memcpy(prevMagnetState,currMagnetState,sizeof(currMagnetState[0])*COLS*ROWS); //prevMagnetState = currMagnetState;

  for(int x=0;x<COLS;x++){
    for(int y=0;y<ROWS;y++){
      prevMagnetState[x][y] = currMagnetState[x][y];
    }
  }
  if(DEBUG){
    if(counter >= 1000){
      //Test1:
      //Conditions: No proper movementAlgorithm, ROWS=12, COLS=23, registers = 10, bytesPerRegister=4 (NOTE: this test used a shiftOut_TinyMatrix routine that only shifted out COLS=23 bits to each register(230 total), not 32)
      //Result:     Avg execution time: 10732us(10.73ms), based on : 1000 executions, current time(ms): 21741
      //Meaning:    The absolute lowest "low time" in a PWM configuration will be around 11ms
      //Test2:
      //Conditions: No proper movementAlgorithm, ROWS=12, COLS=21, registers = 1, bytesPerRegister=4 (NOTE: this test used a shiftOut_TinyMatrix routine that shifted out COLS*ROWS=252 bits to each register, not 32)
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
