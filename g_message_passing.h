#include "message_passing.h"
#include "i_defs.h"

namespace msg {
class key_event : public base {
public:
    key_event( const keyevent_t &event ) : event_(event) {}    
    keyevent_t event_;
};
    
class mouse_event : public base {
public:
    mouse_event( int md_x, int md_y, int ts ) : md_x_(md_x), md_y_(md_y), ts_(ts) {}
    
    int md_x_;
    int md_y_;
    
    int ts_;
    
};

class gc_quit : public base {};
class gc_start_single : public base {};
class gc_drop_game : public base {};
class gc_start_demo : public base {};


class client_frame : public base {};
class swap_buffer : public base {};

}

class g_global_mp {
public:
    
    
    mp::queue &get_queue() {
        return queue_;   
    }
    
    
    static g_global_mp *get_instance();
    
private:
    mp::queue queue_;
    
    
};