#include <Arduino.h>
#include <arm_math.h>

#include "Animation.h"
//#include "SdFat.h"
//#include "debug.h"
#include "RamMonitor.h"
//#include "FreeStack.h"
#include <RTCLib.h>

//int I2C_SCK = 19; #These are defined in "Wire.h", so we don't need to redefine them, but they are written down for future reference
//int I2C_SDA = 18; #These are defined in "Wire.h", so we don't need to redefine them, but they are written down for future reference

RTC_DS3231 rtc;
bool rtc_is_present = true;
DateTime rtc_now;
float rtc_temperature = 0;
uint8_t current_minute = 0;

const bool DEBUG = true;
int counter = 0;                //debugvariables
unsigned long startTime = 0;    //debugvariables

SdFatSdioEX sd;
bool sd_present = true;

//PRELOADED_ANIMATION:  The entire animation is written into memory before the program is compiled
//DYNAMIC_ANIMATION:    The animation can be changed/updated dynamically, but is based on a fixed framerate
//ASYNC_ANIMATION:      The animation is independent of the framerate and a pixel can be set at any given time.
//CLOCK_ANIMATION:      The Ferrofluid Display acts as a clock with 1 minute resolution.
enum AnimationMode{PRELOADED_ANIMATION, DYNAMIC_ANIMATION, ASYNC_ANIMATION, CLOCK_ANIMATION};
AnimationMode animation_mode = PRELOADED_ANIMATION;

int STATE_SWITCH_PINS[4] = {27,26,25,4};
enum StateSwitchMode{ LEFT = 27,
                      MIDLEFT = 26,
                      MIDRIGHT = 25,
                      RIGHT = 4};
StateSwitchMode state_switch_mode;

int BUTTON_LEFT = 5;
int BUTTON_UP   = 6;
int BUTTON_RIGHT= 16;
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
volatile uint8_t NEXT_ANIM_FLAG = 0;
volatile uint8_t PREV_ANIM_FLAG = 0;
volatile uint8_t PLAY_ANIM_FLAG = 0;
volatile uint8_t SWITCH_FLAG = 0;

//Interrupt timeout
volatile uint32_t interrupt_timeout = 0;
volatile uint32_t interrupt_timeout_switch = 0;
const uint32_t interrupt_timeout_delta = 1000; //ms

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
unsigned long frame_rate    = 2; //fps. 8 FPS seems to be the maximum our ferrofluid can handle 02.Sept.2019
unsigned long frame_period   = 1000/frame_rate; //ms
int animation_num = 0;
int initial_animation_number = 10;
int alternative_animation_number = 99; //Used at Skaperfestivalen to display user made animations
int clock_anim_num = 0;
//int frame = 1;

//States
const int transport_anim_count = 20;
Frame *  transport_anim_frames[transport_anim_count];

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


//Preloaded animations
Animation* transport_anim;
Animation* anim1;

//uint32_t dynamic_animation;
Animation* current_anim;
Frame*     current_frame;
//uint32_t  current_picture[COLS]; //This state array contains the "on"/"off" state of a magnet as how it's supposed to stay through the frame. If the pixel has PWM enabled it will be toggled in the "shift_out_mag_state" array corresponding to it's duty cycle
uint32_t  current_shift_out_mag_state[COLS];  //This state array is used to implement PWM by keeping track of duty cycle

RamMonitor ram;

