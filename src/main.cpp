#include <Arduino.h>
#include <arm_math.h>

#define SHIFT_INDICATOR_PIN 32

const bool DEBUG = false;
int counter = 0;                //debugvariables
unsigned long startTime = 0;    //debugvariables

//Pin connected to SRCLK of SN74HC595
const int SHIFT_CLK_PIN = 2; //SRCLK
//Pin connected to RCLK of SN74HC595
const int SHIFT_ENABLE_PIN = 3; //RCLK
//Pin connected to SER of SN74HC595
const int SHIFT_DATA_PINS[12] = {15, 22, 23, 9, 10, 13, 11, 12, 35, 36, 37, 38}; //SER, NOTE: the variable "ALL_ROWS" will change how many of these pinsare initiated

//holders for infromation you're going to pass to shifting function
const int ALL_ROWS = 10; //The total number of rows in the actual hardware
const int ALL_COLS = 19; //The total number of columns in the actual hardware
const int ROWS = ALL_ROWS;//12; //The number of rows that are in use in the current program (different from ALL_ROWS in order to scale down the number of bits shifted out)
const int COLS = ALL_COLS;//21; //The number of cols that are in use in the current program (different from ALL_COLS in order to scale down the number of bits shifted out)

const int REGISTERS = ROWS; // no of register series (indicating no of magnet-driver-PCBs connected to the Arduino)
const int BYTES_PER_REGISTER = 4; // no of 8-bit shift registers in series per line (4 = 32 bits(/magnets))

//Timekeeping variables:
float timeTilStart[COLS][ROWS]; //The time in ms until a magnet should start. Used to make movement patterns before the movement should happen
float timeAtStart[COLS][ROWS];  //The time in ms when a magnet was started. Used as a safety feature so that a magnet doesn't stay turned on too long.
float timeAtEnd[COLS][ROWS];    //The duration a magnet should stay turned on once it's been turned on.
unsigned long timeThisRefresh = 0;
unsigned long timeForNewRefresh = 0;

//Other movement related variables
uint8_t dutyCycle[COLS][ROWS]; 
uint8_t dutyCycleCounter = 0;
uint8_t dutyCycleResolution = 20; //resolution 20 yields 5% increments, resolution 10 yields 10% increments
int shortDelay = 500; //ms
int longDelay = 1000; //ms
unsigned long timeBetweenRefreshes = 6000;//12*shortDelay; //ms

int frame = 1;

//States
uint32_t prevMagnetState_reg_based[COLS];
uint32_t currMagnetState_reg_based[COLS];

//Function headers:
void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent
void turnMagnetsOnIn(int* xArr, int y,int xLength, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent

//Functions:
boolean magnetIsToggledThisFrame_reg_based(int x, int y)
{
  if (prevMagnetState_reg_based[x] & (1 << y) == 0 && currMagnetState_reg_based[x] & (1 << y) == 1)
  {
    return true;
  }
  return false;
}

void updateTimeAtStart_reg_based(int x)
{
  for (size_t y = 0; y < COLS; y++)
  {
    if (magnetIsToggledThisFrame_reg_based(x,y))
    {
      timeAtStart[x][y] = timeThisRefresh;
    }
  }
  
}

void updateAllStates_reg_based()
{
  for (int x = 0; x < COLS; x++)
  {
    currMagnetState_reg_based[x] = 0;
    for (int y = 0; y < ROWS; y++)
    {
      if (timeTilStart[x][y] <= timeThisRefresh && timeAtEnd[x][y] > timeThisRefresh)
      {
        if(dutyCycle[x][y] > dutyCycleCounter){
          currMagnetState_reg_based[x] |= 1 << y;
        }

        #if (DEBUG && false)
         //Using &&false when I don't want this output
          Serial.print("Magnet (");
          Serial.print(x);
          Serial.print(",");
          Serial.print(y);
          Serial.println(") will turn ON this iteration: ");
        #endif
      }

    }
  }
  dutyCycleCounter++;
  if (dutyCycleCounter >= dutyCycleResolution)
    dutyCycleCounter = 0;
}

void shiftOut_one_PCB_per_PORT(void)
{
  //This method is based on the assumption that the SER pin of SN74HC595 is connected to
  //pins controlled in the PORTC register, and that these pins are in ascending order.
  //For Teensy3.6 this will, for a maximum of 12 pins, be:
  //PCB/SN74HC595/Row number: 0 ,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11
  //Teensy3.6, digital pin  : 15, 22, 23,  9, 10, 13, 11, 12, 35, 36, 37, 38
  //Further, the code assumes that SHIFT_CLK_PIN is digital 2 on Teensy3.6

  //indicate to logic analyzer that the shift is starting
  //(The latch pin could be used to achieve this functionality in current software,
  //but in order to test different shifting procedures a separate pin is used)
  digitalWrite(SHIFT_INDICATOR_PIN, HIGH);
  //ground latchPin and hold low for as long as you are transmitting
  digitalWrite(SHIFT_ENABLE_PIN, LOW);

  //clear everything out just in case to prepare shift register for bit shifting
  GPIOC_PDOR = 0;//equivalent to using digitalWrite(pin,LOW) on all data-pins individually  
  for (int x = COLS - 1; x >= 0; x--)
  {
    //NOTE: The first thing written will be pushed to the very end of the register.
    //Therefore the loop counts downwards (so we get the origin in the bottom left corner of the display)
    digitalWrite(SHIFT_CLK_PIN, LOW);
    
    //Sets all data-pins to HIGH or LOW depending on the corresponding value in the currMagnetState
    //(because we've checked that they are on the same port): digitalWrite(SHIFT_DATA_PINS[y], LOW);
    GPIOC_PDOR = currMagnetState_reg_based[x];

    updateTimeAtStart_reg_based(x);

    //register shifts bits on rising edge of clock pin
    digitalWrite(SHIFT_CLK_PIN, HIGH);
    
    //zero the data pin after shift to prevent bleed through
    GPIOC_PDOR = 0; //digitalWrite(SHIFT_DATA_PIN, LOW);
  }
  # if (DEBUG && timeThisRefresh >= timeForNewRefresh)
    Serial.println();
  #endif
  //stop shifting
  digitalWrite(SHIFT_CLK_PIN, LOW);
  
  //return the latch pin high to signal chip that it
  //no longer needs to listen for information
  digitalWrite(SHIFT_ENABLE_PIN, HIGH);

  //Indicate to logic analyzer that the shift is done
  digitalWrite(SHIFT_INDICATOR_PIN, LOW);
}

void refreshScreen_reg_based()
{
  updateAllStates_reg_based();


  shiftOut_one_PCB_per_PORT();
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
    turnMagnetOnIn(2,2,0,timeBetweenRefreshes,60);
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
    # if(DEBUG && false) //Using &&false when I don't want this output
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
    #endif
  }
}

