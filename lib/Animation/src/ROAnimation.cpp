#include "ROAnimation.h"

//Constructor
ROFrame::ROFrame(const uint32_t *picture, const uint8_t **duty_cycle, const int cols, const int rows)
{
    _cols = cols;
    _rows = rows;

   
    _picture = picture;
    _duty_cycle = duty_cycle;
    for (int x = 0; x < cols; x++)
    {
        for (int y = 0; y < rows; y++)
        {
            if (duty_cycle[x][y] < DUTY_CYCLE_RESOLUTION)
            {
                //pwm_pixels_x.push_back(x);
                //pwm_pixels_y.push_back(y);
            }
        }
    }
    

}
ROFrame::~ROFrame()
{
    this->delete_frame();
}

// Public Methods
void ROFrame::delete_frame(void)
{
    /*
    pwm_pixels_x.clear();
    pwm_pixels_x.shrink_to_fit();
    pwm_pixels_y.clear();
    pwm_pixels_y.shrink_to_fit();
    */
    //TODO: Is the above necessary?
    _delete_duty_cycle();
    _delete_picture();
}
const uint32_t *ROFrame::get_picture()
{
    return _picture;
}
const uint32_t ROFrame::get_picture_at(int x) const
{
    return _picture[x];
}
const uint8_t **ROFrame::get_duty_cycle()
{
    return _duty_cycle;
}
const uint8_t ROFrame::get_duty_cycle_at(int x, int y)
{
    return _duty_cycle[x][y];
}

// Private Methods
inline void ROFrame::_delete_picture()
{
    delete _picture;
}

inline void ROFrame::_delete_duty_cycle()
{
    for (int col = 0; col < _cols; col++)
    {
        delete _duty_cycle[col];
    }
    delete _duty_cycle;
}

//Constructor
ROAnimation::ROAnimation(const ROFrame **frames, const int num_frames,const int cols, const int rows)
{
    _num_frames = num_frames;
    _cols = cols;
    _rows = rows;

    _frames = frames;
    //_blank_frame = Frame(empty_picture);
}

// Public Methods
void ROAnimation::delete_anim(void)
{
    /* for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->delete_frame();
    }*/
    delete _frames;
}

const ROFrame *ROAnimation::get_frame(int frame_num)
{
    return _frames[frame_num];
}


int ROAnimation::get_current_frame_num()
{
    return _current_frame;
}

void ROAnimation::goto_next_frame()
{
    if (_current_frame_is_on_edge())
    {
        //Current frame IS on the edge
        switch (_playback_type)
        {
        case LOOP:
            if (_current_frame == _start_idx)
            {
                _loop_iteration++;
            }
            break;
        case ONCE:
            break;
        case BOUNCE:
            _loop_iteration++; //not checked for current_frame vs start_idx because we want to bounce on both ends of the array
            if (_loop_iteration != 1)
            {
                //First iteration we do not want to toggle
                _dir_fwd = !_dir_fwd;
            }
            break;
        case LOOP_N_TIMES:
            if (_current_frame == _start_idx)
            {
                _loop_iteration++;
            }

            break;
        default:
            _playback_state = ERROR;
            break;
        }
    }

    _prev_frame = _current_frame;
    _current_frame = _get_next_frame_idx();

    if(_playback_state == ERROR){
        //TODO: Handle this error. Letting it slip to IDLE for now.
        _current_frame = -1;
    }
    if(_current_frame == -1){
        _playback_state = IDLE;
    }
}
void ROAnimation::goto_prev_frame()
{
    //This whole function may be unnecessary, since going back and forth is
    //handled in the goto_next_frame and get_next_frame_idx methods
    int tmp_prev_frame = _current_frame;
    _current_frame = _prev_frame; //TODO: use private method _get_prev_frame_idx()?
    _prev_frame = tmp_prev_frame;
    
}
const ROFrame *ROAnimation::get_current_frame()
{
    if (_current_frame == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    return _frames[_current_frame];
}
const ROFrame *ROAnimation::get_next_frame()
{
    int idx = _get_next_frame_idx();
    if (idx == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    return _frames[idx];
}
const ROFrame *ROAnimation::get_prev_frame()
{
    if (_prev_frame == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    
    return _frames[_prev_frame];
}

void ROAnimation::start_animation(int start_frame)
{

    _playback_state = RUNNING;
    _start_idx = start_frame;
    _current_frame = start_frame;
    _loop_iteration = 0;
}
void ROAnimation::write_playback_dir(bool forward)
{
    _dir_fwd = forward;
}
void ROAnimation::write_max_loop_count(int n)
{
    _max_iterations = n;
}

bool ROAnimation::get_playback_dir()
{
    return _dir_fwd;
}

bool ROAnimation::anim_done()
{
    if(_current_frame == -1){
        return true;
    }
    return false;
}
void ROAnimation::write_playback_type(PlaybackType type)
{
    _playback_type = type;
}
PlaybackType ROAnimation::get_playback_type()
{
    return _playback_type;
}

// Private Methods

int ROAnimation::_get_next_frame_idx()
{
    //NOTE TO SELF: This function should not modify any variables!
    if (!_current_frame_is_on_edge())
    {
        //Current frame is NOT on the edge
        if(_dir_fwd){
            return _current_frame + 1;
        }else{
            return _current_frame - 1;
        }

        //TODO: Add case for "ONCE" where the animation does not start at 0 or num_frames - 1
        //When that is done, the case of how to handle "edges" in the frame array for the "ONCE" case 
        //also needs to be changed.
    }
    else
    {
        //Current frame IS on the edge
        switch (_playback_type)
        {
        case LOOP:
            if (_dir_fwd)
            {
                if(_current_frame != 0){
                    return 0;
                }
                return _current_frame + 1;
            }
            if(_current_frame == 0){
                return _num_frames - 1;
            }
            return _current_frame - 1;
            
        case ONCE:
            if(_current_frame != _start_idx){
                return -1; //signals that an "empty frame" should be returned from the function calling this
            }
            if(_dir_fwd){
                if(_current_frame == 0){
                    return _current_frame + 1;
                }
                return 0; //the start condition was the last frame but direction forward
            }
            if(_current_frame != 0){
                return _current_frame - 1;
            }
            return _num_frames - 1;//The start condition was 0 but direction backward
        case BOUNCE:
            if (_dir_fwd)
            {
                return _current_frame + 1;
            }
            return _current_frame - 1;
            
            
        case LOOP_N_TIMES:
            if (_loop_iteration >= _max_iterations)
            {
                //using >= in case _max_iterations has illegal value (negative value)
                return -1; //signals that an "empty frame" should be returned from the function calling this
            }
            if (_dir_fwd)
            {
                if (_current_frame != 0)
                {
                    return 0;
                }
                return _current_frame + 1;
            }
            if (_current_frame == 0)
            {
                return _num_frames - 1;
            }
            return _current_frame - 1;

        default:
            _playback_state = ERROR;
            return -1;
        }
    }
}

int ROAnimation::_get_prev_frame_idx()
{
   return _prev_frame;
}

bool ROAnimation::_current_frame_is_on_edge()
{
    if ((_current_frame > 0) && (_current_frame < (_num_frames - 1)) ){
        return false;
    }
    return true;
}