/* 
 * 3dyne Legacy Engine GPL Source Code
 * 
 * Copyright (C) 1996-2012 Matthias C. Berger & Simon Berger.
 * 
 * This file is part of the 3dyne Legacy Engine GPL Source Code ("3dyne Legacy
 * Engine Source Code").
 *   
 * 3dyne Legacy Engine Source Code is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * 
 * 3dyne Legacy Engine Source Code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * 3dyne Legacy Engine Source Code.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * In addition, the 3dyne Legacy Engine Source Code is also subject to certain
 * additional terms. You should have received a copy of these additional terms
 * immediately following the terms and conditions of the GNU General Public
 * License which accompanied the 3dyne Legacy Engine Source Code.
 * 
 * Contributors:
 *     Matthias C. Berger (mcb77@gmx.de) - initial API and implementation
 *     Simon Berger (simberger@gmail.com) - initial API and implementation
 */

#include "compiler_config.h"

// main.c
#include <stdio.h> 
//#include <stdlib.h>

#include <signal.h>
#include <r_interface.h>

#include "interfaces.h"
#include "sys_console.h"

#define RENDER "./r_glide2x.so"

#include "version.h"
#include "log.h"
#include "message_passing.h"
#include "pan.h"
#include "game_shell.h"
#include "sh_alias.h"
#include "r_private.h"
#include "message_passing.h"

#if DD_USE_DLT
#include "dlt.h"
#endif

#if 0
vid_device_t*	vid_dev;
vid_vpage_t*	vid_page;
#endif

char		font[128][64];
int		m_counter;

unsigned char	pal[768];

char	padir[128];
char	pgdir[128];

static int	config_valid = 0;
static int	waserror = 0;
//arche_t		*archets;

g_resources_t	*g_rs;
g_state_t	*g_st;




/*
  ====================
  shock handling
  ====================
*/

void ShutDownBasic();
void ShockHandler()
{
	__named_message( "\n" );

	if( waserror )
	{
		printf( "panic: error while error handling\n" );
		exit( -1 );
	}
	waserror = 1;

	

#ifdef trace_functions
	{
		char	funcname[512];
		TF_FuncHistory( funcname, 512 );
		fprintf( stderr, "function trace: %s\n", funcname );
	}
#endif
	

	fprintf( stderr, "\n" );

//	R_ShutDown();
	ShutDownBasic();

	fprintf( stderr, "ShockHandler: the following segmentation fault is caused by Mesa!\n" );
//	abort();
//	raise( SIGTERM );

	SYS_CloseConsole();
#if D3DYNE_OS_WIN
  //  _asm{ int 3 }
#endif
	exit( -1 );
}

#if D3DYNE_OS_WIN
//typedef LONG (WINAPI *PTOP_LEVEL_EXCEPTION_FILTER)(
//    _In_ struct _EXCEPTION_POINTERS *ExceptionInfo
//    );

LONG WINAPI win32Exception( struct _EXCEPTION_POINTERS *einfo )
{
	__warning( "unhandled exception\n" );
	printf( "exception code: 0x%x\n", einfo->ExceptionRecord->ExceptionCode );
//    _asm{ int 3 }
#ifdef trace_functions
	{
		char	funcname[512];
		TF_FuncHistory( funcname, 512 );
		fprintf( stderr, "function trace: %s\n", funcname );
	}
#endif

//	R_ShutDown();
	ShutDownBasic();
	
	return EXCEPTION_CONTINUE_SEARCH;
}	
#endif

void SecureShutDown( int sig )
{
// 	char	*ptr;

	if( waserror )
	{
		printf( "panic: signal %d while error handling\n", sig );
		exit( -1 );
	}
	__named_message( "\n" );
	__error( "fatal signal %d caught.\n", sig );
#if D3DYNE_OS_WIN
//    _asm{ int 3 }
#endif
//	kill( getpid(), 9 );


#if 0
	R_ShutDown();
	
	ShutDownBasic();



	ptr = NULL;
	if( sig == 11 )
	{
//		fprintf( stderr, "provcate: " );
//		*ptr = 0;
	}
//	raise( SIGTERM );


	exit( sig );
#endif
}

/*
  ====================
  StartUpGpi()
  
  start GSHell + lowlevel gpi services
  ====================
*/ 
void StartUpBasic()
{

	//
	// GSHell startup
	SHP_StartUp();  // shell parser
	SHI_StartUp();  // shell input


	MM_StartUp();
	IB_StartUp();
}


