#include "X11/extensions/XInput2.h"
#include "X11/extensions/XInput.h"
#include <X11/cursorfont.h>
#include <cstring>

#include "g_message_passing.h"
#include "pan.h"
#include "i_defs.h"
#include "log.h"
#include "shared/shock.h"

namespace pan {
    
gl_context::gl_context() : initialized_(false), ptr_last_valid_(false), mouse_grabbed_(false) {}
    
gl_context::gl_context(const pan::gl_context::config& cfg)  
 : initialized_(false) 
 , ptr_last_valid_(false)
 , mouse_grabbed_(false)
{
    set_config( cfg );
    
    
}




void gl_context::set_config(const gl_context::config& cfg) {
    
    bool stop_context = false;
    bool start_context = false;
    
    bool stop_window = false;
    bool start_window = false;
    
    
    
    if( !initialized_ ) {
        start_context = true;
        start_window = true;
    } else if( cfg_.fullscreen_ != cfg.fullscreen_ ) {
        stop_context = true;
        stop_window = true;
        
        start_context = true;
        start_window = true;
    }  else if( cfg_.width_ != cfg.width_ || cfg_.height_ != cfg.height_ ) {
        stop_context = true;
        stop_window = true;
        
        start_context = true;
        start_window = true;
    }
    
    
    if( stop_context ) {
        glXDestroyContext( display_, glx_ctx_ );
    }

    if( stop_window ) {
        XDestroyWindow( display_, window_ );
        XCloseDisplay( display_ );
    }
    
    
    XVisualInfo *visinfo = 0;
    
    if( start_window ) {
        display_ = XOpenDisplay( NULL );
        scrnum_ = DefaultScreen( display_ );
        
        int maxAttribs = 128;
        int attrib[maxAttribs];
        int idx = 0;
        
        
        attrib[idx++] = GLX_RGBA;
        attrib[idx++] = GLX_RED_SIZE;
        attrib[idx++] = 8;
        attrib[idx++] = GLX_GREEN_SIZE;
        attrib[idx++] = 8;
        attrib[idx++] = GLX_BLUE_SIZE;
        attrib[idx++] = 8;
        attrib[idx++] = GLX_ALPHA_SIZE;
        attrib[idx++] = 8;
        attrib[idx++] = GLX_DEPTH_SIZE;
        attrib[idx++] = 24;
        attrib[idx++] = GLX_STENCIL_SIZE;
        attrib[idx++] = 8;
        attrib[idx++] = GLX_DOUBLEBUFFER;
        
        //         // add GLX_ARB_multisample
        //         if ( m_multisamplingAvailable && newConfig->m_multisamplingEnabled ) {
        //             attrib[idx++] = GLX_SAMPLE_BUFFERS_ARB;
        //             attrib[idx++] = 1;
        //             attrib[idx++] = GLX_SAMPLES_ARB;
        //             attrib[idx++] = newConfig->m_multisamplingSamples;
        //             printf( " request %d GLX_SAMPLES_ARB\n", newConfig->m_multisamplingSamples );
        //         }
        
        attrib[idx++] = None;
        //          attrib[idx++] =
        //          attrib[idx++] =
        visinfo = glXChooseVisual( display_, scrnum_, attrib );
        
        Window root;
        root = RootWindow( display_, scrnum_ );
        
        XSetWindowAttributes attr;
        attr.background_pixel = 0;
        attr.border_pixel = 0;
        attr.colormap = XCreateColormap( display_, root, visinfo->visual, AllocNone );
        attr.event_mask =  StructureNotifyMask | ExposureMask | KeyPressMask | FocusChangeMask;
        unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
        
        
        window_ = XCreateWindow( display_, root, 0, 0, cfg.width_, cfg.height_, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr );

        

        
        have_xinput_ = true;
        
        if ( have_xinput_ && !XQueryExtension(display_, "XInputExtension", &xi_opcode_, &xi_event_, &xi_error_)) {
            __warning("X Input extension not available.\n");
            have_xinput_ = false;
            
        } 
        /* Which version of XI2? We support 2.0 */
        int major = 2, minor = 0;
        if ( have_xinput_ && XIQueryVersion(display_, &major, &minor) == BadRequest) {
            __warning("XI2 not available. Server supports %d.%d. XInput disabled!\n", major, minor);
            
            have_xinput_ = false;
            
        }
        
        if( have_xinput_ )
        {
            XIEventMask mask;

        
            //XSelectInput(display_, window_, ExposureMask);

            mask.deviceid = XIAllMasterDevices;
            mask.mask_len = XIMaskLen(XI_RawMotion);
            mask.mask = (unsigned char*)calloc(mask.mask_len, sizeof(char));
            XISetMask(mask.mask, XI_Enter);
            XISetMask(mask.mask, XI_Leave);
//             XISetMask(mask.mask, XI_ButtonPress);
//             XISetMask(mask.mask, XI_ButtonRelease);
//             XISetMask(mask.mask, XI_KeyPress);
//             XISetMask(mask.mask, XI_KeyRelease);
            
            XISelectEvents(display_, window_, &mask, 1);
            
            mask.deviceid = XIAllDevices;
            memset(mask.mask, 0, mask.mask_len);
            XISetMask(mask.mask, XI_RawMotion);
            
            XISelectEvents(display_, DefaultRootWindow(display_), &mask, 1);
            
            free(mask.mask);
            XSelectInput( display_, window_, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask );    
#if 0            
            int mouse_device_id = -1;
            
            int ndevices;
            XIDeviceInfo *devices, device;
            
            devices = XIQueryDevice(display_, XIAllDevices, &ndevices);
            
            for ( int i = 0; i < ndevices; i++) {
                device = devices[i];
                printf("Device %s (id: %d) is a ", device.name, device.deviceid);
                
                switch(device.use) {
                case XIMasterPointer: 
                {
                    printf("master pointer\n"); 
                    mouse_device_id = device.deviceid;
                    
                    
                    break;
                }
//                     case XIMasterKeyboard: printf("master keyboard\n"); break;
//                     case XISlavePointer: printf("slave pointer\n"); break;
//                     case XISlaveKeyboard: printf("slave keyboard\n"); break;
//                     case XIFloatingSlave: printf("floating slave\n"); break;
                }
            }

//             printf("Device is attached to/paired with %d\n", device.attachement);
            XIFreeDeviceInfo(devices);
            
            
            if( mouse_device_id == -1 ) {
                __error( "mouse device not found\n" );   
            }
#endif
            
//             Cursor watch = XCreateFontCursor (display_, XC_watch);
//             XIGrabDevice(display_, mouse_device_id, window_, CurrentTime,
//                          watch , XIGrabModeAsync, XIGrabModeAsync,
//                          True, &eventmask);
        } else {
            XSelectInput( display_, window_, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask );
            
        }
        
        
        
        XMapWindow( display_, window_ );
        
    }
    
    if( start_context ) {
        glx_ctx_ = glXCreateContext( display_, visinfo, 0, True );
        glXMakeCurrent( display_, window_, glx_ctx_ );
    }
     
      
    XFree( visinfo );
      
    
    cfg_ = cfg;
    initialized_ = true;
}


void gl_context::dispatch_input(mp::queue& q) {
    
    if( !initialized_ ) {
        return;   
    }
    
    
    
    while( XEventsQueued( display_, QueuedAfterFlush )) {
        XEvent  event;
        XNextEvent( display_, &event );

        if (event.xcookie.type == GenericEvent &&
            event.xcookie.extension == xi_opcode_ &&
            XGetEventData(display_, &event.xcookie))
        
        {
            
        
            switch(event.xcookie.evtype)
            {
                //case XI_ButtonPress:
                
                case XI_RawMotion:
                {
//                     print_rawmotion( (XIRawEvent *)event.xcookie.data );
                    
                    XIRawEvent *raw_event = (XIRawEvent *)event.xcookie.data;
                    
                    double *raw_valuator = raw_event->raw_values,
                    *valuator = raw_event->valuators.values;
           
           

//                     printf("    device: %d (%d)\n", event->deviceid, event->sourceid);
                    
                    const size_t num_valuators = 64;
                    double raw_values[num_valuators];
                    double values[num_valuators];
                    
                    assert( raw_event->valuators.mask_len * 8 <= num_valuators );
                    
                    for (int i = 0; i < raw_event->valuators.mask_len * 8; i++)
                    {
                        if (XIMaskIsSet(raw_event->valuators.mask, i))
                        {
                            //raw_values[i] = *raw_valuator;
                            
                            raw_values[i] = *raw_valuator;
                            values[i] = *valuator;
                            
                            valuator++;
                            raw_valuator++;
                        }
                    }
                    q.emplace<msg::mouse_event>( raw_values[0], raw_values[1], 0 );
                    
                    xi_warp_.acc_x_ += values[0];
                    xi_warp_.acc_y_ += values[1];
                    
                    double half_width = cfg_.width_ / 2;
                    double half_height = cfg_.height_ / 2;
                    
                    
                    
                    if( mouse_grabbed_ 
                        && (xi_warp_.need_warp_ || fabs( xi_warp_.acc_x_ ) + 50 > half_width || fabs( xi_warp_.acc_y_ ) + 50 > half_height) ) {
                        
                        xi_warp_.acc_x_ = 0.0;
                        xi_warp_.acc_y_ = 0.0;
                        
                        xi_warp_.need_warp_ = false;
                        XWarpPointer( display_, None, window_, 0, 0, 0, 0, half_width, half_height );
                    }
                    
                    break;
                }
//                 case XI_Motion:
//                     XIDeviceEvent *dev = (XIDeviceEvent *)event.xcookie.data;
//                     DD_LOG << "xi2 motion: " << dev->event_x << " " <<  dev->event_y << "\n";
//                     int delta_x = 0;
//                     int delta_y = 0;
//                     
//                     if( ptr_last_valid_ ) {
//                         delta_x += dev->event_x - ptr_last_x_;
//                         delta_y += dev->event_y - ptr_last_y_;
//                     }
//                     ptr_last_x_ = dev->event_x;
//                     ptr_last_y_ = dev->event_y;
//                     ptr_last_valid_ = true;
//                     q.emplace<msg::mouse_event>( delta_x, delta_y, 0 );
//                     //case XI_KeyPress:
//                     //do_something(ev.xcookie.data);
//                     
//                     
//                     
//                     break;
            }
            XFreeEventData(display_, &event.xcookie);
            
            continue;
        }
    
    
        switch( event.type ) {
            // =============================================
            // keyboard events
            // =============================================

        case KeyPress:
        case KeyRelease:
            dispatch_key( q, event.xkey );
            break;

            // =============================================
            // ptr move events
            // =============================================

        case MotionNotify:
        {
            int delta_x = 0;
            int delta_y = 0;
            
            if( ptr_last_valid_ ) {
                delta_x += event.xmotion.x - ptr_last_x_;
                delta_y += event.xmotion.y - ptr_last_y_;
            }
            ptr_last_x_ = event.xmotion.x;
            ptr_last_y_ = event.xmotion.y;
            ptr_last_valid_ = true;
            q.emplace<msg::mouse_event>( delta_x, delta_y, 0 );
            
            break;
        }   
            
         
// 
//             case PtrModeWarp:
//                 getMouseDeltaWarp( &event.xmotion, deltaX, deltaY, &lastX, &lastY );
//                 break;
// 
//             default: // PtrModeNone goes here, too
//                 break;
//             }
// 
// 
//             break;
// 
//             // =============================================
//             // button events
//             // =============================================
//         case ButtonPress:
//         case ButtonRelease:
//             i = insertButton( f, i, maxF, &event.xbutton );
//             break;

//              case FocusOut:
//                  StdPrintf( "===============================================================\n" );
//                  bLostFocus = true;
//                  break;
        
        default:
        {}
            
        }

    }


}
void gl_context::swap_buffers() {
    glXSwapBuffers( display_, window_ );
}
void gl_context::release_resources() {

}


void gl_context::dispatch_key( mp::queue &q, const XKeyEvent &event ) {
    
    bool press = (event.type == KeyPress); // ? SYMTYPE_PRESS : SYMTYPE_RELEASE);

    typedef unsigned int i_ksym_t;
    
    KeySym xkeysym = XKeycodeToKeysym( display_, event.keycode, 0 );


    unsigned int ksym = 0;

    //  printf( "keysym = %d\n", keysym );
    if( ( xkeysym >= 0x020 ) && ( xkeysym <= 0x07f ))  // west orientation!
    {
        // keysym == ASCII code

        ksym = (unsigned int)(xkeysym);
        goto abs_end;
    }

    // do abstraction from X11 keysym to ksym

    switch( xkeysym )
    {
        // ascii control codes 0x0 to 0x1F
    case XK_BackSpace:
        ksym = (i_ksym_t)'\b';
        goto abs_end;

    case XK_Tab:
        ksym = (i_ksym_t)'\t';
        goto abs_end;

    case XK_Return:
        ksym = (i_ksym_t)'\n';
        goto abs_end;

        // cursor keys
    case XK_Up:
    case XK_KP_Up:
        ksym = GSK_CUP;
        goto abs_end;

    case XK_Down:
    case XK_KP_Down:
        ksym = GSK_CDOWN;
        goto abs_end;

    case XK_Right:
    case XK_KP_Right:
        ksym = GSK_CRIGHT;
        goto abs_end;

    case XK_Left:
    case XK_KP_Left:
        ksym = GSK_CLEFT;
        goto abs_end;


        // modifier keys
    case XK_Shift_L:
    case XK_Shift_R:
        ksym = GSK_SHIFT;
#if 0
        if( !isshift )
            isshift = 1;
        else
            isshift = 0;
        SHV_Printf( "isshift" );
#endif
        goto abs_end;

    case XK_Control_L:
    case XK_Control_R:
    case XK_Multi_key:
        ksym = GSK_CTRL;
        goto abs_end;

    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Meta_L:
    case XK_Meta_R:
        ksym = GSK_ALT;
        goto abs_end;


    case XK_Super_L:
    case XK_Super_R:
        ksym = GSK_FUCK0;
        goto abs_end;

    case XK_Hyper_L:
    case XK_Hyper_R:
        ksym = GSK_FUCK1;
        goto abs_end;
        // other cursor conrol keys

    case XK_Insert:
    case XK_KP_Insert:
        ksym = GSK_INSERT;
        goto abs_end;

    case XK_Home:
    case XK_KP_Home:
        ksym = GSK_HOME;
        goto abs_end;

    case XK_End:
    case XK_KP_End:
        ksym = GSK_END;
        goto abs_end;

    case XK_Page_Up:
    case XK_KP_Page_Up:
        ksym = GSK_PGUP;
        goto abs_end;

    case XK_Page_Down:
    case XK_KP_Page_Down:
        ksym = GSK_PGDN;
        goto abs_end;

    case XK_Escape:
        ksym = GSK_ESCAPE;
        goto abs_end;

        // function keys

    case XK_F1:
        ksym = GSK_F1;
        goto abs_end;


    case XK_F2:
        ksym = GSK_F2;
        goto abs_end;

    case XK_F3:
        ksym = GSK_F3;
        goto abs_end;

    case XK_F4:
        ksym = GSK_F4;
        goto abs_end;

    case XK_F5:
        ksym = GSK_F5;
        goto abs_end;

    case XK_F6:
        ksym = GSK_F6;
        goto abs_end;

    case XK_F7:
        ksym = GSK_F7;
        goto abs_end;

    case XK_F8:
        ksym = GSK_F8;
        goto abs_end;

    case XK_F9:
        ksym = GSK_F9;
        goto abs_end;

    case XK_F10:
        ksym = GSK_F10;
        goto abs_end;

    case XK_F11:
        ksym = GSK_F11;
        goto abs_end;

    case XK_F12:
        ksym = GSK_F12;
        goto abs_end;

    default:
        goto abs_end;

    }
    
abs_end:
    
    
    keyevent_t kevent;
    kevent.sym = gs_ksym(ksym);
    kevent.type = press ? SYMTYPE_PRESS : SYMTYPE_RELEASE;
    q.emplace<msg::key_event>( kevent );

}

void gl_context::grab_mouse(bool grab) {
    if( grab == mouse_grabbed_ ) {
        return;
    }

    mouse_grabbed_ = grab;
    
    if( mouse_grabbed_ ) {
        xi_warp_.need_warp_ = true;
    }
    
}


}



