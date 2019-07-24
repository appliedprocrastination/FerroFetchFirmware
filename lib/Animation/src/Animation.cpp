#include "Animation.h"

//Constructor
Frame::Frame(int cols, int rows, uint8_t **duty_cycle, uint32_t *picture)
{
    _cols = cols;
    _rows = rows;

    if (duty_cycle == nullptr)
    {
        _duty_cycle = new uint8_t *[cols];
        for (int col = 0; col < cols; col++)
        {
            _duty_cycle[col] = new uint8_t[rows];
        }
    }
    else
    {
        _duty_cycle = duty_cycle;
    }

    if (picture == nullptr)
    {
        _picture = new uint32_t[cols];
    }
    else
    {
        _picture = picture;
    }
}

// Public Methods
void Frame::delete_frame(void)
{
    _delete_duty_cycle();
    _delete_picture();
}
uint32_t *Frame::get_picture()
{
    return _picture;
}
uint32_t Frame::get_picture_at(int x)
{
    return _picture[x];
}
uint8_t **Frame::get_duty_cycle()
{
    return _duty_cycle;
}
uint8_t Frame::get_duty_cycle_at(int x, int y)
{
    return _duty_cycle[x][y];
}
inline void Frame::write_picture(uint32_t* picture)
{
    _delete_picture();
    _picture = picture;
}

inline void Frame::write_pixel(int x, int y, bool state)
{
    if (state)
    {
        set_pixel(x, y);
    }else
    {
        clear_pixel(x, y);
    }
}
inline void Frame::set_pixel(int x, int y)
{
    _picture[x] |= 1 << y;
}
inline void Frame::clear_pixel(int x, int y)
{
    _picture[x] &= ~(1 << y);
}

// Private Methods
inline void Frame::_delete_picture()
{
    delete _picture;
}

inline void Frame::_delete_duty_cycle()
{
    for (int col = 0; col < _cols; col++)
    {
        delete _duty_cycle[col];
    }
    delete _duty_cycle;
}

//Constructor
Animation::Animation(int num_frames, int cols, int rows, Frame **frames)
{
    _num_frames = num_frames;
    _cols = cols;
    _rows = rows;
    _current_frame = 0;

    if(frames == nullptr){
        _frames = new Frame*[num_frames];
        for (int frame = 0; frame < num_frames; frame++)
        {
            _frames[frame] = new Frame(cols,rows);

        }
    }else{
        _frames = frames;
    }
    
}

Frame* Animation::get_frame(int frame_num)
{
    return _frames[frame_num];
}
void Animation::set_frame(int frame_num, Frame* frame)
{
    _frames[frame_num] = frame;
}  

// Public Methods 
void Animation::delete_anim(void)
{
    for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->delete_frame();
    }
    delete _frames;

}
void Animation::goto_next_frame(){
    _current_frame = _get_next_frame_idx();
    //TODO: implement a "looping_anim" variable
    //if loop=false return enum "ANIM_DONE" when _current_frame == 0
    //this can be built on in the main program
    //where several animations can follow each other,
    //or a static image can be displayed after an animation
}
Frame *Animation::get_current_frame(){
    return _frames[_current_frame];
}
Frame *Animation::get_next_frame(){
    return _frames[_get_next_frame_idx()];
}
Frame *Animation::get_prev_frame(){
    return _frames[_get_prev_frame_idx()];
}

// Private Methods 

int Animation::_get_next_frame_idx(){
    if (_current_frame < _num_frames-1)
    {
        return _current_frame+1;
    }else{
        return 0;
    }
}

int Animation::_get_prev_frame_idx()
{
    if (_current_frame > 0)
    {
        return _current_frame - 1;
    }
    else
    {
        return _num_frames-1;
    }
}