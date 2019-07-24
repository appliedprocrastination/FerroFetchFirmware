#include "Animation.h"

//Constructor
Frame::Frame(int cols, int rows, uint8_t **duty_cycle, uint32_t *animation)
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

    if (animation == nullptr)
    {
        _animation = new uint32_t[cols];
    }
    else
    {
        _animation = animation;
    }
}

// Public Methods
void Frame::delete_frame(void)
{
    _delete_duty_cycle();
    _delete_animation();
}
uint32_t *Frame::get_animation()
{
    return _animation;
}
uint32_t Frame::get_animation_at(int x)
{
    return _animation[x];
}
uint8_t **Frame::get_duty_cycle()
{
    return _duty_cycle;
}
uint8_t Frame::get_duty_cycle_at(int x, int y)
{
    return _duty_cycle[x][y];
}
inline void Frame::write_animation(uint32_t* animation)
{
    _delete_animation();
    _animation = animation;
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
    _animation[x] |= 1 << y;
}
inline void Frame::clear_pixel(int x, int y)
{
    _animation[x] &= ~(1 << y);
}

// Private Methods
inline void Frame::_delete_animation()
{
    delete _animation;
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


// Private Methods 
