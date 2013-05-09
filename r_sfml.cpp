#define vgl_notextern
#include <SFML/Graphics.hpp>

#define GL_GLEXT_PROTOTYPES 1
#include <SFML/OpenGL.hpp>

// #include "glcorearb.h"
#include "interfaces.h"
#include "r_private.h"
#include "r_interface.h"




// #include <SFML/Window.hpp>


sh_var_t *r_devicewidth, *r_deviceheight;
static sh_var_t *r_fullscreen;

static int              isshift = 0;

static char             shiftmap[256];

keyevent_t              keventlist[128];
unsigned int            keventlistptr;
int     md_x, md_y;

gl_info_t       *r_glinfo = NULL;
/*
  ====================
  R_StartUp

  ====================
*/

class sfml_state {
public:
    sfml_state( const sf::ContextSettings & cs ) 
    : window_(sf::VideoMode(r_devicewidth->ivalue, r_deviceheight->ivalue), "3dyne legacy engine", sf::Style::Default, cs )
    {
        std::cout << "sfml_state constructor\n";
        
    }
    
    sf::RenderWindow window_;
};


static sfml_state *s_sfml_state = 0;

void I_SDLStartUp();
void R_StartUp()
{
	TFUNC_ENTER;
        
        
        
     
        
        
// 	if ( SDL_Init ( SDL_INIT_AUDIO ) < 0 ) {
// 		__error ( "SDL_Init failed\n" );
// 	}

	r_devicewidth = SHP_GetVar ( "r_devicewidth" );
	r_deviceheight = SHP_GetVar ( "r_deviceheight" );
	r_fullscreen = SHP_GetVar ( "r_fullscreen" );

	sf::ContextSettings cs(32, 0, 0, 1, 2 );
    
	
    if( s_sfml_state == 0 ) {
        s_sfml_state = new sfml_state(cs);
    }
    
    s_sfml_state->window_.display();
    GLuint x;
    glGenBuffers( 1, &x );
	
	I_SDLStartUp();

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
	
	glClear ( GL_COLOR_BUFFER_BIT );
//         glGenBuffers();
// let e'm say sth
	R_Talk();

	if ( r_fullscreen->ivalue ) {
		R_Hint ( R_HINT_GRAB_MOUSE );
	}

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
        
        if( s_sfml_state != 0 ) {
            s_sfml_state->window_.display();
        }
}




///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
void R_Hint ( int hint )
{
    
#if 0
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
#endif
}


void sfml_do_key ( const sf::Event & event );
void sfml_do_mouse_motion ( const sf::Event & event );
void sfml_do_mouse_button ( const sf::Event & event );

