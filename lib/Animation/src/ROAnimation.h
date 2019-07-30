/*
  Animation.h - library for storing Animations for Fetch
  Copyright (c) 20019 Simen E. Sørensen. 
*/

// ensure this library description is only included once
#ifndef ROAnimation_h
#define ROAnimation_h

#include "Animation.h"
#include <Arduino.h>
#include <vector>

#define DUTY_CYCLE_RESOLUTION 20
//holders for infromation you're going to pass to shifting function
/*const int ALL_ROWS = 10;   //The total number of rows in the actual hardware
const int ALL_COLS = 19;   //The total number of columns in the actual hardware
const int ROWS = ALL_ROWS; //12; //The number of rows that are in use in the current program (different from ALL_ROWS in order to scale down the number of bits shifted out)
const int COLS = ALL_COLS; //21; //The number of cols that are in use in the current program (different from ALL_COLS in order to scale down the number of bits shifted out)

const int REGISTERS = ROWS;       // no of register series (indicating no of magnet-driver-PCBs connected to the Arduino)
const int BYTES_PER_REGISTER = 4; // no of 8-bit shift registers in series per line (4 = 32 bits(/magnets))
*/
const uint32_t empty_picture[COLS] PROGMEM = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//Read only frame
class ROFrame
{
public:
    ROFrame(const uint32_t *picture, const uint8_t **duty_cycle = nullptr ,const int cols = COLS, const int rows = ROWS);
    ~ROFrame();
    void        delete_frame(void);
    const uint32_t *get_picture();
    const uint32_t get_picture_at(int x) const;
    const uint8_t **get_duty_cycle();
    const uint8_t get_duty_cycle_at(int x, int y);

    uint8_t *pwm_pixels_x;
    uint8_t *pwm_pixels_y;

private:
    int         _cols;
    int         _rows;

    const uint8_t **  _duty_cycle;
    const uint32_t *  _picture;

    void _delete_picture();
    void _delete_duty_cycle();
};
/*
enum PlaybackType
{
    ONCE,
    LOOP,
    BOUNCE,
    LOOP_N_TIMES //Add "STATIC_IMAGE" here?
};

enum PlaybackState
{
    IDLE, //could be called "DONE", but until a "MagnetMatrix" class controls playbacks (and can change from "done" to "not started" when ending an animation), the name may be confusing.
    RUNNING,
    ERROR
};*/

class ROAnimation
{
public:
    ROAnimation(const ROFrame **frames, const int num_frames,const int cols = COLS,const int rows = ROWS);
    void    delete_anim(void);
    const ROFrame*  get_frame(int frame_num);
    int     get_current_frame_num();
    void    goto_next_frame();
    void    goto_prev_frame();
    const ROFrame *get_current_frame();
    const ROFrame *get_next_frame();
    const ROFrame *get_prev_frame();

    void    start_animation(int start_frame = 0);
    void    write_playback_dir(bool forward);
    void    write_max_loop_count(int n);
    bool    get_playback_dir();
    bool    anim_done();
    void    write_playback_type(PlaybackType type);
    PlaybackType get_playback_type();

private:
    int             _cols;
    int             _rows;
    int             _num_frames; 

    PlaybackType    _playback_type  = LOOP;
    PlaybackState   _playback_state = IDLE;

    bool            _dir_fwd = true;
    int             _current_frame = 0;
    int             _prev_frame = -1;
    int             _loop_iteration = 0;
    int             _max_iterations = -1;
    int             _start_idx = 0; //Where the animation started (not necessarily first index in array)
    

    const ROFrame **_frames;
    const ROFrame   _blank_frame PROGMEM = ROFrame(empty_picture); //used as return statement when playback_state is DONE
    int             _get_next_frame_idx();
    int             _get_prev_frame_idx();
    bool            _current_frame_is_on_edge();
};

#endif