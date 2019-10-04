#include <Arduino.h>
#include <arm_math.h>

#include "Animation.h"
//#include "SdFat.h"
#include "debug.h"
#include "RamMonitor.h"
//#include "FreeStack.h"

const bool DEBUG = true;
int counter = 0;                //debugvariables
unsigned long startTime = 0;    //debugvariables

SdFatSdioEX sd;
bool sd_present = true;

//PRELOADED_ANIMATION:  The entire animation is written into memory before the program is compiled
//DYNAMIC_ANIMATION:    The animation can be changed/updated dynamically, but is based on a fixed framerate
//ASYNC_ANIMATION:      The animation is independent of the framerate and a pixel can be set at any given time.
enum AnimationMode{PRELOADED_ANIMATION, DYNAMIC_ANIMATION, ASYNC_ANIMATION};
AnimationMode animation_mode = PRELOADED_ANIMATION;

int STATE_SWITCH_PINS[4] = {27,25,26,4};
enum StateSwitchMode{ LEFT = 27,
                      MIDRIGHT = 25,
                      MIDLEFT = 26,
                      RIGHT = 4};
StateSwitchMode state_switch_mode;

int BUTTON_LEFT = 5;
int BUTTON_UP   = 6;
int BUTTON_RIGHT= 7;
int BUTTON_DOWN = 8;
int BUTTON_PINS[4] = {BUTTON_LEFT, BUTTON_UP, BUTTON_RIGHT, BUTTON_DOWN};

//Pin connected to SRCLK of SN74HC595
const int SHIFT_CLK_PIN = 2; //SRCLK
//Pin connected to RCLK of SN74HC595
const int SHIFT_ENABLE_PIN = 3; //RCLK
//Pin connected to SER of SN74HC595
const int SHIFT_DATA_PINS[12] = {15, 22, 23, 9, 10, 13, 11, 12, 35, 36, 37, 38}; //SER, NOTE: the variable "ALL_ROWS" will change how many of these pinsare initiated

//Interrupt Flags:
volatile uint8_t RESTART_ANIM_FLAG = 0;

//Timekeeping variables:
float timeTilStart[COLS][ROWS]; //The time in ms until a magnet should start. Used to make movement patterns before the movement should happen
float timeAtStart[COLS][ROWS];  //The time in ms when a magnet was started. Used as a safety feature so that a magnet doesn't stay turned on too long.
float timeAtEnd[COLS][ROWS];    //The duration a magnet should stay turned on once it's been turned on.
unsigned long timeThisRefresh = 0;
unsigned long time_for_next_frame = 0;

//Other movement related variables
uint8_t async_dutyCycle[COLS][ROWS];
uint8_t dutyCycleCounter = 0;
//DUTY_CYCLE_RESOLUTION = 20 is defined in Animation.h. Resolution 20 yields 5% increments, resolution 10 yields 10% increments
//int shortDelay = 500; //ms
//int longDelay = 1000; //ms
//unsigned long timeBetweenRefreshes = 6000;//12*shortDelay; //ms
unsigned long frame_rate    = 4; //fps. 8 FPS seems to be the maximum our ferrofluid can handle 02.Sept.2019
unsigned long frame_period   = 1000/frame_rate; //ms
int animation_num = 0;

//int frame = 1;


//States
//uint32_t prevMagnetState[COLS];
//uint32_t currMagnetState[COLS];
const int transport_anim_count = 20;
//const int transport_anim_count = 2;

