#include <thread>
#include "pan.h"


#define vgl_notextern
#include "shared/log.h"

#include "interfaces.h"
#include "r_private.h"
#include "r_interface.h"
#include "g_message_passing.h"

#define GL_GLEXT_PROTOTYPES 1
#include "GL/gl.h"
#include "GL/glext.h"

sh_var_t *r_devicewidth, *r_deviceheight;
static sh_var_t *r_fullscreen;


keyevent_t              keventlist[128];
unsigned int            keventlistptr;
int     md_x, md_y;

gl_info_t       *r_glinfo = NULL;


#define USE_INPUT_THREAD (0)

#if 0
static pan::gl_context s_gl_context;
static mp::queue *s_queue = 0;

void R_StartUp( mp::queue &q )
{
	TFUNC_ENTER;
    s_queue = &q;
	

	r_devicewidth = SHP_GetVar ( "r_devicewidth" );
	r_deviceheight = SHP_GetVar ( "r_deviceheight" );
	r_fullscreen = SHP_GetVar ( "r_fullscreen" );

    pan::gl_context::config cfg;
    
//    cfg.fullscreen_ = true;
    
    
//     if( cfg.fullscreen_ ) {
//         SHP_SetVar("r_devicewidth", "2560", 0 );
//         SHP_SetVar("r_deviceheight", "1440", 0 );
//     }
    cfg.width_ = r_devicewidth->ivalue;
    cfg.height_ = r_deviceheight->ivalue;
    
    s_gl_context.set_config(cfg);

    
    GLuint x[2];
//     glGenBuffers( 2, x );
    
    int err =  ((int)glGetError());
    
    const GLubyte *version = glGetString( GL_VERSION );
    
    DD_LOG << "opengl version: " << (const char*)version << " " << x[0] << " " << x[1] << " " << (err == GL_NO_ERROR ? "good" : "bad" ) << "\n";
    //getchar();
    
	// hard coded gl info. I assume that _every_ card in use on this planet supports this stuff...
	r_glinfo = ( gl_info_t * ) MM_Malloc ( sizeof ( gl_info_t ) );
	r_glinfo->arb_multitexture = 1;
	r_glinfo->texenv_units = 1;
	r_glinfo->texenv_have_add = 1;


	glBegin ( GL_POLYGON );

	glVertex3f ( -0.5, -0.5, 0 );
	glVertex3f ( 0.5, -0.5, 0 );
	glVertex3f ( 0.5, 0.5, 0 );
	glVertex3f ( -0.5, 0.5, 0 );
	glEnd();

	glFlush();
	
    
    s_gl_context.swap_buffers();
    
	glClear ( GL_COLOR_BUFFER_BIT );

// let e'm say sth
	R_Talk();

	if ( r_fullscreen->ivalue ) {
		R_Hint ( R_HINT_GRAB_MOUSE );
	}
}

/*
  ====================
  R_ShutDown

  ====================
*/
void R_ShutDown()
{
	TFUNC_ENTER;


    
    
//     if( input_thread.joinable() ) {
//         
//         SDL_Event user_event;
// 
//         user_event.type=SDL_USEREVENT;
//         user_event.user.code=2;
//         user_event.user.data1=NULL;
//         user_event.user.data2=NULL;
//         SDL_PushEvent(&user_event);
//         
//         input_thread.join();   
//     }
//     
// 	__named_message ( "\n" );


    //TODO!
    s_gl_context.release_resources();
	TFUNC_LEAVE;
}

/*
  ====================
  R_SwapBuffer

  ====================
*/
void R_SwapBuffer ( void )
{
    s_gl_context.swap_buffers();
}




///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
void R_Hint ( int hint )
{
	switch ( hint ) {
	case R_HINT_GRAB_MOUSE:
        s_gl_context.grab_mouse(true);
		
		break;
	

	case R_HINT_UNGRAB_MOUSE:
		s_gl_context.grab_mouse(false);
		break;

	default:
		__warning ( "unknown render hint: %d\n", hint );
	}
}



void I_Update()
{
    s_gl_context.dispatch_input( *s_queue );
}



void I_SetInputMode ( int _mode )
{
	__named_message ( "\n" );
}


unsigned char I_GetChar()
{
	return  'q';
}
#endif