void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime){
  //TODO: Make a FIFO buffer that can hold future (inMillis,forMillis) tuples. (Maybe: https://github.com/rlogiacco/CircularBuffer)
  //The future inMillis must then be modified by subtracting the current inMillis, so that it can be placed in the timeTilStart array when the current inMillis has passed and the magnet is turned on.
  //There also needs to be a check that the future inMillis will happen AFTER the current inMillis+current forMillis, so that the magnet has a pause of at least shortDelay (this minimum delay is a guess).

  timeTilStart[x][y] = timeThisRefresh + inMillis;
  timeAtEnd[x][y] = timeThisRefresh + inMillis + forMillis; //Equal to: timeTilStart+forMillis
  if(uptime > 100) uptime = 100;
  dutyCycle[x][y] = (uint8_t)((uptime / 100.0f) * (dutyCycleResolution));
  # if(DEBUG && false) //Using &&false when I don't want this output
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
  #endif
}

void movementAlgorithm(){
  #if (DEBUG && false)
    Serial.print("timeThisRefresh: ");
    Serial.println(timeThisRefresh);
    Serial.print("timeForNewRefresh: ");
    Serial.println(timeForNewRefresh);
  #endif
  if(timeThisRefresh >= timeForNewRefresh){

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

  //set pins to output because they are addressed in the main loop
  pinMode(SHIFT_INDICATOR_PIN,OUTPUT);
  digitalWrite(SHIFT_INDICATOR_PIN,LOW);

  pinMode(SHIFT_CLK_PIN,OUTPUT);
  pinMode(SHIFT_ENABLE_PIN,OUTPUT);
  for (int i = 0; i < ALL_ROWS; i++)
  {
    pinMode(SHIFT_DATA_PINS[i],OUTPUT);
  }
  for(int x=0; x<COLS; x++){
    for(int y=0;y<ROWS;y++){
      timeAtEnd[x][y] = 0;
      timeAtStart[x][y] = 0;
      dutyCycle[x][y] = dutyCycleResolution;
    }
  }

  //Clear all shift registers.
  //Especially important if ALL_ROWS != ROWS
  for (int x = ALL_COLS - 1; x >= 0; x--)
  {
    currMagnetState_reg_based[x] = 0;
  }
  shiftOut_one_PCB_per_PORT();
  for (int i = ALL_ROWS - 1; i > ROWS-1 ; i--)
  {
    //"Deinitialize" pins that are connected but not supposed to be used.
    pinMode(SHIFT_DATA_PINS[i], INPUT);
  }

  startTime = micros();
  timeThisRefresh = millis();
  timeForNewRefresh = timeThisRefresh+timeBetweenRefreshes;
  Serial.print("Setup was executed in: ");
  Serial.print(startTime);
  Serial.println("us");
}

void loop(){
  timeThisRefresh = millis();
  
  movementAlgorithm();
  refreshScreen_reg_based();
  for(int x=0;x<COLS;x++){
    prevMagnetState_reg_based[x] = currMagnetState_reg_based[x];
  }

  #if(DEBUG)
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
  #endif

}
