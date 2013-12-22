#include "compiler_config.h"
#if !D3DYNE_OS_WIN

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
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
 , mouse_grabbed_(false)
 , ptr_last_valid_(false)
{
    set_config( cfg );
    
    
}

static double mode_refresh (XRRModeInfo *mode_info)
{
    unsigned int vTotal = mode_info->vTotal;

    if (mode_info->modeFlags & RR_DoubleScan) {
        /* doublescan doubles the number of lines */
        vTotal *= 2;
    }

    if (mode_info->modeFlags & RR_Interlace) {
        /* interlace splits the frame into two fields */
        /* the field rate is what is typically reported by monitors */
        vTotal /= 2;
    }

    if (mode_info->hTotal && vTotal)
        return ((double) mode_info->dotClock / ((double) mode_info->hTotal * (double) vTotal));
    else {
        return 0;
    }
    
}
#if 1
class xrandr_mode_setter {
public:
    
    class server_grab {
        Display *d_;
        
    public:
        
        
        server_grab() = delete;
        server_grab( const server_grab &) = delete;
        server_grab& operator=( const server_grab &) = delete;
        
        server_grab( Display *d = 0 ) : d_(d) {
            if( d_ != 0 ) {
                XGrabServer( d_ );
            }
        }
        ~server_grab() {
            if( d_ != 0 ) {
                XUngrabServer( d_ );
            }
        }
        
    };
    
    bool delta_compare( double v1, double v2 ) {
        return fabs( v1 - v2 ) < 0.5;
        
    }
    
    xrandr_mode_setter( Display *display, Window root, int width, int height )
    : initial_mode_( None )
    , display_(display), root_(root)
    {
        server_grab grab(display_);
        res = XRRGetScreenResourcesCurrent (display, root);
        RROutput primary_output = XRRGetOutputPrimary(display, DefaultRootWindow(display_));
        info = XRRGetOutputInfo( display, res, primary_output );
        
        crtc_info = XRRGetCrtcInfo( display, res, info->crtc );
        
        initial_mode_ = crtc_info->mode;
        
        RRMode target_mode = None;
        for( int i = 0; i != info->nmode; ++i ) {
            
            RRMode mode = info->modes[i];
            
            for( int j = 0; j < res->nmode; ++j ) {
                if( mode == res->modes[j].id ) {
                    XRRModeInfo *mode_info = res->modes + j;
                    
                    std::cout << mode_info->width << " " << mode_info->height << " " << mode_refresh(mode_info) << ((mode == initial_mode_) ? "*" : "") << "\n";
                    if( int(mode_info->width) == width && int(mode_info->height) == height && delta_compare( mode_refresh(mode_info), 60 )) {
                        std::cout << "xxx\n";
                        target_mode = mode;
                    }
                    
                    
                }
                
            }
            
        }
        
        std::cout << "setting target mode: " << target_mode << "\n";
        
        if( target_mode != None ) {
            XRRSetCrtcConfig( display, res, info->crtc, CurrentTime, crtc_info->x, crtc_info->y, target_mode, crtc_info->rotation, crtc_info->outputs, crtc_info->noutput );
    
            XRRFreeCrtcInfo(crtc_info);
           // XRRCrtcInfo *crtc_info = XRRGetCrtcInfo( display, res, info->crtc );
        } else {
            XRRFreeCrtcInfo(crtc_info);
            initial_mode_ = None;
        }
        
    }
    
    ~xrandr_mode_setter() {
        std::cout << "setting initial mode: " << initial_mode_ << "\n";
        if( initial_mode_ != None ) {
            server_grab grab(display_);
            
            XRRSetCrtcConfig( display_, res, info->crtc, CurrentTime, crtc_info->x, crtc_info->y, initial_mode_, crtc_info->rotation, crtc_info->outputs, crtc_info->noutput );
            XRRFreeCrtcInfo(crtc_info);
        }
        
    }
    RRMode initial_mode_;
    
    Display *display_;
    Window root_;
    
    XRRScreenResources *res;
    XRROutputInfo* info;
    
    XRRCrtcInfo *crtc_info;

    
};
#else
class xrandr_mode_setter {
public:
    
