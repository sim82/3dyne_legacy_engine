
#ifndef __pan_h
#define __pan_h


#include "compiler_config.h"
#include <GL/gl.h>

#if !D3DYNE_OS_WIN
#include <X11/Xlib.h>
#include <GL/glx.h>
#else
#include <GL/glext.h>
struct SDL_Surface;
#endif

#include <memory>
#include "message_passing.h"





namespace pan {
#if !D3DYNE_OS_WIN
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

#else


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

    config  cfg_;

    SDL_Surface *sdl_surf_display;

    bool initialized_;
    bool have_xinput_;

    bool mouse_grabbed_;



};
#endif
}
#endif