Frame *  transport_anim_frames[transport_anim_count];
const int loop_frames_count = 14;
Frame *loop_frames[loop_frames_count];
/*
uint32_t transport_animation[transport_anim_count][COLS] = {
    {0xa1b2c3d4, 1, 1, 1, 1, 1, 1,     1,        1,        1,         1,       1, 1, 1, 1, 1, 1, 1, 0xf1e2d3c4},
    {0xa2b3c4d5, 1, 1, 1, 1, 1, 1,     1,        1,        1,         1,       1, 1, 1, 1, 1, 1, 1, 0xf2e3d4c5},
};
*/
uint32_t transport_animation[transport_anim_count][COLS] = {
    {0, 0, 0, 0, 0, 0, 0,      0b1,        0,        0b1,         0,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,     0b11,        0,       0b11,         0,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,     0b10,        0,       0b10,         0,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,    0b110,      0b1,      0b110,       0b1,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,    0b100,     0b11,      0b100,      0b11,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,   0b1100,     0b10,     0b1100,      0b10,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,   0b1001,    0b110,     0b1001,     0b110,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,  0b11011,    0b100,    0b11011,     0b100,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,  0b10010,   0b1100,    0b10010,    0b1100,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b110110,   0b1001,   0b110110,    0b1001,       0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0b100100,  0b11011,   0b100100,   0b11011,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,0b1101100,  0b10010,  0b1101100,   0b10010,       0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0,0b1001000, 0b110110,  0b1001000,  0b110110,       0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0,0b1101100, 0b100100,  0b1001000,  0b100100,       0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0,0b1101100, 0b100100,  0b1001000, 0b1101100,       0, 0, 0, 0, 0, 0, 0, 0}, 
    
    {0, 0, 0, 0, 0, 0, 0,0b0100100, 0b000000,  0b0000000, 0b1101100,       0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0,0b0100100, 0b000000,  0b0000000, 0b1001000,       0, 0, 0, 0, 0, 0, 0, 0},  
 
    {0, 0, 0, 0, 0, 0, 0,0b0100100, 0b000000,  0b0000000, 0b1001000,       0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0,0b0100100, 0b000000,  0b0000000, 0b1001000,       0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0,0b0100100, 0b000000,  0b0000000, 0b1001000,       0, 0, 0, 0, 0, 0, 0, 0} 
};


uint32_t loop_animation[loop_frames_count][COLS] = {
    {0, 0, 0, 0, 0, 0, 0, 0b0100100, 0b0000000, 0b0000000, 0b1001000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0110100, 0b0000100, 0b1000000, 0b1011000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0010000, 0b0000100, 0b1000000, 0b0010000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0011000, 0b1000100, 0b1000100, 0b0110000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0001000, 0b1000000, 0b0000100, 0b0100000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b1001100, 0b1000000, 0b0000100, 0b1100100, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b1000100, 0b0000000, 0b0000000, 0b1000100, 0, 0, 0, 0, 0, 0, 0, 0},

    {0, 0, 0, 0, 0, 0, 0, 0b1100100, 0b0000100, 0b1000000, 0b1001100, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0100000, 0b0000100, 0b1000000, 0b0001000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0110000, 0b1000100, 0b1000100, 0b0011000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b0010000, 0b1000000, 0b0000100, 0b0010000, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b1011000, 0b1000000, 0b0000100, 0b0110100, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b1001000, 0b0000000, 0b0000000, 0b0100100, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0b1101100, 0b0000000, 0b0000000, 0b1101100, 0, 0, 0, 0, 0, 0, 0, 0},
};
//Preloaded animations
Animation* once_anim;
Animation* loop_anim;
Animation* anim1;
Animation* anim2;
Animation* anim3;

//uint32_t dynamic_animation;
Animation* current_anim;
Frame*     current_frame;
//uint32_t  current_picture[COLS]; //This state array contains the "on"/"off" state of a magnet as how it's supposed to stay through the frame. If the pixel has PWM enabled it will be toggled in the "shift_out_mag_state" array corresponding to it's duty cycle
uint32_t  current_shift_out_mag_state[COLS];  //This state array is used to implement PWM by keeping track of duty cycle

RamMonitor ram;