    bool delta_compare( double v1, double v2 ) {
        return fabs( v1 - v2 ) < 0.5;
        
    }
    
    xrandr_mode_setter( Display *display, Window root, int width, int height ) 
    : initial_trans_attr_( 0 )
    , display_(display), root_(root)
    {
        res = XRRGetScreenResourcesCurrent (display, root);
        RROutput primary_output = XRRGetOutputPrimary(display, DefaultRootWindow(display_));
        info = XRRGetOutputInfo( display, res, primary_output );
        
        crtc_info = XRRGetCrtcInfo( display, res, info->crtc );
        
        initial_mode = crtc_info->mode;
        double phys_width = 0;
        double phys_height = 0;
        
        for( int j = 0; j < res->nmode; ++j ) {
            if( initial_mode == res->modes[j].id ) {
                XRRModeInfo *mode_info = res->modes + j;
                
                phys_width = mode_info->width;
                phys_height = mode_info->height;
            }
        }
        
                
        double scale_x = (phys_width / width);
        double scale_y = (phys_height / height);
        XGrabServer( display_ );
        
        XRRGetCrtcTransform( display_, info->crtc, &initial_trans_attr_ );
        
        //initial_trans_ = trans_attr->currentTransform;
        
        
        
        XTransform trans;
        
        for( int i = 0; i < 3; ++i ) {
            //trans.matrix[i][i] = 1.0;
            trans.matrix[0][i] = 0.0;
            trans.matrix[1][i] = 0.0;
            trans.matrix[2][i] = 0.0;
        }
        std::cout << "setting scale: " << scale_x << " " << scale_y << " | " << initial_trans_attr_->currentFilter << " | " << initial_trans_attr_->currentNparams << "\n";
        trans.matrix[0][0] = XDoubleToFixed( scale_x );
        trans.matrix[1][1] = XDoubleToFixed( scale_y );
        trans.matrix[2][2] = 1.0;
        
        XRRSetCrtcTransform( display_, info->crtc, &trans, "nearest", 0, 0 );
        XUngrabServer( display_ );
//         XRRSetCrtcConfig( display_, res, info->crtc, CurrentTime, crtc_info->x, crtc_info->y, initial_mode, crtc_info->rotation, crtc_info->outputs, crtc_info->noutput );
    
//         trans.filter = "bilinear";
//         trans.nparams = 0;
//         trans.params = 0;
        
    }
    
    ~xrandr_mode_setter() {
      
        if( initial_trans_attr_ != 0 ) {
            
            XRRSetCrtcTransform( display_, info->crtc, &initial_trans_attr_->currentTransform, initial_trans_attr_->currentFilter, initial_trans_attr_->currentParams, initial_trans_attr_->currentNparams );
//             XRRSetCrtcConfig( display_, res, info->crtc, CurrentTime, crtc_info->x, crtc_info->y, initial_mode, crtc_info->rotation, crtc_info->outputs, crtc_info->noutput );
            
            //XRRSetCrtcConfig( display_, res, info->crtc, CurrentTime, crtc_info->x, crtc_info->y, initial_mode_, crtc_info->rotation, crtc_info->outputs, crtc_info->noutput );
            XRRFreeCrtcInfo(crtc_info);
        }
        
    }
    
    
    Display *display_;
    Window root_;
    
    XRRScreenResources *res;
    XRROutputInfo* info;
    RRMode initial_mode;
    XRRCrtcInfo *crtc_info;

    XRRCrtcTransformAttributes *initial_trans_attr_;
    
};