/*
  ====================
  ShutDownGpi
  ====================
*/
void ShutDownBasic()
{
//	__named_message( "\tshutting down GPI\n" );

	HUD_ShutDown();

	LA_ShutDown();

	SND_ShutDown();

	signal( SIGSEGV, SIG_DFL );
	signal( SIGABRT, SIG_DFL );
	signal( SIGTERM, SIG_DFL );
	signal( SIGINT,  SIG_DFL );
	signal( SIGFPE,  SIG_DFL );


	IB_ShutDown();
	MM_ShutDown();

}

/*
  ====================
  SaveConfig
  
  write some dynmaic config
  scripts
  ====================
*/
void SaveConfig()
{
	FILE	*h;
	char	name[256];


	//
	// create HOME/tgfkaa/graph.gsh
	// this is the game independent graphic configuration
	memset( name, 0, 256 );
	strcat( name, padir );
	strcat( name, "/graph.gsh" );
	
//	__named_message( "\twriting %s ...\n", name );
	
	h = fopen( name, "wb" );
	if( !h )
	{
		__warning( "cannot write %s\n", name );
		goto write_config;  // yiiiieeeeehaaa!!
	}
	fprintf( h, "#####################################################################\n" );
	fprintf( h, "# This file contains the graphic configuration. it will be \n" );
	fprintf( h, "# regenerated by the game. so dont add new commands here.\n" );

	fprintf( h, "#####################################################################\n" );	
	fprintf( h, "\n" );
	fprintf( h, "let r_gldriver\t%s\n", ( SHP_GetVar( "r_gldriver" ))->string);
	fprintf( h, "let r_gldriver_win32\t%s\n", ( SHP_GetVar( "r_gldriver_win32" ))->string);

	fprintf( h, "let r_devicewidth\t%s\n", ( SHP_GetVar( "r_devicewidth" ))->string );
	fprintf( h, "let r_deviceheight\t%s\n", ( SHP_GetVar( "r_deviceheight" ))->string );
	fprintf( h, "let r_devicerefresh\t%s\n", ( SHP_GetVar( "r_devicerefresh" ))->string );
	fprintf( h, "let r_viewwidth\t%s\n", ( SHP_GetVar( "r_viewwidth" ))->string );
	fprintf( h, "let r_viewheight\t%s\n",( SHP_GetVar( "r_viewheight" ))->string );
	fprintf( h, "let r_gamma\t%s\n", ( SHP_GetVar( "r_gamma" ))->string );
	fprintf( h, "let r_fullscreen\t%d\n", ( SHP_GetVar( "r_fullscreen" ))->ivalue );
	fprintf( h, "let gc_dumpudp\t%d\n", ( SHP_GetVar( "gc_dumpudp" ))->ivalue );

	
	fclose( h );


write_config:
	//
	// write HOME/tgfkaa/GAME/config.gsh
	// contains game-dependent configuration ( key bindings, sh vars ) 


	memset( name, 0, 256 );
	strcat( name, pgdir );
	strcat( name, "/config.gsh" );
	
	h = fopen( name, "wb" );
	if( !h )
	{
		__warning( "cannot write %s\n", name );
		return;
	}

	fprintf( h, "# ===========================================================================\n" );
	fprintf( h, "# this file was generated by the game. do only change it,\n" );
	fprintf( h, "# if you are familiar with the console !\n" );
	fprintf( h, "# ===========================================================================\n\n" );

	fprintf( h, "let gc_mousescale %.2f\n", gc_mousescale->fvalue );
	fprintf( h, "let gc_invmouse %d\n", gc_invmouse->ivalue );
	fprintf( h, "let direct_level %s\n", ((sh_var_t * )SHP_GetVar( "direct_level" ))->string );
	fprintf( h, "let r_lod %.2f\n", ((sh_var_t * )SHP_GetVar( "r_lod" ))->fvalue );
	fprintf( h, "let snd_sfxvol %.2f\n", ((sh_var_t * )SHP_GetVar( "snd_sfxvol" ))->fvalue );
	fprintf( h, "let snd_musvol %.2f\n", ((sh_var_t * )SHP_GetVar( "snd_musvol" ))->fvalue );
	fprintf( h, "let snd_echodelay %.2f\n", ((sh_var_t * )SHP_GetVar( "snd_echodelay" ))->fvalue );
	fprintf( h, "let snd_echovol %.2f\n", ((sh_var_t * )SHP_GetVar( "snd_echovol" ))->fvalue );
	fprintf( h, "let snd_echofb %.2f\n", ((sh_var_t * )SHP_GetVar( "snd_echofb" ))->fvalue );
	fprintf( h, "let snd_doecho %d\n", ((sh_var_t * )SHP_GetVar( "snd_doecho" ))->ivalue );

	fprintf( h, "let sound %d\n", ((sh_var_t * )SHP_GetVar( "sound" ))->ivalue );
	fprintf( h, "let music %d\n", ((sh_var_t * )SHP_GetVar( "music" ))->ivalue );

	fprintf( h, "let gc_remoteaddr %s\n", ((sh_var_t * )SHP_GetVar( "gc_remoteaddr" ))->string );
	fprintf( h, "let gc_remoteport %s\n", ((sh_var_t * )SHP_GetVar( "gc_remoteport" ))->string );
	fprintf( h, "let gc_localport %s\n", ((sh_var_t * )SHP_GetVar( "gc_localport" ))->string );

	fprintf( h, "\n" );
	SHI_WriteJoints( h );
	fclose( h );

	__named_message( "alive!\n" );
}  
// \x1b[K
void Greetings()
{
//               = clear    =
	// i've seen this in mpg123 ...
//	__named_message( "\n" );
#if 0
	{                                                                                 
		const char *term_type;   
		char	string[256];
		
		term_type = getenv("TERM");                                            
		if( term_type )
		{
			if (!strcmp(term_type,"xterm"))                                        
			{        
				sprintf( string, "DarkestDays %s v%s - DarkestDays@gmx.net -", BTYPE, BVERSION );
				
				fprintf(stderr, "\033]0;%s\007", string );                          
			}
		}
	}

#endif
	fprintf( stderr, "watch!\n" );

	printf( "\x1b[H\x1b[J\x1b[37;44m\x1b[K\x1b[1;33m                     DarkestDays %s version %s\x1b[0m\n", DD_BTYPE, BVERSION );
	printf( "Built at %s on %s\n", BDATE, BHOST );

	printf( "==============================================================================\n" );
	printf( " Starting up your Darkest Days\n" );
	printf( "                                   %s version %s\n", DD_BTYPE, BVERSION );
	printf( "                         copyright (c) 1999-2000 by Matthias & Simon Berger\n" );
	printf( "                            %s\n", BCOMMENT );
	printf( "==============================================================================\n\n" );
	//	sleep( 2 );
}