//Function headers:
void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent
void turnMagnetsOnIn(int* xArr, int y,int xLength, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent

//Functions:
void report_ram_stat(const char *aname, uint32_t avalue)
{
  // Code from RamMonitorExample.cpp
  // copyright Adrian Hunt (c) 2015 - 2016
  Serial.print(aname);
  Serial.print(": ");
  Serial.print((avalue + 512) / 1024);
  Serial.print(" Kb (");
  Serial.print((((float)avalue) / ram.total()) * 100, 1);
  Serial.print("% of ");
  Serial.print((ram.total()+512)/1024 );
  Serial.println(" Kb)");
};

void report_ram()
{ 
  // Code from RamMonitorExample.cpp
  // copyright Adrian Hunt (c) 2015 - 2016
  bool lowmem;
  bool crash;

  Serial.println("==== memory report ====");
  Serial.printf("heapsize: %d\n",ram.heap_used());
  Serial.printf("heapfree: %d\n",ram.heap_free());
  Serial.printf("heaptotal: %d\n",ram.heap_total());
  Serial.printf("stacksize: %d\n",ram.stack_used());
  Serial.printf("stackfree: %d\n",ram.stack_free());
  Serial.printf("stacktotal: %d\n",ram.stack_total());
  Serial.printf("totalfree: %d\n",ram.free());
  Serial.printf("total: %d\n",ram.total());
  report_ram_stat("free", ram.adj_free());
  report_ram_stat("stack", ram.stack_total());
  report_ram_stat("heap", ram.heap_total());

  lowmem = ram.warning_lowmem();
  crash = ram.warning_crash();
  if (lowmem || crash)
  {
    Serial.println();

    if (crash)
      Serial.println("**warning: stack and heap crash possible");
    else if (lowmem)
      Serial.println("**warning: unallocated memory running low");
  };

  Serial.println();
};

//Interrupt functions:

void restart_anim_isr(void)
{
  cli();
  RESTART_ANIM_FLAG = 1;
  sei();
}
void restart_anim(void)
{
  current_anim = anim1;

  current_anim->write_playback_type(ONCE);
  current_anim->write_playback_dir(true);
  current_anim->start_animation(0);
  current_frame = current_anim->get_current_frame();
  animation_num = 0;
  RESTART_ANIM_FLAG = 0;
  delay(200);
  Serial.println("Interrupt");
}

/*
boolean magnetIsToggledThisFrame(int x, int y)
{
  if ( (prevMagnetState[x] & (1 << y) ) == 0 && ( currMagnetState[x] & (1 << y) ) == 1)
  {
    return true;
  }
  return false;
}

void updateTimeAtStart(int x)
{
  //This function is supposed to be a safety feature 
  //that keeps track of how long a magnet has been "on" consecutively.
  for (size_t y = 0; y < COLS; y++)
  {
    if (magnetIsToggledThisFrame(x,y))
    {
      timeAtStart[x][y] = timeThisRefresh;
    }
  }
}
*/

void updateAllStates_async()
{
  for (int x = 0; x < COLS; x++)
  {
    current_shift_out_mag_state[x] = 0;
    for (int y = 0; y < ROWS; y++)
    {
      if (timeTilStart[x][y] <= timeThisRefresh && timeAtEnd[x][y] > timeThisRefresh)
      {
        if(current_frame->get_duty_cycle_at(x,y) > dutyCycleCounter){
          current_shift_out_mag_state[x] |= 1 << y;
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
  if (dutyCycleCounter >= DUTY_CYCLE_RESOLUTION)
    dutyCycleCounter = 0;
}

void update_all_states()
{
  uint32_t local_shift_out_mag_state = 0;
  for (int x = 0; x < COLS; x++)
  {
    local_shift_out_mag_state = current_frame->get_picture_at(x);
    /*
    for (size_t i = 0; i < current_frame->pwm_pixels_x.size(); i++)
    {
      if(current_frame->pwm_pixels_x[i] == x){
        if (current_frame->get_duty_cycle_at(current_frame->pwm_pixels_x[i], current_frame->pwm_pixels_y[i]) < dutyCycleCounter)
        {
          current_shift_out_mag_state[x] ^= 1 << current_frame->pwm_pixels_y[i]; //Toggle (turn off )
        } 
      }
    }*/
    for(int y = 0; y < ROWS; y++){
      uint8_t cduty = current_frame->get_duty_cycle_at(x, y);
      if (dutyCycleCounter > cduty)
      //if (cduty + dutyCycleCounter <= DUTY_CYCLE_RESOLUTION) //inverts the test, so that a PWM cycle ends in the "on" state instead of "off"
      {
        local_shift_out_mag_state &= ~(1 << y); //Clear
      }
    }
    current_shift_out_mag_state[x] = local_shift_out_mag_state;
  }
  dutyCycleCounter++;
  if (dutyCycleCounter >= DUTY_CYCLE_RESOLUTION)
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
  //if(dutyCycleCounter == 0)
    //digitalWrite(PWM_PERIOD_INDICATOR, HIGH);
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
    GPIOC_PDOR = current_shift_out_mag_state[x];
    
    //The following line has not been used properly in the rest of the code, so I've disabled it for now.
    //Ideally it should set a warning flag if the time since a magnet was turned on has passed a threshold
    //so that no magent will stay "on" infinetely.
    //updateTimeAtStart(x);

    //register shifts bits on rising edge of clock pin
    digitalWrite(SHIFT_CLK_PIN, HIGH);
    
    //zero the data pin after shift to prevent bleed through
    GPIOC_PDOR = 0; //digitalWrite(SHIFT_DATA_PIN, LOW);
  }
  //stop shifting
  digitalWrite(SHIFT_CLK_PIN, LOW);
  
  //return the latch pin high to signal chip that it
  //no longer needs to listen for information
  digitalWrite(SHIFT_ENABLE_PIN, HIGH);

  //Indicate to logic analyzer that the shift is done
  //if (dutyCycleCounter == DUTY_CYCLE_RESOLUTION-1)
    //digitalWrite(PWM_PERIOD_INDICATOR, LOW);
}

void refreshScreen()
{
  if(animation_mode == ASYNC_ANIMATION){
    //The animation is based on individual times
    updateAllStates_async();
  }else{
    //The animation is based on a fixed framerate
    update_all_states();
  }


  shiftOut_one_PCB_per_PORT();
}

void turnMagnetsOnIn(int* xArr, int y, int xLength, int inMillis, int forMillis, uint8_t uptime){
  //TODO: Make a FIFO buffer that can hold future (inMillis,forMillis) tuples. (Maybe: https://github.com/rlogiacco/CircularBuffer)
  //The future inMillis must then be modified by subtracting the current inMillis, so that it can be placed in the timeTilStart array when the current inMillis has passed and the magnet is turned on.
  //There also needs to be a check that the future inMillis will happen AFTER the current inMillis+current forMillis, so that the magnet has a pause of at least shortDelay (this minimum delay is a guess).
  int x;
  for (int i = 0; i < xLength; i++) {
    x = xArr[i];
    timeTilStart[x][y] = timeThisRefresh + inMillis;
    timeAtEnd[x][y] = timeThisRefresh + inMillis + forMillis; //Equal to: timeTilStart+forMillis
    if(uptime > 100) uptime = 100;
      async_dutyCycle[x][y] = (uint8_t)((uptime / 100.0) * (DUTY_CYCLE_RESOLUTION-1));
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
  async_dutyCycle[x][y] = (uint8_t)((uptime / 100.0f) * (DUTY_CYCLE_RESOLUTION));
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

void turn_on_magnet_in_frame(int frame_num, int x, int y, uint8_t uptime){
  //TODO: Make it so that this method only changes the dynamic animation, not the preloaded one.
  Frame *frame = current_anim->get_frame(frame_num);
  
  frame->set_pixel(x,y);
  
  if (uptime > 100)
    uptime = 100;
  frame->write_duty_cycle_at(x,y,(uint8_t)((uptime / 100.0f) * (DUTY_CYCLE_RESOLUTION)));

  #if (DEBUG && false) //Using &&false when I don't want this output
    Serial.print("Magnet (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.print(") will turn ON in frame: ");
    Serial.print(frame_num);
    Serial.print(", at:");
    Serial.print(frame.get_duty_cycle(x,y));
    Serial.print("duty. Current frame: ");
    Serial.println(current_anim->get_current_frame_num());
  #endif
}

void movementAlgorithm(){
  #if (DEBUG && false)
    Serial.print("timeThisRefresh: ");
    Serial.println(timeThisRefresh);
    Serial.print("time_for_next_frame: ");
    Serial.println(time_for_next_frame);
  #endif
  if(timeThisRefresh >= time_for_next_frame){
    
    current_anim->goto_next_frame();

    if(current_anim->anim_done()){
      //TODO: Switch to new/next animation?
      //remember to use current_anim->start_animation() after loading the next animation
      Serial.printf("New Anim!\n");
      animation_num++;
      animation_num+=5;
      Serial.printf("Preparing Animation num: %d\n",animation_num);
      if(animation_num == 1){
        /*
        loop_anim->write_max_loop_count(5);
        loop_anim->write_playback_type(LOOP_N_TIMES);
        loop_anim->write_playback_dir(true);

        current_anim = loop_anim;
        current_anim->start_animation();
        current_frame = current_anim->get_current_frame();
        */
        //digitalWrite(SD_READ_INDICATOR, HIGH);
        anim2->read_from_SD_card(sd,20);
        anim2->write_playback_type(ONCE);
        anim2->write_playback_dir(true);
        //digitalWrite(SD_READ_INDICATOR, LOW);
        //digitalWrite(SD_READ_INDICATOR, HIGH);
        
        current_anim = anim2;
        current_anim->start_animation();
        current_frame = current_anim->get_current_frame();
        //digitalWrite(SD_READ_INDICATOR, LOW);
      }
      else if (animation_num == 2)
      {
        /*
        loop_anim->write_max_loop_count(5);
        loop_anim->write_playback_type(LOOP_N_TIMES);
        loop_anim->write_playback_dir(false);

        current_anim = loop_anim;
        current_anim->start_animation(loop_frames_count-1);
        current_frame = current_anim->get_current_frame();
        */
        //anim3->read_from_SD_card(sd, 12);
        anim2->write_playback_type(ONCE);
        anim2->write_playback_dir(false);

        current_anim = anim2;
        current_anim->start_animation(-1);
        current_frame = current_anim->get_current_frame();
        //current_frame->write_duty_cycle_at(9,4,14-animation_num);
        //Serial.printf("Duty: %d/20\n", 14 - animation_num);
      }
      else if (animation_num == 0)
      {
        /*
        once_anim->write_playback_type(ONCE);
        once_anim->write_playback_dir(false);

        current_anim = once_anim;
        current_anim->start_animation(transport_anim_count-1);

        current_frame = current_anim->get_current_frame();
        */
        anim2->write_playback_dir(false);
        current_anim = anim2;
        current_anim->start_animation(-1);
        current_frame = current_anim->get_current_frame();
      }else if (animation_num == 0){
        anim1->write_playback_dir(false);
        current_anim = anim1;
        current_anim->start_animation(-1);
        current_frame = current_anim->get_current_frame();
      }else{
        //No animation is currently loaded, running or prepared to run.
        //Inserting a delay here so the Teensy doesn't go into overdrive, causing it to be unresponsive to programming events.
        current_frame = current_anim->get_frame(-1);
        delay(50);
      }
      //animation_num++;
    }
    if(animation_mode != PRELOADED_ANIMATION){
      //TODO: handle incoming changes
      //(or should this be done more often than once per frame? That would slow down the shifting)
    }

    if (RESTART_ANIM_FLAG)
    {
      restart_anim();

    }

    Serial.print("Preparing Frame: ");
    Serial.print(current_anim->get_current_frame_num());
    Serial.println();
    
    current_frame = current_anim->get_current_frame();

    
    //ram.run();
    //report_ram();
    /*
    if(current_anim->get_current_frame_num() == -1){
      Serial.printf("Presumably empty frame:");
      for (size_t i = 0; i < COLS; i++)
      {
        Serial.printf("%lu",current_frame->get_picture_at(i));
      }
      Serial.println();
    }
    */

    time_for_next_frame += frame_period; //Theoretically the same as timeThisRefresh+frame_period, but in case a turn(ms) is skipped for some reason this will be more accurate.
  }
}

void setup() {
  Serial.begin(9600);
  ram.initialize();
  //Serial.begin(9600);
  debug::init(Serial);
  while(!Serial);
  Serial.println("Serial initiated");

  if (!sd.begin())
  {
    Serial.println("SD initialitization failed. Read unsucessful.");
    sd_present = false;
  }


  for (int i = 0; i < 4; i++)
  {
    pinMode(STATE_SWITCH_PINS[i],INPUT_PULLUP);
    pinMode(BUTTON_PINS[i],INPUT_PULLDOWN);
    if(digitalRead(STATE_SWITCH_PINS[i]) == LOW){
      state_switch_mode = (StateSwitchMode)STATE_SWITCH_PINS[i];
      Serial.printf("State Switch Mode: %d \n",state_switch_mode);
    }else{
      //Serial.printf("StateSwitchMode %d not in use.",STATE_SWITCH_PINS[i]);
    }
  }

  attachInterrupt(BUTTON_UP,restart_anim_isr,RISING);

  pinMode(SHIFT_CLK_PIN,OUTPUT);
  pinMode(SHIFT_ENABLE_PIN,OUTPUT);

  digitalWrite(SHIFT_CLK_PIN, LOW);
  digitalWrite(SHIFT_ENABLE_PIN, HIGH);
  for (int i = 0; i < ALL_ROWS; i++)
  {
    pinMode(SHIFT_DATA_PINS[i],OUTPUT);
  }
  for(int x=0; x<COLS; x++){
    for(int y=0;y<ROWS;y++){
      timeAtEnd[x][y] = 0;
      timeAtStart[x][y] = 0;
      //dutyCycle[x][y] = DUTY_CYCLE_RESOLUTION;
    }
  }

  //Clear all shift registers.
  //Especially important if ALL_ROWS != ROWS
  for (int x = ALL_COLS - 1; x >= 0; x--)
  {
    current_shift_out_mag_state[x] = 0;
  }
  shiftOut_one_PCB_per_PORT();
  for (int i = ALL_ROWS - 1; i > ROWS-1 ; i--)
  {
    //"Deinitialize" pins that are connected but not supposed to be used.
    pinMode(SHIFT_DATA_PINS[i], INPUT);
  }

  ram.run();
  report_ram();
  //Load preloaded frames into objects.
  /*
  for (size_t i = 0; i < transport_anim_count; i++)
  {
    transport_anim_frames[i] = new Frame(transport_animation[i]);
  }

  once_anim = new Animation(transport_anim_frames,transport_anim_count,true,false);
  once_anim->write_playback_type(ONCE); 
  once_anim->write_playback_dir(true);
   */
  //once_anim->save_to_SD_card(1);


  /*
  once_anim->save_to_SD_card(99);
  once_anim->write_playback_type(LOOP); //Set to something other than what's in the file being read below to validate that it's being overwritten
  once_anim->write_playback_dir(false); //Set to something other than what's in the file being read below to validate that it's being overwritten
  once_anim->read_from_SD_card(sd, 99);
  Serial.printf("Type: %d (LOOP = %d)\n",(int)once_anim->get_playback_type(),(int) LOOP);
  Serial.printf("Dir: %d (backwards = %d)\n",(int)once_anim->get_playback_dir(),(int)false);
  */
  /*
  for (size_t i = 0; i < loop_frames_count; i++)
  {
    loop_frames[i] = new Frame(loop_animation[i]);
  }
  loop_anim = new Animation(loop_frames, loop_frames_count,true,false);
  loop_anim->write_max_loop_count(10);
  loop_anim->write_playback_type(LOOP_N_TIMES);
  loop_anim->write_playback_dir(true);
  //loop_anim->save_to_SD_card(2);
  */
  anim1 = new Animation(nullptr, 1);
  //anim1->save_to_SD_card(999);
  if(sd_present){
    anim1->read_from_SD_card(sd, 2);
  }
  anim1->write_playback_type(ONCE);
  anim1->write_playback_dir(true);

  anim2 = new Animation(nullptr, 1);
  anim3 = new Animation(nullptr, 1);

  current_anim = anim1;
  current_anim->start_animation();
  current_frame = current_anim->get_current_frame();  
  
  ram.run();
  report_ram();

  startTime = micros();
  timeThisRefresh = millis();
  time_for_next_frame = timeThisRefresh+frame_period;
  Serial.print("Setup was executed in: ");
  Serial.print(startTime);
  Serial.println("us");
}

void loop(){
  timeThisRefresh = millis();
  
  movementAlgorithm();
  refreshScreen();

    /*
  if(animation_mode == ASYNC_ANIMATION){
    //TODO: Figure out if this is still necessary in async animation or if the frame obj. can be used.
    for(int x=0;x<COLS;x++){
      prevMagnetState[x] = currMagnetState[x];
    }
  }
  */

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