#endif

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
        
        
//        mode_setter_.reset( new xrandr_mode_setter(display_, root, cfg.width_, cfg.height_) );
        
        XSetWindowAttributes attr;
        memset( &attr, 0, sizeof( XSetWindowAttributes ));
        attr.background_pixel = 0;
        attr.border_pixel = 0;
        attr.colormap = XCreateColormap( display_, root, visinfo->visual, AllocNone );
        attr.event_mask =  StructureNotifyMask | ExposureMask | KeyPressMask | FocusChangeMask;

        unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
        
        if( cfg.fullscreen_ ) {
            attr.override_redirect = 1;
            mask |= CWOverrideRedirect;
        
            
                    
        }
        
        
        
        {
            XRRScreenResources *res = XRRGetScreenResourcesCurrent (display_, root);
            RROutput primary_output = XRRGetOutputPrimary(display_, DefaultRootWindow(display_));
            XRROutputInfo* info = XRRGetOutputInfo( display_, res, primary_output );
            
            
            
            XRRCrtcInfo *crtc_info = XRRGetCrtcInfo( display_, res, info->crtc );
            
            RRMode current_mode = crtc_info->mode;
            
            
            for( int i = 0; i != info->nmode; ++i ) {
            
                RRMode mode = info->modes[i];
                
                for( int j = 0; j < res->nmode; ++j ) {
                    if( mode == res->modes[j].id ) {
                        XRRModeInfo *mode_info = res->modes + j;
                        
                        std::cout << mode_info->width << " " << mode_info->height << " " << mode_refresh(mode_info) << ((mode == current_mode) ? "*" : "") << "\n";
                        
                        
                        
                    }
                    
                }
                
            }
            
//             int num_sizes;
//             XRRScreenSize* xrrs   = XRRSizes(display_, 0, &num_sizes);
//  
//             for(int i = 0; i < num_sizes; i ++) {
//                 short   *rates;
//                 int     num_rates;
// 
//                 printf("\n\t%2i : %4i x %4i   (%4imm x%4imm ) ", i, xrrs[i].width, xrrs[i].height, xrrs[i].mwidth, xrrs[i].mheight);
// 
//                 rates = XRRRates(display_, 0, i, &num_rates);
//                 
//                 for(int j = 0; j < num_rates; j ++) {
//                     
//                     printf("%4i ", rates[j]); 
//                     
//                 } 
//                 printf( "\n" );
//             
//             }
            
            //     GET CURRENT RESOLUTION AND FREQUENCY
            //
            //                     conf                   = XRRGetScreenInfo(dpy, root);
            //                     original_rate          = XRRConfigCurrentRate(conf);
            //                     original_size_id       = XRRConfigCurrentConfiguration(conf, &original_rotation);
            
            //                     printf("\n\tCURRENT SIZE ID  : %i\n", original_size_id);
            //                     printf("\tCURRENT ROTATION : %i \n", original_rotation);
            //                     printf("\tCURRENT RATE     : %i Hz\n\n", original_rate);   
            
        }
        
        
        
        window_ = XCreateWindow( display_, root, 0, 0, cfg.width_, cfg.height_, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr );

        XMapWindow( display_, window_ );


        if( cfg.fullscreen_ ) {
            
        } else {
            XSizeHints size_hints;
            size_hints.width = size_hints.min_width = size_hints.max_width = cfg.width_;
            size_hints.height = size_hints.min_height = size_hints.max_height = cfg.height_;
            size_hints.flags = PSize | PMinSize | PMaxSize;

            XSetWMNormalHints( display_, window_, &size_hints );
        }
        
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
            
            
            
            
            
            
#if 0            
            