#if 1
int ibntest()
{
	ib_file_t	*h1;
	char	buf[256];

	IB_StartUp();

	IB_AddSource( "dd1/ibntest.sar", SOURCE_SAR );
//	IB_AddSource( "dd1/ibntest2", SOURCE_DISK );

	h1 = IB_Open( "file1" );
	memset( buf, 0, 255 );

	IB_SetPos( h1, 2 );

	IB_Read( buf, 3, 1, h1 );

	printf( "read: %s %d %d\n", buf, IB_GetPos( h1 ), IB_GetSize( h1 ) );

	IB_Close( h1 );

	IB_ShutDown();

//	__error( "exiting .. \n" );

    return 0;
}
	
#endif




void Exit( void )
{
	__named_message( " bye ...\n" );
//	R_ShutDown();

	SaveConfig();
	ShutDownBasic();
	fprintf( stderr, "Exit: the following segmentation fault is caused by Mesa!\n" );
	exit( 0 );
}
	


int g_main( int argc, char* argv[] )
{
	ibfile_t*	handle;
	int	i, i2;
	char	text[128];
	char	argline[128];

#if DD_USE_DLT
    DLT_REGISTER_APP("LOG","Test Application for Logging");
#endif
    
// 	ca_wave_t	*wave;
// 	ca_tga_t	*tga;
	
// 	timeval_t	time1, time2;
// 	unsigned int	usecs;

	config_valid = 0;

	TFUNC_ENTER;
	Greetings();

	StartUpBasic();
	signal( SIGSEGV, (void(*) (int)) SecureShutDown );
	signal( SIGABRT, (void(*) (int)) SecureShutDown );
	signal( SIGTERM, (void(*) (int)) SecureShutDown );
//        signal( SIGQUIT, (void(*) ()) SecureShutDown );
	signal( SIGINT,  (void(*) (int)) SecureShutDown );

#if D3DYNE_OS_WIN
	LPTOP_LEVEL_EXCEPTION_FILTER xxx;

	//SetUnhandledExceptionFilter( win32Exception );
#endif

	SOS_SetShockHandler( ShockHandler );


	//
	// low level configuration


//	printf( "testing ibn\n" );
//	ibntest();

	strcpy( padir, SYS_GetPADir() );
//	IB_AddSource( text, SOURCE_DISK );

    ibase::service ib_service;
    ibase::service::set_singleton( &ib_service );






	SHP_SetVar( "game", "dd1", SH_SV_RDONLY );


	//
	// parse graph.gsh
	// must be here because the vars set in graph.gsh should be flatable
	// by the commandline
	memset( text, 0, 127 );
 
	sprintf( text, "%s/graph.gsh", padir );


	// HERE!

	handle = IB_OpenDiskFile( text );
	if( !handle )
	{
		// no private graph.gsh ... look in .
		handle = IB_OpenDiskFile( "./graph.gsh" );
	}

	// HERE!

	if( !handle )
	{
		// in windows padir == . so we need an alternate file for default configuration
		// in unix the master graph.gsh lying in ./ should'n be touched by any user. ( very theoretical. )
		handle = IB_OpenDiskFile( "./graph.orig" );
		__message( "graphic configuration file deleted.\nusing graph.orig\n" );
	}
	// HERE!

	if( !handle ) // bad
		__error( "graph.orig not found.\nreinstall Darkest Days.\n" );



	SHP_ParseFile( handle );

	IB_Close( handle );


	

	// NOT!

	memset( argline, 0, 128 );
	for( i = 1; i < argc;)
	{
		if( *argv[i] == '+' )
		{
			memset( argline, 0, 128 );
			strcat( argline, argv[i]+1 );
			strcat( argline, " " );
			i++;
			for( i2 = i; i2 < argc; i2++ )
			{
				if( *argv[i2] == '+' )
				{
					break;
				}
				strcat( argline, argv[i2] );
				strcat( argline, " " );
				i++;
			}
			//printf( "al %s\n", argline );
			SHP_ParseBuf( argline );
			
 		} else
		{
			break;
		}
	}

	//
	// do game dependent configuration
	SHV_Printf( "game: %s\n", SHP_GetVar( "game" )->string );	
	// NOT!
	//
	// add ./GAME and HOME/tgfkaa/GAME

	IB_AddSource( ( SHP_GetVar( "game" ))->string, SOURCE_DISK );

	strcpy( pgdir, SYS_GetPGDir( padir ) );
	IB_AddSource( pgdir, SOURCE_DISK );


	//
	// parse init.gsh. found in HOME/tgfkaa/GAME or ./GAME
	// in init.gsh the user may set his own, game dependent, configuration.
	// normaly archXX.sar ..., are added here.
	handle = IB_Open( "init.gsh" );
	SHP_ParseFile( handle );
	IB_Close( handle );

	IB_DumpLastSar( "/scratch/arch00.sar/");

//	InitZones();

	//
	// endof low level configuration



	{
		g_rs = G_NewResources();
		G_ResourceTypeRegister( g_rs, "tga", Res_RegisterTGA, Res_UnregisterTGA, Res_CacheTGA, Res_UncacheTGA );
		G_ResourceTypeRegister( g_rs, "gltex", Res_RegisterGLTEX, Res_UnregisterGLTEX, Res_CacheGLTEX, Res_UncacheGLTEX );
		G_ResourceTypeRegister( g_rs, "submd", Res_RegisterSUBMD, Res_UnregisterSUBMD, Res_CacheSUBMD, Res_UncacheSUBMD );
		G_ResourceTypeRegister( g_rs, "moves", Res_RegisterMOVES, Res_UnregisterMOVES, Res_CacheMOVES, Res_UncacheMOVES );
		G_ResourceTypeRegister( g_rs, "sound", Res_RegisterSound, Res_UnregisterSound, Res_CacheSound, Res_UncacheSound );
		G_ResourceTypeRegister( g_rs, "lump", Res_RegisterLUMP, Res_UnregisterLUMP, Res_CacheLUMP, Res_UncacheLUMP );
		G_ResourceTypeRegister( g_rs, "mp3", Res_RegisterMp3, Res_UnregisterMp3, Res_CacheMp3, Res_UncacheMp3 );
//		G_ResourcesDump( g_rs );
        g_res::manager::get_instance().add_loader<g_res::tag::gltex>( new g_res::loader::gltex() );
        g_res::manager::get_instance().add_loader<g_res::tag::sound>( new g_res::loader::sound() );
        g_res::manager::get_instance().add_loader<g_res::tag::lump>( new g_res::loader::lump() );
	}	

// 	usecs = 0;

	SHI_StartUp();

	handle = IB_Open( "config.gsh" );
	SHP_ParseFile( handle );
	IB_Close( handle );

	__named_message( "register 'sound_res' resources\n" );
	G_ResourceFromClass( g_rs, "res/sound_res.hobj" );
    g_res::manager::get_instance().init_from_res_file( "res/sound_res.hobj" );


	SND_StartUp();

	

	SHV_StartUp();  // shell visual
	SHM_StartUp();



	config_valid = 1;
	
//	GL_StartTimer( 100000 );



	
//    R_StartUp( q );


    pan::gl_context gl_ctx;
    {
        r_devicewidth = SHP_GetVar ( "r_devicewidth" );
        r_deviceheight = SHP_GetVar ( "r_deviceheight" );
        auto r_fullscreen = SHP_GetVar ( "r_fullscreen" );

        pan::gl_context::config cfg;

        cfg.width_ = r_devicewidth->ivalue;
        cfg.height_ = r_deviceheight->ivalue;

        gl_ctx.set_config(cfg);

        r_glinfo = ( gl_info_t * ) MM_Malloc ( sizeof ( gl_info_t ) );
        r_glinfo->arb_multitexture = 1;
        r_glinfo->texenv_units = 1;
        r_glinfo->texenv_have_add = 1;
    }
	signal( SIGSEGV, (void(*) (int)) SecureShutDown );
	signal( SIGABRT, (void(*) (int)) SecureShutDown );
	signal( SIGTERM, (void(*) (int)) SecureShutDown );
//       signal( SIGQUIT, (void(*) ()) SecureShutDown );
	signal( SIGINT,  (void(*) (int)) SecureShutDown );
	signal( SIGFPE,  (void(*) (int)) SecureShutDown );




	LA_StartUp();
	HUD_StartUp();
	GC_LoadGraphCursor();

    //R_Init();


//	__chkptr( tga );

	
	__named_message( "add resources: models/testobj.d/resources.hobj\n" );
	G_ResourceFromClass( g_rs, "models/testobj.d/resources.hobj" );

	__named_message( "add resources: models/testguy1.d/resources.hobj\n" );
	G_ResourceFromClass( g_rs, "models/testguy1.d/resources.hobj" );

	__named_message( "add resources: models/rocket1.d/resources.hobj\n" );
	G_ResourceFromClass( g_rs, "models/rocket1.d/resources.hobj" );

	__named_message( "add resources: models/rlaunch1.d/resources.hobj\n" );
	G_ResourceFromClass( g_rs, "models/rlaunch1.d/resources.hobj" );


	__named_message( "add resources: res/gltex_res.hobj\n" );
	G_ResourceFromClass( g_rs, "res/gltex_res.hobj" );

	__named_message( "add resources: res/resources.hobj\n" );
	G_ResourceFromClass( g_rs, "res/resources.hobj" );

	__named_message( "add resources: res/fx_res.hobj\n" );
	G_ResourceFromClass( g_rs, "res/fx_res.hobj" );


    g_res::manager::get_instance().init_from_res_file( "res/gltex_res.hobj" );
    g_res::manager::get_instance().init_from_res_file( "res/resources.hobj" );
    g_res::manager::get_instance().init_from_res_file( "res/fx_res.hobj" );
    g_res::manager::get_instance().init_from_res_file( "res/hud.hobj" );
    
#if 0
	{
		ib_file_t *h;
		unsigned int i1, i2;

		h = IB_Open( "misc/lfont.lmp" );

		i1 = IB_GetInt( h );
		i2 = IB_GetInt( h );
		
		
		printf( "i1: %d i2: %d\n", i1, i2 );
	
		IB_Close( h );
	}
#endif

    DD_LOG << "test log\n";
//    __error("");
    GC_MainLoop( gl_ctx );
    SaveConfig();
	TFUNC_LEAVE;
    return 0;
}

