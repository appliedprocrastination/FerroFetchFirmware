#include <Arduino.h>

const bool DEBUG = true;
int counter = 0;     //debugvariabler 
int startTime = 0;   //debugvariabler
//Pin connected to RCLK of SN74HC595
int shiftEnablePins[] = {2,5,8,11,14,17,20,23,26,29};   //RCLK
//Pin connected to SRCLK of SN74HC595
int shiftClkPins[] = {3,6,9,12,15,18,21,24,27,30};      //SRCLK
////Pin connected to SER of SN74HC595
int shiftDataPins[] = {4,7,10,13,16,19,22,25,28,31};    //SER

int registers = 1; // no of register series (indicating no of PCBs)
int bytesPerRegister = 4; // no of shift registers in series per line (4 = 32 bit/magnets)

//holders for infromation you're going to pass to shifting function
const int ROWS = 6;
const int COLS = 5;

//Timekeeping variables:
float timeTilStart[COLS][ROWS]; //The time in ms until a magnet should start. Used to make movement patterns before the movement should happen
float timeAtStart[COLS][ROWS]; //The time in ms when a magnet was started. Used as a safety feature so that a magnet doesn't stay turned on too long.
float timeAtEnd[COLS][ROWS]; //The duration a magnet should stay turned on once it's been turned on.
unsigned long timeThisRefresh = 0;
unsigned long timeForNewRefresh = 0;
unsigned long timeBetweenRefreshes = 5000; //ms, meaning 60 seconds

//Other movement related variables
int dutyCycle[COLS][ROWS]; //Probably not needed, just creating it to remember thinking more about it later
int shortDelay = 200; //ms
int longDelay = 500; //ms

//States
enum MagState{OFF, ON};
MagState prevMagnetState[COLS][ROWS];
MagState currMagnetState[COLS][ROWS];


void updateStateAt(int x, int y){
  if(prevMagnetState[x][y] == OFF && currMagnetState[x][y] == ON){
    timeAtStart[x][y] = timeThisRefresh;
  }
}

void updateAllStates(){
  for (int x = 0; x < COLS; x++) {
    for (int y = 0; y < ROWS; y++) {
      if(timeAtStart[x][y] <= timeThisRefresh && timeAtEnd[x][y]>timeThisRefresh){
        currMagnetState[x][y] = ON;
      }else if (timeAtEnd[x][y] <= timeThisRefresh ) {
        currMagnetState[x][y] = OFF;

      } else {
        /* code */
      }
    }
  }
}

void shiftOut(int registerIndex){
  //This shifts (8*BitsPerRegister) bits out (TODO: was MSB first when the loops counted down from 7, how is this now?),
  //on the rising edge of the clock,
  //clock idles low

  //Register-index must be translated to display-coordinates
  //For y < COLS: y=registerIndex (The m=21 MSB represents the y'th row)
  //For y >=COLS: TODO: Yet to be decided (spread out on the ten PCBs somehow)
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
    updateStateAt(x,y);
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
      updateStateAt(x,y);
      digitalWrite(shiftClkPins[registerIndex], HIGH);
      digitalWrite(shiftDataPins[registerIndex], LOW);
  }
    //stop shifting
    digitalWrite(shiftClkPins[registerIndex], LOW);
}

void shiftOut_TinyMatrix(int registerIndex){
  //This shifts (8*BitsPerRegister) bits out (TODO: was MSB first when the loops counted down from 7, how is this now?),
  //on the rising edge of the clock,
  //clock idles low

  //Register-index must be translated to display-coordinates
  //For y < COLS: y=registerIndex (The m=21 MSB represents the y'th row)
  //For y >=COLS: TODO: Yet to be decided (spread out on the ten PCBs somehow)
  int x;
  int y = registerIndex;
  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(shiftDataPins[registerIndex], LOW);
  digitalWrite(shiftClkPins[registerIndex], LOW);
  //Write out the COLS = 21 first values:

  for(y=0; y<ROWS; y++){
    for(x=0; x<COLS; x++){
      //NOTE: The first thing written will be pushed to the very end of the register (meaning coord (0,0) will be on Q_h of the 4th 8-bit register in a 32-bit regiser series).
      digitalWrite(shiftClkPins[registerIndex], LOW);
      //Sets the pin to HIGH or LOW depending on pinState

      digitalWrite(shiftDataPins[registerIndex], currMagnetState[x][y]);
      /*
      Serial.print("(");
      Serial.print(x);
      Serial.print(",");
      Serial.print(y);
      Serial.print(") = ");
      Serial.print(currMagnetState[x][y]);
      */
      updateStateAt(x,y);

      //register shifts bits on upstroke of clock pin
      digitalWrite(shiftClkPins[registerIndex], HIGH);
      //zero the data pin after shift to prevent bleed through
      digitalWrite(shiftDataPins[registerIndex], LOW);
    }

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

void turnMagnetOnIn(int x, int y, int inMillis, int forMillis){
  timeTilStart[x][y] = timeThisRefresh + inMillis;
  timeAtEnd[x][y] = timeThisRefresh + inMillis + forMillis; //Equal to: timeTilStart+forMillis
}

void movementAlgorithm(){
  if (!DEBUG){
    Serial.print("timeThisRefresh: ");
    Serial.println(timeThisRefresh);
    Serial.print("timeForNewRefresh: ");
    Serial.println(timeForNewRefresh);
  }
  if(timeThisRefresh >= timeForNewRefresh){

    for(int x = 0; x < COLS; x++){
      for(int y = 0; y < ROWS; y++){
        currMagnetState[x][y] = OFF;
        //turnMagnetOnIn(x,y,shortDelay*counter+longDelay*counter, longDelay)
        //counter++;
      }
    }

    turnMagnetOnIn(2,0,0,longDelay);
    turnMagnetOnIn(2,1,0,longDelay);
    turnMagnetOnIn(2,2,0,longDelay);
    turnMagnetOnIn(2,3,0,longDelay);
    turnMagnetOnIn(2,4,0,longDelay);
    turnMagnetOnIn(2,5,0,longDelay);

    timeForNewRefresh += timeBetweenRefreshes;
  }
}


void setup() {
  Serial.begin(9600);

  Serial.println("Serial initiated");
  //set pins to output because they are addressed in the main loop
  for(unsigned int i = 0; i<sizeof(shiftEnablePins);i++){
    pinMode(shiftEnablePins[i],HIGH);
  }
  for(unsigned int i = 0; i<sizeof(shiftClkPins);i++){
    pinMode(shiftClkPins[i],HIGH);
  }
  for(unsigned int i = 0; i<sizeof(shiftDataPins);i++){
    pinMode(shiftDataPins[i],HIGH);
  }
  for(int x=0; x<COLS; x++){
    for(int y=0;y<ROWS;y++){
      timeAtEnd[x][y] = 0;
      timeAtStart[x][y] = 0;
    }
  }
  Serial.println("Setup done...");

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
  timeThisRefresh =ms;
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
      Serial.print("Avg execution time (without 1ms delay): ");
      Serial.print((millis()-startTime)/counter);
      Serial.print(", based on : ");
      Serial.print(counter);
      Serial.println(" executions");
      counter = 0;
      startTime = millis();
    }else{
      counter++;
    }
  }

}
