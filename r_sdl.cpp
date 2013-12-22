#include <thread>


#if D3DYNE_OS_WIN


#include <SDL.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_mouse.h>
#include <SDL/SDL_opengl.h>
#endif

#define vgl_notextern
#include "shared/log.h"

#include "interfaces.h"
#include "r_private.h"
#include "r_interface.h"
#include "g_message_passing.h"

static SDL_Surface* sdl_surf_display = 0;
sh_var_t *r_devicewidth, *r_deviceheight;
static sh_var_t *r_fullscreen;

static int              isshift = 0;

static char             shiftmap[256];

keyevent_t              keventlist[128];
unsigned int            keventlistptr;
int     md_x, md_y;

gl_info_t       *r_glinfo = NULL;


#define USE_INPUT_THREAD (0)

/*
  ====================
  R_StartUp

  ====================
*/
void I_SDLDoKey ( SDL_Event event );
void I_SDLDoMouseMotion ( SDL_Event event );
void I_SDLDoMouseButton ( SDL_Event event );

void input_thread_func() {
 
    mp::queue &q = g_global_mp::get_instance()->get_queue();
    
    bool stop_requested = false;
    
    while( !q.is_stopped() && !stop_requested ) {
     
        SDL_Event event;
        keventlistptr = 1;
        md_x = md_y = 0;
        
        SDL_WaitEvent(&event);
        
        switch ( event.type ) {
        case SDL_MOUSEMOTION:
            I_SDLDoMouseMotion ( event );
            break;
            
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
            I_SDLDoMouseButton ( event );
            break;
            
            
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            I_SDLDoKey ( event );
            break;
            
        case SDL_USEREVENT:
            if( event.user.code == 2 ) {
                DD_LOG << "stopping input thread due to stop event\n";
                stop_requested = true;
             
            }
            break;
            
        default:
            printf ( "sdl event: %d\n", event.type );
        }
        
    }

  
  
  if( !stop_requested ) {
      DD_LOG << "input thread stopped because queue::is_stopped\n";   
  }
  DD_LOG << "input thread returned\n";
    
    
}
static std::thread input_thread;

void I_SDLStartUp();

void TestOpenGL20() {
	GLenum err = glewInit();
	if ( err!=GLEW_OK )	{
		//Problem: glewInit failed, something is seriously wrong.
		__error("glewInit failed, aborting.");
	}

	GLuint index_buffer_obj = 0;
	glGenBuffers(1, &index_buffer_obj);
	printf("index_buffer_obj: %d\n", index_buffer_obj);

	GLuint vertex_buffer_obj = 0;
	glGenBuffers(1, &vertex_buffer_obj);
	printf("vertex_buffer_obj: %d\n", vertex_buffer_obj);

//	__error("xx");
}

void R_StartUp()
{
	TFUNC_ENTER;

	if ( SDL_Init ( SDL_INIT_EVERYTHING ) < 0 ) {
		__error ( "SDL_Init failed\n" );
	}

	r_devicewidth = SHP_GetVar ( "r_devicewidth" );
	r_deviceheight = SHP_GetVar ( "r_deviceheight" );
	r_fullscreen = SHP_GetVar ( "r_fullscreen" );

	Uint32 fs_flag = r_fullscreen->ivalue ? SDL_FULLSCREEN : 0;

    
    
	
	
	if ( ( sdl_surf_display = SDL_SetVideoMode ( r_devicewidth->ivalue, r_deviceheight->ivalue, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_OPENGL | fs_flag ) ) == NULL ) {
		__error ( "SDL_SetVidMode failed\n" );;
	}
	I_SDLStartUp();

    const GLubyte *version = glGetString(GL_VERSION);
    
    GLuint x[2];
    glGenBuffers( 2, x );
    
    int err =  ((int)glGetError());
    
    DD_LOG << "opengl version: " << (const char*)version << " " << x[0] << " " << x[1] << " " << (err == GL_NO_ERROR ? "good" : "bad" ) << "\n";
    //getchar();
    
	// hard coded gl info. I assume that _every_ card in use on this planet supports this stuff...
	r_glinfo = ( gl_info_t * ) MM_Malloc ( sizeof ( gl_info_t ) );
	r_glinfo->arb_multitexture = 1;
	r_glinfo->texenv_units = 1;
	r_glinfo->texenv_have_add = 1;

	TestOpenGL20();

	glBegin ( GL_POLYGON );

	glVertex3f ( -0.5, -0.5, 0 );
	glVertex3f ( 0.5, -0.5, 0 );
	glVertex3f ( 0.5, 0.5, 0 );
	glVertex3f ( -0.5, 0.5, 0 );
	glEnd();

	glFlush();
	SDL_GL_SwapBuffers();
	glClear ( GL_COLOR_BUFFER_BIT );

// let e'm say sth
	R_Talk();

	if ( r_fullscreen->ivalue ) {
		R_Hint ( R_HINT_GRAB_MOUSE );
	}

#if USE_INPUT_THREAD
	input_thread = std::thread( input_thread_func );
//     t1.detach();
#endif
	TFUNC_LEAVE;
}

