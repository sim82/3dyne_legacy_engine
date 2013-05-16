
#ifndef __pan_h
#define __pan_h

#include <X11/Xlib.h>
#include <GL/glx.h>
#include <memory>
#include "message_passing.h"

namespace pan {

    
// class x11_display {
// public:
//     
//     x11_display( bool open = false ) 
//     {
//         if( open ) {
//             display_ = XOpenDisplay(NULL);
//         } else {
//             display_ = 0;
//         }
//     }
//     
//     ~x11_display() {
//         if( display_ != 0 ) {
//             XCloseDisplay( display_ );
//         }
//         
//         display_ = 0;
//         
//     }
//     
//     x11_display( const x11_display & ) = delete;
//     x11_display &operator=(const x11_display &) = delete;
//     
//     x11_display( x11_display && other ) {
//         display_ = other.display_;
//         other.display_ = 0;
//     }
//     
//     x11_display &operator=(x11_display && other) {
//         display_ = other.display_;
//         other.display_ = 0;
//         
//         return *this;
//     }
//     
//     
//     
//     Display *display_;
//     
// };
class xrandr_mode_setter;
class gl_context {
public:
    struct config {
        
        config() : width_(0), height_(0), fullscreen_(false) {}
        
        size_t width_;
        size_t height_;
        
        bool fullscreen_;
    };
    
    gl_context();
    gl_context( const config &cfg );
    ~gl_context();
    
    void set_config( const config &cfg );
    
    void release_resources();
    
    void dispatch_input( mp::queue &q );
    void swap_buffers();
    
    void grab_mouse( bool grab );
private:
    
    void dispatch_key( mp::queue &q, const XKeyEvent &event );
    
    Window  window_;
    Display *display_;
    int     scrnum_; 
    
    GLXContext glx_ctx_;
    
    config  cfg_;
    bool initialized_;
    bool have_xinput_;
    
    bool mouse_grabbed_;
    
    
    // non-XInput tracking of last position for relative motion calculation
    bool ptr_last_valid_;
    int ptr_last_x_;
    int ptr_last_y_;
    
    
    
    
    // Xinput2 stuff
    int xi_opcode_, xi_event_, xi_error_;
    
    struct xi_warp_state {
      xi_warp_state() 
      : need_warp_(true), acc_x_(0.0), acc_y_(0.0) {}
       
      bool need_warp_;
      double acc_x_;
      double acc_y_;
    };
    
    xi_warp_state xi_warp_;
    
    std::unique_ptr<xrandr_mode_setter> mode_setter_;
};
    
    
    
};


#endif