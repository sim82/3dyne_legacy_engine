/* 
 * 3dyne Legacy Engine GPL Source Code
 * 
 * Copyright (C) 2013 Matthias C. Berger & Simon Berger.
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


#ifndef __g_message_passing_h
#define __g_message_passing_h

#include "message_passing.h"
#include "i_defs.h"

namespace msg {
class key_event : public base {
public:
    key_event( const keyevent_t &event ) : event_(event) {}    
    keyevent_t event_;
};
    
class mouse_event : public base {
public:
    mouse_event( int md_x, int md_y, int ts ) : md_x_(md_x), md_y_(md_y), ts_(ts) {}
    
    int md_x_;
    int md_y_;
    
    int ts_;
    
};

class gc_quit : public base {};
class gc_start_single : public base {};
class gc_drop_game : public base {};
class gc_start_demo : public base {};

class world_frame : public base {};

class client_frame : public base {};
class swap_buffer : public base {};

}

class g_global_mp {
public:
    
    
    mp::queue &get_queue() {
        return queue_;   
    }
    
    mp::queue &get_bg_queue() {
        return bg_queue_;   
    }
    
    
    static g_global_mp *get_instance();
    
    std::mutex gl_mtx_;
private:
    mp::queue queue_;
    mp::queue bg_queue_;
    
    
};

#endif