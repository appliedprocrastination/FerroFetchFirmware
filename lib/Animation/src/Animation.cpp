#include "Animation.h"

//Constructor
Frame::Frame(uint32_t *picture, uint8_t **duty_cycle, int cols, int rows)
{
    _cols = cols;
    _rows = rows;

    if (duty_cycle == nullptr)
    {
        _duty_cycle = new uint8_t *[cols];
        for (int col = 0; col < cols; col++)
        {
            _duty_cycle[col] = new uint8_t[rows];
            for (int row = 0; row < rows; row++)
            {
                _duty_cycle[col][row] = (uint8_t)DUTY_CYCLE_RESOLUTION;
            }
        }
    }
    else
    {
        _duty_cycle = duty_cycle;
        for (int x = 0; x < cols; x++)
        {
            for (int y = 0; y < rows; y++)
            {
                if (duty_cycle[x][y] < DUTY_CYCLE_RESOLUTION)
                {
                    pwm_pixels_x.push_back(x);
                    pwm_pixels_y.push_back(y);
                }
            }
        }
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
Frame::~Frame()
{
    this->delete_frame();
}

// Public Methods
void Frame::delete_frame(void)
{
    pwm_pixels_x.clear();
    pwm_pixels_x.shrink_to_fit();
    pwm_pixels_y.clear();
    pwm_pixels_y.shrink_to_fit();
    //TODO: Is the above necessary?
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

void Frame::overwrite_duty_cycle(uint8_t **duty_cycle)
{
    _delete_duty_cycle();
    _duty_cycle = duty_cycle;
}

void Frame::write_duty_cycle_at(int x, int y, uint8_t duty_cycle)
{
    if (duty_cycle > DUTY_CYCLE_RESOLUTION)
    {
        duty_cycle = DUTY_CYCLE_RESOLUTION;
    }

    if ((duty_cycle == DUTY_CYCLE_RESOLUTION) && (_duty_cycle[x][y] != DUTY_CYCLE_RESOLUTION))
    {
        //The coordinate is already stored in the pwm_pixels vectors,
        //but the new value is not a PWM value, so the coord. must be removed
        for (size_t i = 0; i < pwm_pixels_x.size(); i++)
        {
            if (pwm_pixels_x[i] == x && pwm_pixels_y[i] == y)
            {
                pwm_pixels_x.erase(pwm_pixels_x.begin() + i);
                pwm_pixels_y.erase(pwm_pixels_y.begin() + i);
                break;
            }
        }
    }
    else if ((duty_cycle < DUTY_CYCLE_RESOLUTION) && (duty_cycle > 0) && (_duty_cycle[x][y] == DUTY_CYCLE_RESOLUTION))
    {
        //The coordinates are not stored in the pwm_pixels vectors
        pwm_pixels_x.push_back(x);
        pwm_pixels_y.push_back(y);
    }
    else
    {
        //duty_cycle is either max or minimum (or see next line), no need to check for those values in the refresh screen routine.
        //the third possibility is that the coordinate is already stored in the pwm_pixels vectors, so no need to add them.
    }
    _duty_cycle[x][y] = duty_cycle;
}
void Frame::write_picture(uint32_t *picture)
{
    _delete_picture();
    _picture = picture;
}

void Frame::write_pixel(int x, int y, bool state)
{
    if (state)
    {
        set_pixel(x, y);
    }
    else
    {
        clear_pixel(x, y);
    }
}
void Frame::set_pixel(int x, int y)
{
    _picture[x] |= 1 << y;
}
void Frame::clear_pixel(int x, int y)
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
Animation::Animation(Frame **frames, int num_frames, int cols, int rows)
{
    _num_frames = num_frames;
    _cols = cols;
    _rows = rows;
    _current_frame = 0;

    if (frames == nullptr)
    {
        _frames = new Frame *[num_frames];
        for (int frame = 0; frame < num_frames; frame++)
        {
            _frames[frame] = new Frame();
        }
    }
    else
    {
        _frames = frames;
    }
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

Frame *Animation::get_frame(int frame_num)
{
    return _frames[frame_num];
}
void Animation::set_frame(int frame_num, Frame *frame)
{
    _frames[frame_num] = frame;
}
void Animation::load_frames_from_array(uint32_t **animation, uint8_t ***duty_cycle)
{
    for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->write_picture(animation[frame]);
        if (duty_cycle != nullptr)
        {
            _frames[frame]->overwrite_duty_cycle(duty_cycle[frame]);
        }
    }
}

int Animation::get_current_frame_num()
{
    return _current_frame;
}

void Animation::goto_next_frame()
{
    _current_frame = _get_next_frame_idx();
    //TODO: implement a "looping_anim" variable
    //if loop=false return enum "ANIM_DONE" when _current_frame == 0
    //this can be built on in the main program
    //where several animations can follow each other,
    //or a static image can be displayed after an animation
}
Frame *Animation::get_current_frame()
{
    return _frames[_current_frame];
}
Frame *Animation::get_next_frame()
{
    return _frames[_get_next_frame_idx()];
}
Frame *Animation::get_prev_frame()
{
    return _frames[_get_prev_frame_idx()];
}

// Private Methods

int Animation::_get_next_frame_idx()
{
    if (_current_frame < _num_frames - 1)
    {
        return _current_frame + 1;
    }
    else
    {
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
        return _num_frames - 1;
    }
}