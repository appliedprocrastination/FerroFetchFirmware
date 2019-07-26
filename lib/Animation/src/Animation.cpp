#include "Animation.h"

uint32_t empty_picture[COLS] = {0};

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
    _blank_frame = Frame(empty_picture);
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
void Animation::write_frame(int frame_num, Frame *frame)
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
void Animation::goto_prev_frame()
{
    //This whole function may be unnecessary, since going back and forth is
    //handled in the goto_next_frame and get_next_frame_idx methods
    int tmp_prev_frame = _current_frame;
    _current_frame = _prev_frame; //TODO: use private method _get_prev_frame_idx()?
    _prev_frame = tmp_prev_frame;
    
}
Frame *Animation::get_current_frame(){
    if (_current_frame == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    return _frames[_current_frame];
}
Frame *Animation::get_next_frame(){
    int idx = _get_next_frame_idx();
    if (idx == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    return _frames[idx];
}
Frame *Animation::get_prev_frame(){
    if (_prev_frame == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    
    return _frames[_prev_frame];
}

void Animation::start_animation(int start_frame){
    
    _playback_state = RUNNING;
    _start_idx = start_frame;
    _current_frame = start_frame;
    _loop_iteration = 0;
}
void Animation::write_playback_dir(bool forward){
    _dir_fwd = forward;
}
bool Animation::get_playback_dir(){
    return _dir_fwd;
}

bool Animation::anim_done(){
    if(_playback_state == IDLE){
        return true;
    }
    return false;
}
void Animation::write_playback_type(PlaybackType type){
    _playback_type = type;
}
PlaybackType Animation::get_playback_type(){
    return _playback_type;
}

// Private Methods

int Animation::_get_next_frame_idx()
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

int Animation::_get_prev_frame_idx()
{
   return _prev_frame;
}

bool Animation::_current_frame_is_on_edge(){
    if ((_current_frame > 0) && (_current_frame < (_num_frames - 1)) ){
        return false;
    }
    return true;
}