/*
  ====================
  R_ShutDown

  ====================
*/
void R_ShutDown()
{
	TFUNC_ENTER;


    
    
    if( input_thread.joinable() ) {
        
        SDL_Event user_event;

        user_event.type=SDL_USEREVENT;
        user_event.user.code=2;
        user_event.user.data1=NULL;
        user_event.user.data2=NULL;
        SDL_PushEvent(&user_event);
        
        input_thread.join();   
    }
    
	__named_message ( "\n" );


	TFUNC_LEAVE;
}

/*
  ====================
  R_SwapBuffer

  ====================
*/
void R_SwapBuffer ( void )
{
//      __named_message( "\n" );
	glFlush();
	SDL_GL_SwapBuffers();
}




///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
void R_Hint ( int hint )
{
	switch ( hint ) {
	case R_HINT_GRAB_MOUSE:
		
		SDL_ShowCursor(0);
		SDL_WM_GrabInput ( SDL_GRAB_ON );
		break;
	

	case R_HINT_UNGRAB_MOUSE:
		
		SDL_ShowCursor(1);
		SDL_WM_GrabInput ( SDL_GRAB_OFF );
		break;

	default:
		__warning ( "unknown render hint: %d\n", hint );
	}
}



void I_Update()
{
    
#if !USE_INPUT_THREAD
    
	SDL_Event event;
	keventlistptr = 1;
	md_x = md_y = 0;

	while ( SDL_PollEvent ( &event ) ) {
		switch ( event.type ) {
		case SDL_MOUSEMOTION:
			I_SDLDoMouseMotion ( event );
			break;

		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			I_SDLDoMouseButton ( event );
			break;


		case SDL_KEYDOWN:
		case SDL_KEYUP:
			I_SDLDoKey ( event );
			break;

		default:
			printf ( "sdl event: %d\n", event.type );
		}
	}

	keventlist[0].sym = ( i_ksym_t ) keventlistptr; // first element in list is listsize!
#endif
}


void I_SDLStartUp()
{


	for ( size_t i = 0; i < 256; i++ ) {
		shiftmap[i] = 0;
	}

	shiftmap['1'] = '!';
	shiftmap['2'] = '@';
	shiftmap['3'] = '#';
	shiftmap['4'] = '$';
	shiftmap['5'] = '%';
	shiftmap['6'] = '^';
	shiftmap['7'] = '&';
	shiftmap['8'] = '*';
	shiftmap['9'] = '(';
	shiftmap['0'] = ')';
	shiftmap['-'] = '_';
	shiftmap['='] = '+';
	shiftmap['\\'] = '|';
	shiftmap['['] = '{';
	shiftmap[']'] = '}';
	shiftmap[';'] = ':';
	shiftmap['\''] = '"';
	shiftmap[','] = '<';
	shiftmap['.'] = '>';
	shiftmap['/'] = '?';


}


void I_SDLDoKey ( SDL_Event event )
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
	if ( shiftmap[gsksym] && isshift ) {
		gsksym = shiftmap[gsksym];
	}

	keventlist[keventlistptr].sym = ( i_ksym_t ) gsksym;
	keventlist[keventlistptr].type = type;
	g_global_mp::get_instance()->get_queue().emplace<msg::key_event>( keventlist[keventlistptr] );
    
    //keventlistptr++;
    
    
    
//      printf( "gsksym: %d\n", gsksym );
}


void I_SDLDoMouseMotion ( SDL_Event event )
{



//  	md_x += event.motion.xrel;
//  	md_y += event.motion.yrel;

    g_global_mp::get_instance()->get_queue().emplace<msg::mouse_event>( event.motion.xrel, event.motion.yrel, SYS_GetMsec() );

}




void I_SDLDoMouseButton ( SDL_Event event )
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

	keventlist[keventlistptr].sym = ( i_ksym_t ) gsksym;
	keventlist[keventlistptr].type = type;
	g_global_mp::get_instance()->get_queue().emplace<msg::key_event>( keventlist[keventlistptr] );
    
    //keventlistptr++;
}


void I_SetInputMode ( int _mode )
{
	__named_message ( "\n" );
}


unsigned char I_GetChar()
{
	return  'q';
}


