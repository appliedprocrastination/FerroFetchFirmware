#include <Arduino.h>
#include <arm_math.h>

#include <MagnetControllerV2.h>
#include <Animation.h>
#include <SdFat.h>
//#include "debug.h"
#include "RamMonitor.h"
//#include "FreeStack.h"
#include <RTCLib.h>

//int I2C_SCK = 19; #These are defined in "Wire.h", so we don't need to redefine them, but they are written down for future reference
//int I2C_SDA = 18; #These are defined in "Wire.h", so we don't need to redefine them, but they are written down for future reference

MagnetController fetch = MagnetController(1, 0b1000000, 21, 2);
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

int animation_num = 0;
int initial_animation_number = 10;
int alternative_animation_number = 99; //Used at Skaperfestivalen to display user made animations
int clock_anim_num = 0;

//Preloaded animations
Animation* anim1;
Animation* anim2; //For testing animation-merge

//uint32_t dynamic_animation;
Animation *current_anim;

RamMonitor ram;

//Function headers:
void turnMagnetOnIn(int x, int y, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent
void turnMagnetsOnIn(int* xArr, int y,int xLength, int inMillis, int forMillis, uint8_t uptime=100); //Uptime in percent

void start_current_anim(void){
  fetch.playAnimation(current_anim);
}

void start_anim_num(int number){
  //TODO: Add test to check if animation exists
  Serial.printf("Preparing Animation num: %d\n", number);

  current_anim->read_from_SD_card(sd,number);
  animation_num = number;

  //START testing for animation merge
  anim2->read_from_SD_card(sd, 100);
  Serial.printf("Performing a merge test with A100. SEARCH FOR THIS STRING TO FIND CODE\n");
  current_anim->set_location(0, 0);
  current_anim->set_origin(0, 0);
  anim2->set_location(0, 0);
  anim2->set_origin(0, 0);
  int test = current_anim->merge_with(anim2);
  Serial.printf("Merge returned with: %d\n",test);
  //END testing for animation merge

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

void animation_done_event_handler(void)
{
  //TODO: Do whatever you want to do when an animation is done playing. e.g. switch to next animation
  if (PLAY_ANIM_FLAG)
  {
    play_anim_button();
  }
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

  ram.run();
  report_ram(ram);

  anim1 = new Animation(nullptr, 1);
  
  animation_num = initial_animation_number;

  if(sd_present){
    anim1->read_from_SD_card(sd, animation_num);
  }


  //START testing for animation merge
  anim2 = new Animation(nullptr, 1);
  //END testing for animation merge
  
  current_anim = anim1;
  check_switch_state();
  
  ram.run();
  report_ram(ram);

  fetch.playAnimation(current_anim,animation_done_event_handler);

  startTime = micros();
  Serial.print("Setup was executed in: ");
  Serial.print(startTime);
  Serial.println("us");
}

void loop(){
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
  
  fetch.animationManagement();
  //movementAlgorithm();
  //refreshScreen();
  if(SWITCH_FLAG){
    check_switch_state();
  }

  if (RESTART_ANIM_FLAG)
  {
    restart_anim();
  }

  if (NEXT_ANIM_FLAG)
  {
    next_anim_button();
  }

  if (PREV_ANIM_FLAG)
  {
    prev_anim_button();
  }

  if (animation_mode == CLOCK_ANIMATION)
  {
    if (rtc_is_present)
    {
      if (rtc_now.minute() > current_minute)
      {
        current_minute = rtc_now.minute();
        clock_anim_num = ((int)rtc_now.hour()) << 6 | ((int)rtc_now.minute()); //Highest possible minute value is 60d=11100b (6-bit). The number is unique and a corresponding anim is stored on SD card
        Serial.printf("Clock Anim Num: %d\n", clock_anim_num);
        start_anim_num(clock_anim_num);
      }
    }
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
