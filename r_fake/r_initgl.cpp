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



// r_initgl.c
#include "sh_parser.h"
#include "render.h"

gl_extensions_t		gl_ext;	// available gl extensions

static sh_var_t *size_x = NULL, *size_y = NULL;
 
void R_InitGL_rendering( void )
{
//	printf( "R_InitGL:\n" );

//	static int old_size_x = 0;
//	static int old_size_y = 0;

	if( !size_x )
	{
		size_x = SHP_GetVar( "r_devicewidth" );
		size_y = SHP_GetVar( "r_deviceheight" );

		glViewport( 0, 0, (GLint)size_x->ivalue, (GLint)size_y->ivalue );
	}

	glDisable(GL_CULL_FACE);
	glFrontFace( GL_CW );
	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
	
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glMatrixMode( GL_MODELVIEW );
       	glLoadIdentity();	

	gl_ext.have_arb_multitexture = false;


}
