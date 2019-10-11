#include "Animation.h"
#include "FreeStack.h"
#include "csv_helpers.h"

File sdFile;
//Constructor
Frame::Frame(uint32_t *picture, bool alloced_picture, uint8_t *duty_cycle, bool alloced_duty, int cols, int rows)
{
    _cols = cols;
    _rows = rows;
    if (duty_cycle == nullptr)
    {
        int tolerance = 10000;
        if((FreeStack() < (_cols*_rows + tolerance))){
            Serial.printf("Failed to initialize frame with duty_cycle array of size: %d\n"
            "Available space in RAM: %d\n", _cols*_rows, FreeStack());
            return;
        }
        //Serial.printf("Initializing frame with duty_cycle array of size: %d\n"
        //   "Available space in RAM: %d\n", _cols*_rows, FreeStack());

        _malloced_duty = true;
        _duty_cycle = new uint8_t [cols*rows];
        for (int row = 0; row < rows; row++)
        {
            //_duty_cycle[col] = new uint8_t[rows];
            for (int col = 0; col < cols; col++)
            {
                _duty_cycle[row*cols + col] = (uint8_t)DUTY_CYCLE_RESOLUTION;
            }
            Serial.println();
        }
    }else
    {
        //Serial.printf("Starting to fill duty cycle from %s array at: %p\n", alloced_duty?"dynamically allocated":"static",duty_cycle);
        _malloced_duty = alloced_duty;
        _duty_cycle = duty_cycle;
        for (int y = 0; y < rows; y++)
        {
            for (int x = 0; x < cols; x++)
            {
                if (duty_cycle[y*cols + x] < DUTY_CYCLE_RESOLUTION)
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
        _malloced_picture = true;
    }
    else
    {
        _picture = picture;
        _malloced_picture = alloced_picture;
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
    if(_malloced_duty)
        _delete_duty_cycle();
    if(_malloced_picture)
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
uint8_t *Frame::get_duty_cycle()
{
    return _duty_cycle;
}
uint8_t Frame::get_duty_cycle_at(int x, int y)
{
    return _duty_cycle[y*_cols + x];
}

void Frame::overwrite_duty_cycle(uint8_t *duty_cycle,bool alloced_duty)
{
    _delete_duty_cycle();
    _duty_cycle = duty_cycle;
    _malloced_duty = alloced_duty;
}

void Frame::write_duty_cycle_at(int x, int y, uint8_t duty_cycle)
{
    if (duty_cycle > DUTY_CYCLE_RESOLUTION)
    {
        duty_cycle = DUTY_CYCLE_RESOLUTION;
    }

    if ((duty_cycle == DUTY_CYCLE_RESOLUTION) && (_duty_cycle[x + y*_cols] != DUTY_CYCLE_RESOLUTION))
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
    else if ((duty_cycle < DUTY_CYCLE_RESOLUTION) && (duty_cycle > 0) && (_duty_cycle[x + y*_cols] == DUTY_CYCLE_RESOLUTION))
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
    _duty_cycle[x + y*_cols] = duty_cycle;
}
void Frame::write_picture(uint32_t *picture, bool alloced_picture)
{
    _delete_picture();
    _picture = picture;
    _malloced_picture = alloced_picture;
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
    if(_malloced_picture){
        delete _picture;
        _malloced_picture = false;
    }
}

inline void Frame::_delete_duty_cycle()
{
    if (_malloced_duty){
        delete _duty_cycle;
        _malloced_duty = false;
    }
}

//Constructor
Animation::Animation(Frame **frames, int num_frames, bool alloced_frames, bool alloced_frame_array, int cols, int rows)
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
        _malloced_frames = true;
        _malloced_frame_array = true;
    }
    else
    {    
        _frames = frames;
        _malloced_frames = alloced_frames;
        _malloced_frame_array = alloced_frame_array;
    }
}

// Public Methods
void Animation::delete_anim(void)
{
    if(_malloced_frames){
        for (int frame = 0; frame < _num_frames; frame++)
        {
            _frames[frame]->delete_frame();
        }
    }
    if(_malloced_frame_array)
        delete _frames;
    if(_frame_buf != nullptr)
        delete _frame_buf;
    if (_duty_buf != nullptr)
        delete _duty_buf;
}

Frame *Animation::get_frame(int frame_num)
{
    return _frames[frame_num];
}
void Animation::write_frame(int frame_num, Frame *frame)
{
    _frames[frame_num] = frame;
}
void Animation::load_frames_from_array(uint32_t **animation, bool alloced_animation, uint8_t **duty_cycle, bool alloced_duty)
{
    //TODO: This method has not been properly adjusted to new variant of duty_cycle storage
    for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->write_picture(animation[frame],alloced_animation);
        if (duty_cycle != nullptr)
        {
            _frames[frame]->overwrite_duty_cycle(duty_cycle[frame],alloced_duty);
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

void Animation::start_animation_at(int start_frame){
    if(start_frame == -1){
        start_frame = _num_frames - 1;
    }
    _playback_state = RUNNING;
    _start_idx = start_frame;
    _current_frame = start_frame;
    _loop_iteration = 0;
}
void Animation::write_playback_dir(bool forward){
    _dir_fwd = forward;
}
void Animation::write_max_loop_count(int n){
    _max_iterations = n;
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

/*\brief Saves two corresponding files to the SD card.
    One that provides info about the animation settings, the other who contains the actual data.
    filename format: "A000_C.txt" for config files
    filename format: "A000_D.bin" for data files
    
    \param[in] file_index is a maximum 5 digit number that will be added to the filename. 
        If the number is not unique, the previous file (with the same index) will be overwritten.
 */
int Animation::save_to_SD_card(SdFatSdioEX sd, uint16_t file_index)
{
    if(!sd.begin()){
        Serial.println("SD initialitization failed. Save unsucessful.");
        return -1;
    }
    //Write ASCII config file:
    //(Using ASCII here because we as users are more likely to 
    // change these parameters manually than the animation itself.
    // The animation will mainly be updated through a GUI program)
    //filename format: "A000_C.txt" for config files
    //filename format: "A000_D.bin" for data files
    const int cfg_len = 150;
    const int cols = _cols;
    const int rows = _rows;
    const int frames = _num_frames;
    static char config_str[cfg_len]; //The data that will be written to the .txt file
    uint32_t frame_buf[frames*cols];
    uint8_t duty_buf[frames*cols*rows];

    char full_filename[11]; //Max length of filename is 8 chars +".ext"
    sprintf(full_filename,"A%u_C.txt",file_index);
    if (!sdFile.open(full_filename, O_RDWR | O_CREAT)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }
    sprintf(config_str,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        _cols,
        _rows,
        _num_frames,
        _playback_type,
        _playback_state,
        _dir_fwd ? 1:0,
        _current_frame,
        _prev_frame,
        _loop_iteration,
        _max_iterations,
        _start_idx);
    sdFile.write(config_str);
    //sdFile.write(transport_animation, transport_anim_count*COLS*4);
    sdFile.flush();
    sdFile.close();
    Serial.printf("Config-file saved to SD card as: '%s'.\n",full_filename);

    //Write binary datafile:
    sprintf(full_filename,"A%u_D.bin",file_index);

    if (!sdFile.open(full_filename, O_RDWR | O_CREAT)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }
    //Fill buffers:
    for (int f = 0; f < frames; f++)
    {
        Frame *curr_f = get_frame(f);
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                duty_buf[(f*cols*rows)+(r*cols)+c] = curr_f->get_duty_cycle_at(c,r);
                if(r==0)
                    frame_buf[(f*cols)+c] = curr_f->get_picture_at(c);            
            }
        }
    }
    
    uint8_t* framebuf8 = (uint8_t*) frame_buf;
    sdFile.write(framebuf8,frames*cols*sizeof(uint32_t)); //*4 because the buffer is uint32_t
    sdFile.write(duty_buf,frames*cols*rows);
    sdFile.flush();

    /*
    Serial.printf("Duty buffer before save:\n");
    for(int f = 0; f<frames;f++){
        for (int r = 0; r < rows; r++)
        {
            Serial.printf("%8lp, [frame:%2d,row:%2d]:",&duty_buf[(f*cols*rows)+(r*cols)], f,r);
            for (int c = 0; c < cols; c++)
            {
                Serial.printf("0x%2x,", duty_buf[(f*cols*rows)+(r*cols)+c]);
            }
            Serial.println();
        }
    }
    */

    sdFile.close();
    Serial.printf("Data-file saved to SD card as: '%s'.\n",full_filename);
    
    Serial.println("Save sucessful.");
    return 1;
}

int Animation::read_from_SD_card(SdFatSdioEX sd, uint16_t file_index){
    /*if(!sd.begin()){
        Serial.println("SD initialitization failed. Read unsucessful.");
        return -1;
    }*/
    //Serial.printf("numframes:%d\n",_num_frames);
    char full_filename[11]; //Max length of filename is 8 chars +".ext"
    sprintf(full_filename, "A%u_C.txt", file_index);
    
    /*
    Serial.println("Test");
    if(!sdFile.exists(full_filename)){
        Serial.printf("File: '%s' does not exist\n",full_filename);
        return -1;
    }
    Serial.println("Test2");
    */
   
    //Clear memory of old frames
    if (_malloced_frames)
    {
        for (int i = 0; i < _num_frames; i++)
        {
            //Serial.printf("i:%d\n",i);
            _frames[i]->delete_frame();
        }
    }
    if (_malloced_frame_array)
    {
        delete _frames;
    }
    
    
    //Read ASCII config file:
    //(Using ASCII here because we as users are more likely to 
    // change these parameters manually than the animation itself.
    // The animation will mainly be updated through a GUI program)
    //filename format: "A000_C.txt" for config files
    //filename format: "A000_D.bin" for data files


    if (!sdFile.open(full_filename, O_RDONLY)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }

    
    char delim = ',';
    csvReadInt(&sdFile,&_cols,delim);
    csvReadInt(&sdFile,&_rows,delim);
    csvReadInt(&sdFile,&_num_frames,delim);
    csvReadPBType(&sdFile,&_playback_type,delim);
    csvReadPBState(&sdFile,&_playback_state,delim);
    csvReadBool(&sdFile,&_dir_fwd,delim);
    csvReadInt(&sdFile,&_current_frame,delim);
    csvReadInt(&sdFile,&_prev_frame,delim);
    csvReadInt(&sdFile,&_loop_iteration,delim);
    csvReadInt(&sdFile,&_max_iterations,delim);
    csvReadInt(&sdFile,&_start_idx,delim);
    
    sdFile.close();
    Serial.printf("Config-file read from SD card: '%s'.\n",full_filename);

    //Read binary datafile:
    int tolerance = 10000;
    if(!(FreeStack() > (int)(_num_frames*_cols*sizeof(uint32_t) + _num_frames*_cols*_rows + tolerance))){
        Serial.printf("Not enough memory to store animation of size: %d\n"
        "Available space in RAM: %d", _num_frames*_cols*4 + _num_frames*_cols*_rows, FreeStack());
        return -2;
    }
    const int cols = _cols;
    const int rows = _rows;
    const int frames = _num_frames;
    uint8_t* frame_buf8 = new uint8_t[frames*cols*sizeof(uint32_t)];
    if (frame_buf8 == 0)
    {
        Serial.printf("Could not allocate memory for frames of size: %d\n"
        "Available space in RAM: %d",frames*cols*sizeof(uint32_t), FreeStack());
    }
    
    _frame_buf = (uint32_t*) frame_buf8;
    
    _duty_buf = new uint8_t[frames*cols*rows*sizeof(uint8_t)];
    if (&_duty_buf[0] == 0 || _duty_buf == 0 || _duty_buf == (uint8_t*) nullptr || &_duty_buf[0] == (uint8_t*) nullptr || _duty_buf == nullptr || &_duty_buf[0] == nullptr || _duty_buf == NULL || &_duty_buf[0] == NULL ||&_duty_buf[0] ==(uint8_t *) 0 || _duty_buf == (uint8_t *)0 )
    {
        Serial.printf("Could not allocate memory for duty_cycle arr of size: %d\n"
        "Available space in RAM: %d\n",frames*cols*rows, FreeStack());

        Serial.printf("Pointer to _duty_buf: %8p,%d\n",_duty_buf,_duty_buf);
        Serial.printf("Pointer to _frame_buf: %p\n",_frame_buf);
        return-1;
    }else{
        //Serial.printf("Pointer to _duty_buf: %p,%d\n",_duty_buf,_duty_buf);
        //Serial.printf("Pointer to _frame_buf: %p\n",_frame_buf);
    }
    sprintf(full_filename,"A%u_D.bin",file_index);
    if (!sdFile.open(full_filename, O_RDONLY)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }
    
    sdFile.read(frame_buf8,frames*cols*sizeof(uint32_t));
    sdFile.read(_duty_buf,frames*cols*rows);
    

    /*Serial.printf("Frame buffer after read:\n");
    for(int f = 0; f<frames;f++){
        for (int c = 0; c < cols; c++)
        {
            Serial.printf("[frame:%2d,col:%2d]:0x%8lx\n",f, c, _frame_buf[(f*cols)+c]);
        }
        
    }*/
    /*
    Serial.printf("Duty buffer after read:\n");
    for(int f = 0; f<frames;f++){
        for (int r = 0; r < rows; r++){
            Serial.printf("%8lp, [frame:%2d,row:%2d]:",&_duty_buf[(f*cols*rows)+(r*cols)], f,r);
            for (int c = 0; c < cols; c++)
            {
                Serial.printf("0x%2x,", _duty_buf[(f*cols*rows)+(r*cols)+c]);
            }
            Serial.println();
        }
    }
    */

    _frames = new Frame *[frames];
    Serial.printf("frames:%d,cols:%d,rows:%d\n",frames,cols,rows);
    for (int frame = 0; frame < frames; frame++)
    {
        //The arrays sent to the frame constructor are marked as NOT dynamically allocated (even though they are)
        //This is because the "delete" will be handled by the animation object, and should therefore not be performed by the frame object.
        _frames[frame] = new Frame(&_frame_buf[frame*cols],false,&_duty_buf[frame*cols*rows],false); 
    }
    _malloced_frame_array = true;
    _malloced_frames = true;
    
    /* The below was needed when duty_cycle was stored differently in this method and in the frame object.
    Therefore it should now be irrelevant because we have changed how the Frame object stores duty cycles
    //Fill buffers:
    for (int f = 0; f < frames; f++)
    {
        Frame *curr_f = get_frame(f);
        for (int c = 0; c < cols; c++)
        {
            for (int r = 0; r < rows; r++)
            {
                //TODO: Write a method for Frame that can reuse this buffer instead of copying all values.
                Serial.printf("x:%d,y:%d,val:%d\n",c,r,_duty_buf[f*cols*rows + c*rows + r]);
                curr_f->write_duty_cycle_at(c,r,_duty_buf[f*cols*rows + c*rows + r]);
            }          
        }
    }*/

    sdFile.flush();
    sdFile.close();
    Serial.printf("Data-file: '%s' read from SD card.\n",full_filename);
    
        

    Serial.println("Read sucessful.");
    return 1;
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