void I_Update()
{
    if( s_sfml_state == 0 ) {
        return;
    }

    

    
    sf::Event event;
    
    while( s_sfml_state->window_.pollEvent( event ) ) {
            switch ( event.type ) {
            case sf::Event::MouseMoved:
                    sfml_do_mouse_motion ( event );
                    break;

            case sf::Event::MouseButtonPressed:
            case sf::Event::MouseButtonReleased:
                    sfml_do_mouse_button ( event );
                    break;


            case sf::Event::KeyPressed:
            case sf::Event::KeyReleased:
                    sfml_do_key ( event );
                    break;

            default:
                    printf ( "sfml event: %d\n", event.type );
            }    
        
    }
    
	
	
	
	keventlist[0].sym = ( gs_ksym ) keventlistptr; // first element in list is listsize!
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


void sfml_do_key ( const sf::Event & event )
{

	unsigned int    gsksym = 0;

	unsigned int type = event.type == sf::Event::KeyPressed ? SYMTYPE_PRESS : SYMTYPE_RELEASE;

	// do abstraction from X11 keysym to gsksym

        
        
	switch ( event.key.code ) {
// ascii control codes 0x0 to 0x1F
	case sf::Keyboard::BackSpace:
		gsksym = '\b';
		goto abs_end;

	case sf::Keyboard::Tab:
		gsksym = '\t';
		goto abs_end;

	case sf::Keyboard::Return:
		gsksym = '\n';
		goto abs_end;

// cursor keys
	case sf::Keyboard::Up:
		//case SDLK_KP:
		gsksym = GSK_CUP;
		goto abs_end;

	case sf::Keyboard::Down:
		//case XK_KP_Down:
		gsksym = GSK_CDOWN;
		goto abs_end;

	case sf::Keyboard::Right:
//         case XK_KP_Right:
		gsksym = GSK_CRIGHT;
		goto abs_end;

	case sf::Keyboard::Left:
//         case XK_KP_Left:
		gsksym = GSK_CLEFT;
		goto abs_end;


// modifier keys
	case sf::Keyboard::LShift:
        case sf::Keyboard::RShift:
		gsksym = GSK_SHIFT;
		if ( !isshift ) {
			isshift = 1;
		} else {
			isshift = 0;
		}
		SHV_Printf ( "isshift" );
		goto abs_end;

	case sf::Keyboard::LControl:
	case sf::Keyboard::RControl:
		gsksym = GSK_CTRL;
		goto abs_end;

	case sf::Keyboard::LAlt:
	case sf::Keyboard::RAlt:
//         case XK_Meta_L:
//         case XK_Meta_R:
		gsksym = GSK_ALT;
		goto abs_end;


	case sf::Keyboard::LSystem:
	case sf::Keyboard::RSystem:
		gsksym = GSK_FUCK0;
		goto abs_end;

//         case SDLK_RH:
//         case XK_Hyper_R:
//                 gsksym = GSK_FUCK1;
//                 goto abs_end;
//                 // other cursor conrol keys

	case sf::Keyboard::Insert:
//         case XK_KP_Insert:
		gsksym = GSK_INSERT;
		goto abs_end;

	case sf::Keyboard::Home:
//         case XK_KP_Home:
		gsksym = GSK_HOME;
		goto abs_end;

	case sf::Keyboard::End:
//         case XK_KP_End:
		gsksym = GSK_END;
		goto abs_end;

	case sf::Keyboard::PageUp:
//         case XK_KP_Page_Up:
		gsksym = GSK_PGUP;
		goto abs_end;

	case sf::Keyboard::PageDown:
//         case XK_KP_Page_Down:
		gsksym = GSK_PGDN;
		goto abs_end;

	case sf::Keyboard::Escape:
		gsksym = GSK_ESCAPE;
		goto abs_end;

// function keys

	case sf::Keyboard::F1:
		gsksym = GSK_F1;
		goto abs_end;


	case sf::Keyboard::F2:
		gsksym = GSK_F2;
		goto abs_end;

	case sf::Keyboard::F3:
		gsksym = GSK_F3;
		goto abs_end;

	case sf::Keyboard::F4:
		gsksym = GSK_F4;
		goto abs_end;

	case sf::Keyboard::F5:
		gsksym = GSK_F5;
		goto abs_end;

	case sf::Keyboard::F6:
		gsksym = GSK_F6;
		goto abs_end;

        case sf::Keyboard::F7:
		gsksym = GSK_F7;
		goto abs_end;

	case sf::Keyboard::F8:
		gsksym = GSK_F8;
		goto abs_end;

	case sf::Keyboard::F9:
		gsksym = GSK_F9;
		goto abs_end;

	case sf::Keyboard::F10:
		gsksym = GSK_F10;
		goto abs_end;

	case sf::Keyboard::F11:
		gsksym = GSK_F11;
		goto abs_end;

	case sf::Keyboard::F12:
		gsksym = GSK_F12;
		goto abs_end;

	default:

		if ( ( event.key.code >= sf::Keyboard::A ) && ( event.key.code <= sf::Keyboard::Z ) ) {
			gsksym = 'a' + (event.key.code - sf::Keyboard::A);
		} else if ( ( event.key.code >= sf::Keyboard::Num0 ) && ( event.key.code <= sf::Keyboard::Num9 ) ) {
                    gsksym = '0' + (event.key.code - sf::Keyboard::Num0);
                }
		goto abs_end;

	}


abs_end:
	if ( shiftmap[gsksym] && isshift ) {
		gsksym = shiftmap[gsksym];
	}

	keventlist[keventlistptr].sym = ( gs_ksym ) gsksym;
	keventlist[keventlistptr].type = type;
	keventlistptr++;
//      printf( "gsksym: %d\n", gsksym );
}


void sfml_do_mouse_motion ( const sf::Event & event )
{
	md_x += event.mouseMove.x;
	md_y += event.mouseMove.y;
}




void sfml_do_mouse_button ( const sf::Event& event )
{
	unsigned int    gsksym = 0;
	unsigned int    type = event.type == sf::Event::MouseButtonPressed ? SYMTYPE_PRESS : SYMTYPE_RELEASE;


	switch ( event.mouseButton.button ) {
        case sf::Mouse::Left:
		gsksym = GSK_BUTTON1;
		break;

	case sf::Mouse::Middle:
		gsksym = GSK_BUTTON2;
		break;

        case sf::Mouse::Right:
		gsksym = GSK_BUTTON3;
		break;

#if 0
	case 4:
		gsksym = GSK_BUTTON4;
		break;


	case 5:
		gsksym = GSK_BUTTON5;
		break;
#endif
	default:
		__warning ( "noknown mousebutton pressed\n" );
		break;
	}

	keventlist[keventlistptr].sym = ( gs_ksym ) gsksym;
	keventlist[keventlistptr].type = type;
	keventlistptr++;
}


void I_SetInputMode ( int _mode )
{
	__named_message ( "\n" );
}


unsigned char I_GetChar()
{
	return  'q';
}