//Function headers:
void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent
void turnMagnetsOnIn(int* xArr, int y,int xLength, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent

void start_current_anim(void){
  if (current_anim->get_playback_dir())
  {
    //dir = true = forward
    current_anim->start_animation_at(0);
  }else{
    current_anim->start_animation_at(-1);
  }
  current_frame = current_anim->get_current_frame();
}

void start_anim_num(int number){
  //TODO: Add test to check if animation exists
  Serial.printf("Preparing Animation num: %d\n", number);

  current_anim->read_from_SD_card(sd,number);
  animation_num = number;
  start_current_anim();
}
//Interrupt functions:

void restart_anim_isr(void)
{
  if (millis() > interrupt_timeout)
  {
    cli();
    RESTART_ANIM_FLAG = 1;
    sei();
  }
}

void next_anim_isr(void){
  if (millis() > interrupt_timeout)
  {
    cli();
    NEXT_ANIM_FLAG = 1;
    sei();
  }
}

void prev_anim_isr(void)
{
  if (millis() > interrupt_timeout)
  {
    cli();
    PREV_ANIM_FLAG = 1;
    sei();
  }
}

void play_anim_isr(void)
{
  if (millis() > interrupt_timeout)
  {
    cli();
    PLAY_ANIM_FLAG = 1;
    sei();
  }
}

void switch_turned_isr(void){
  if (millis() > interrupt_timeout_switch)
  {
    cli();
    SWITCH_FLAG = 1;
    sei();
  }
}

void restart_anim(void)
{
  Serial.println("Up button pushed");
  if (state_switch_mode == RIGHT)
  {
    start_anim_num(initial_animation_number);
  }else if(state_switch_mode == MIDRIGHT){
    start_anim_num(alternative_animation_number);
  }else{
    //Error, assume default mode (RIGHT)
    start_anim_num(initial_animation_number);
  }
  

  RESTART_ANIM_FLAG = 0;
  interrupt_timeout = millis() + interrupt_timeout_delta;
}

void next_anim_button(void)
{
  Serial.println("Right button pressed");
  if (state_switch_mode != MIDRIGHT)
  {
    start_anim_num(animation_num+1);
  }else{
    //ignore this button in MIDRIGHT mode
  }
  

  NEXT_ANIM_FLAG = 0;
  interrupt_timeout = millis() + interrupt_timeout_delta;
}

void prev_anim_button(void)
{
  Serial.println("Left button pressed");
  if (state_switch_mode != MIDRIGHT)
  {
    start_anim_num(animation_num - 1);
  }
  else
  {
    //ignore this button in MIDRIGHT mode
  }

  PREV_ANIM_FLAG = 0;
  interrupt_timeout = millis() + interrupt_timeout_delta;
}

void play_anim_button(void){
  Serial.printf("Play button pressed. Stating animation number: %d\n",animation_num);
  start_current_anim();

  PLAY_ANIM_FLAG = 0;
  interrupt_timeout = millis() + interrupt_timeout_delta;
}

void check_switch_state(void){
  uint32_t switch_states[4];
  uint8_t numlow = 0;

  delay(50); //This is not optimal, because if the switch is turned during an animation, the animation will temporarily be stopped. Can implement a timeout variable instead, just cant be arsed right now.
  
  Serial.print("Switch pins: ");
  for (uint8_t i = 0; i < 4; i++)
  {
    switch_states[i] = digitalRead(STATE_SWITCH_PINS[i]);
    if(switch_states[i] == 0){
      numlow++;
      state_switch_mode = (enum StateSwitchMode) STATE_SWITCH_PINS[i];
    }
    Serial.print(switch_states[i]);
  }
  Serial.println();

  if(numlow > 1){
    check_switch_state();
  }else{
    SWITCH_FLAG = 0;
    interrupt_timeout_switch = millis() + 10;
    switch (state_switch_mode)
    {
    case LEFT:
      animation_mode = PRELOADED_ANIMATION;
      sd.chdir("/");
      /* not implemented */
      break;
    case MIDLEFT:
      if(rtc_is_present){
        animation_mode = CLOCK_ANIMATION;
        bool test = sd.chdir("CLK");
        Serial.printf("Changed dir to CLK: %d\n",test);
        clock_anim_num = ((int)rtc_now.hour())<<6 | ((int)rtc_now.minute()); //Highest possible minute value is 60d=11100b (6-bit). The number is unique and a corresponding anim is stored on SD card
        Serial.printf("Clock Anim Num: %d\n",clock_anim_num);
        start_anim_num(clock_anim_num);
      }
      break;
    case MIDRIGHT:
      animation_mode = PRELOADED_ANIMATION;
      sd.chdir("/");
      animation_num = alternative_animation_number;
      Serial.printf("Animation number: %d\n",animation_num);
      break;
    case RIGHT:
      animation_mode = PRELOADED_ANIMATION;
      sd.chdir("/");
      animation_num = initial_animation_number;
      Serial.printf("Animation number: %d\n", animation_num);
      break;
    default:
      animation_mode = PRELOADED_ANIMATION;
      sd.chdir("/");
      Serial.println("No state detected");
      Serial.printf("State swith mode: %d\n",state_switch_mode);
      break;
    }
  }
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
    if(animation_mode == CLOCK_ANIMATION){
      if (rtc_is_present)
      {        
        if (rtc_now.minute() > current_minute)
        {
          current_minute = rtc_now.minute();
          clock_anim_num = ((int)rtc_now.hour())<<6 | ((int)rtc_now.minute()); //Highest possible minute value is 60d=11100b (6-bit). The number is unique and a corresponding anim is stored on SD card
          Serial.printf("Clock Anim Num: %d\n",clock_anim_num);
          start_anim_num(clock_anim_num);
          time_for_next_frame = timeThisRefresh;
        }
      }
    }
  if(timeThisRefresh >= time_for_next_frame){
    
    current_anim->goto_next_frame();

    if(current_anim->anim_done()){
      //TODO: Switch to new/next animation?
      //remember to use current_anim->start_animation_at() after loading the next animation
      if(PLAY_ANIM_FLAG){
        play_anim_button();
      }
    }

    if (RESTART_ANIM_FLAG)
    {
      restart_anim();
    }

    if(NEXT_ANIM_FLAG){
      next_anim_button();
    }

    if(PREV_ANIM_FLAG){
      prev_anim_button();
    }

    Serial.print("Preparing Frame: ");
    Serial.print(current_anim->get_current_frame_num());
    Serial.println();
    
    current_frame = current_anim->get_current_frame();

    
    //ram.run();
    //report_ram(ram);
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

    time_for_next_frame += frame_period; 
  }
}

void setup() {
  Serial.begin(9600);
  ram.initialize();
  //Serial.begin(9600);
  //debug::init(Serial);
  //while(!Serial);
  while(millis()<5000){
    if(Serial){
      break;
    }
  }

  if (!sd.begin())
  {
    Serial.println("SD initialitization failed. Read unsucessful.");
    sd_present = false;
  }
  Serial.println("Serial initiated");

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    rtc_is_present = false;
  }
  if(rtc_is_present){
    if (rtc.lostPower())
    {
      Serial.println("RTC lost power, lets set the time.!");
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    rtc_now = rtc.now();
    rtc_temperature = rtc.getTemperature();
  }


  for (int i = 0; i < 4; i++)
  {
    pinMode(STATE_SWITCH_PINS[i],INPUT_PULLUP);
    attachInterrupt(STATE_SWITCH_PINS[i],switch_turned_isr,FALLING);

    pinMode(BUTTON_PINS[i],INPUT_PULLDOWN);
    if(digitalRead(STATE_SWITCH_PINS[i]) == LOW){
      state_switch_mode = (StateSwitchMode)STATE_SWITCH_PINS[i];
      Serial.printf("State Switch Mode: %d \n",state_switch_mode);
    }else{
      //Serial.printf("StateSwitchMode %d not in use.",STATE_SWITCH_PINS[i]);
    }
  }

  attachInterrupt(BUTTON_UP,restart_anim_isr,RISING);
  attachInterrupt(BUTTON_RIGHT, next_anim_isr, RISING);
  attachInterrupt(BUTTON_LEFT, prev_anim_isr, RISING);
  attachInterrupt(BUTTON_DOWN, play_anim_isr, RISING);

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
  report_ram(ram);

  anim1 = new Animation(nullptr, 1, true);
  
  animation_num = initial_animation_number;

  if(sd_present){
    anim1->read_from_SD_card(sd, animation_num);
  }

  current_anim = anim1;
  current_frame = current_anim->get_current_frame();
  //start_current_anim();
  check_switch_state();
  
  ram.run();
  report_ram(ram);

  startTime = micros();
  timeThisRefresh = millis();
  time_for_next_frame = timeThisRefresh+frame_period;
  Serial.print("Setup was executed in: ");
  Serial.print(startTime);
  Serial.println("us");
}
void loop(){
  timeThisRefresh = millis();
  if (rtc_is_present)
  {
    rtc_now = rtc.now();
    rtc_temperature = rtc.getTemperature();
    
    if (rtc_now.minute() > current_minute)
    {
      Serial.printf("One minute has passed. New Time: %2d:%2d:%2d\n",rtc_now.hour(),rtc_now.minute(),rtc_now.second());
      if (animation_mode != CLOCK_ANIMATION)
      {
        current_minute = rtc_now.minute();
      }
      
    }
  }
  

  movementAlgorithm();
  refreshScreen();
  if(SWITCH_FLAG){
    check_switch_state();
  }
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