#endif
            
            if( cfg.fullscreen_ ) {
                XGrabKeyboard( display_, DefaultRootWindow( display_ ), True, GrabModeAsync,
                               GrabModeAsync, CurrentTime );
                
                XGrabPointer( display_, DefaultRootWindow( display_ ), True,
                              ButtonPressMask |ButtonReleaseMask,
                              GrabModeAsync, GrabModeAsync, None,  None, CurrentTime );
                
                
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
                
                Cursor watch = XCreateFontCursor (display_, XC_watch);
                XIGrabDevice(display_, mouse_device_id, window_, CurrentTime,
                             watch , XIGrabModeAsync, XIGrabModeAsync,
                             True, &mask);
            } else {
                XISelectEvents(display_, window_, &mask, 1);
            }
            
            mask.deviceid = XIAllDevices;
            memset(mask.mask, 0, mask.mask_len);
            XISetMask(mask.mask, XI_RawMotion);
            XISelectEvents(display_, DefaultRootWindow(display_), &mask, 1);
            XSelectInput( display_, window_, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask );    
            free(mask.mask);
        } else {
            
            if( cfg.fullscreen_ ) {
                
                XGrabKeyboard( display_, DefaultRootWindow( display_ ), True, GrabModeAsync,
                               GrabModeAsync, CurrentTime );
                
                XGrabPointer( display_, DefaultRootWindow( display_ ), True,
                              ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                              GrabModeAsync, GrabModeAsync, None,  None, CurrentTime );   
            }
            
            XSelectInput( display_, window_, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask );
            
        }
        
        
        
       
        
    }
    
    if( start_context ) {
        glx_ctx_ = glXCreateContext( display_, visinfo, 0, True );
        glXMakeCurrent( display_, window_, glx_ctx_ );
    }
     
      
    XFree( visinfo );

    
    if( cfg.fullscreen_ ) {
        mouse_grabbed_ = true;
        xi_warp_.need_warp_ = true;
    }
    
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
    mode_setter_.reset();
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
    if( cfg_.fullscreen_ || grab == mouse_grabbed_ ) {
        return;
    }

    mouse_grabbed_ = grab;
    
    if( mouse_grabbed_ ) {
        xi_warp_.need_warp_ = true;
    }
    
}
gl_context::~gl_context() {}


}



#else

#include <SDL.h>

#include "pan.h"

#define vgl_notextern



#include "shared/log.h"

#include "interfaces.h"
#include "r_private.h"
#include "r_interface.h"
#include "g_message_passing.h"

static int              isshift = 0;

pan::gl_context::gl_context()
{
}


pan::gl_context::gl_context(const pan::gl_context::config &cfg)
{
    set_config(cfg);
}


pan::gl_context::~gl_context()
{
}


void pan::gl_context::set_config(const pan::gl_context::config &cfg)
{
    Uint32 fs_flag = cfg.fullscreen_ ? SDL_FULLSCREEN : 0;

    if ( ( sdl_surf_display = SDL_SetVideoMode ( cfg.width_, cfg.height_, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_OPENGL | fs_flag ) ) == NULL ) {
        __error ( "SDL_SetVidMode failed\n" );;
    }

    cfg_ = cfg;
}


void pan::gl_context::release_resources()
{
}

