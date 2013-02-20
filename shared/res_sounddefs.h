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



// res_sounddefs.h

#ifndef __res_sounddefs
#define __res_sounddefs

typedef struct res_sound_register_s
{
	char	path[32];	// variable sized
} res_sound_register_t;

typedef struct res_sound_cache_s
{
	int		size;

	unsigned int	al_buf;

	unsigned char	*sound;
} res_sound_cache_t;

namespace g_res {
namespace tag {
     class sound;
}


template<typename tag>
class traits;


template<>
class traits<tag::sound> {
public:
//     typedef resource::sound type;
    typedef res_sound_register_t reg_state;
    typedef res_sound_cache_t cache_state;


    const static size_t id = 2;
    const static char *name;// = "sound";
};
} // namespace g_res
#endif