std::pair<gs_ksym, uint32_t> translate_key_event( SDL_Event event )
{

    unsigned int    gsksym = 0;

    unsigned int type = event.key.type == SDL_KEYDOWN ? SYMTYPE_PRESS : SYMTYPE_RELEASE;

    // do abstraction from X11 keysym to gsksym

    switch ( event.key.keysym.sym ) {
// ascii control codes 0x0 to 0x1F

    case SDLK_BACKSPACE:
        gsksym = '\b';
        goto abs_end;

    case SDLK_TAB:
        gsksym = '\t';
        goto abs_end;

    case SDLK_RETURN:
        gsksym = '\n';
        goto abs_end;

// cursor keys
    case SDLK_UP:
        //case SDLK_KP:
        gsksym = GSK_CUP;
        goto abs_end;

    case SDLK_DOWN:
        //case XK_KP_Down:
        gsksym = GSK_CDOWN;
        goto abs_end;

    case SDLK_RIGHT:
//         case XK_KP_Right:
        gsksym = GSK_CRIGHT;
        goto abs_end;

    case SDLK_LEFT:
//         case XK_KP_Left:
        gsksym = GSK_CLEFT;
        goto abs_end;


// modifier keys
    case SDLK_RSHIFT:
    case SDLK_LSHIFT:
        gsksym = GSK_SHIFT;
        if ( !isshift ) {
            isshift = 1;
        } else {
            isshift = 0;
        }
        SHV_Printf ( "isshift" );
        goto abs_end;

    case SDLK_RCTRL:
    case SDLK_LCTRL:
        gsksym = GSK_CTRL;
        goto abs_end;

    case SDLK_RALT:
    case SDLK_LALT:
//         case XK_Meta_L:
//         case XK_Meta_R:
        gsksym = GSK_ALT;
        goto abs_end;


    case SDLK_RSUPER:
    case SDLK_LSUPER:
        gsksym = GSK_FUCK0;
        goto abs_end;

//         case SDLK_RH:
//         case XK_Hyper_R:
//                 gsksym = GSK_FUCK1;
//                 goto abs_end;
//                 // other cursor conrol keys

    case SDLK_INSERT:
//         case XK_KP_Insert:
        gsksym = GSK_INSERT;
        goto abs_end;

    case SDLK_HOME:
//         case XK_KP_Home:
        gsksym = GSK_HOME;
        goto abs_end;

    case SDLK_END:
//         case XK_KP_End:
        gsksym = GSK_END;
        goto abs_end;

    case SDLK_PAGEUP:
//         case XK_KP_Page_Up:
        gsksym = GSK_PGUP;
        goto abs_end;

    case SDLK_PAGEDOWN:
//         case XK_KP_Page_Down:
        gsksym = GSK_PGDN;
        goto abs_end;

    case SDLK_ESCAPE:
        gsksym = GSK_ESCAPE;
        goto abs_end;

// function keys

    case SDLK_F1:
        gsksym = GSK_F1;
        goto abs_end;


    case SDLK_F2:
        gsksym = GSK_F2;
        goto abs_end;

    case SDLK_F3:
        gsksym = GSK_F3;
        goto abs_end;

    case SDLK_F4:
        gsksym = GSK_F4;
        goto abs_end;

    case SDLK_F5:
        gsksym = GSK_F5;
        goto abs_end;

    case SDLK_F6:
        gsksym = GSK_F6;
        goto abs_end;

    case SDLK_F7:
        gsksym = GSK_F7;
        goto abs_end;

    case SDLK_F8:
        gsksym = GSK_F8;
        goto abs_end;

    case SDLK_F9:
        gsksym = GSK_F9;
        goto abs_end;

    case SDLK_F10:
        gsksym = GSK_F10;
        goto abs_end;

    case SDLK_F11:
        gsksym = GSK_F11;
        goto abs_end;

    case SDLK_F12:
        gsksym = GSK_F12;
        goto abs_end;

    default:

        if ( ( event.key.keysym.sym >= 0x020 ) && ( event.key.keysym.sym <= 0x07f ) ) {
            gsksym = event.key.keysym.sym;
        }
        goto abs_end;

    }


abs_end:
//    if ( shiftmap[gsksym] && isshift ) {
//        gsksym = shiftmap[gsksym];
//    }

    return std::make_pair( gs_ksym(gsksym), type );
}

std::pair<gs_ksym, uint32_t> translate_mouse_button_event ( SDL_Event event )
{
    unsigned int    gsksym = 0;
    unsigned int    type = event.button.type == SDL_MOUSEBUTTONDOWN ? SYMTYPE_PRESS : SYMTYPE_RELEASE;


    switch ( event.button.button ) {
    case 1:
        gsksym = GSK_BUTTON1;
        break;

    case 2:
        gsksym = GSK_BUTTON2;
        break;

    case 3:
        gsksym = GSK_BUTTON3;
        break;

    case 4:
        gsksym = GSK_BUTTON4;
        break;


    case 5:
        gsksym = GSK_BUTTON5;
        break;

    default:
        __warning ( "noknown mousebutton pressed\n" );
        break;
    }

    return std::make_pair(gs_ksym(gsksym),type);
}
void pan::gl_context::dispatch_input(mp::queue &q)
{
    SDL_Event event;
    while ( SDL_PollEvent ( &event ) ) {
        switch ( event.type ) {
        case SDL_MOUSEMOTION:
            q.emplace<msg::mouse_event>(event.motion.xrel, event.motion.yrel, SYS_GetMsec());
            break;

        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
        {
            auto p = translate_mouse_button_event(event);
            q.emplace<msg::key_event>( keyevent_t(p.first, p.second) );


            break;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            auto p = translate_key_event(event);
            q.emplace<msg::key_event>( keyevent_t(p.first, p.second) );
            break;
        }
        default:
            printf ( "sdl event: %d\n", event.type );
        }
    }
}


void pan::gl_context::swap_buffers()
{
    glFlush();
    SDL_GL_SwapBuffers();
}


void pan::gl_context::grab_mouse(bool grab)
{
}

#endif
