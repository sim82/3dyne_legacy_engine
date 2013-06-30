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
#include <GL/gl.h>
#include "message_passing.h"
#include "i_defs.h"
#include <tuple>
#include <memory>

class mipmap_cache;

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
class print_queue_profiling : public base {};

class menu_setpage : public base {};
class game_shell_execute : public base {
public:
    game_shell_execute( const std::string &script ) : script_(script) {}
    std::string script_;

};

#if 0
class gl_upload_texture : public base {
public:
    gl_upload_texture( GLuint t, GLuint w, GLuint h, GLenum pixel_format, GLint mip_level, GLint max_level, std::vector<uint8_t> && data )
        : t_(t),
          w_(w),
          h_(h),
          pixel_format_(pixel_format),
          mip_level_(mip_level),
          max_level_(max_level),
          data_(std::move(data))
    {

    }

    GLuint t_;
    GLuint w_;
    GLuint h_;
    GLenum pixel_format_;
    GLint mip_level_;
    GLint max_level_;
    std::vector<uint8_t> data_;
};
#endif


class gl_upload_texture : public base {
public:
    gl_upload_texture( GLuint t, std::shared_ptr<mipmap_cache> mc, GLenum pixel_format, GLint mip_level, GLint max_level )
        : t_(t),
          mc_(mc),
          pixel_format_(pixel_format),
          mip_level_(mip_level),
          max_level_(max_level)
    {}

    GLuint t_;
    std::shared_ptr<mipmap_cache> mc_;
    GLenum pixel_format_;

    GLint mip_level_;
    GLint max_level_;

};



class test_tuple : public base {
public:
    typedef std::tuple<GLuint,GLuint,GLuint,GLint,GLint,std::vector<uint8_t>> data_type;

    test_tuple( data_type && data )
        : data_( std::move(data)) {}

     data_type data_;
};

}

//class g_global_mp {
//public:
    
//    g_global_mp() : queue_( "main" ), bg_queue_( "bg" ) {}
    
//    mp::queue &get_queue() {
//        return queue_;
//    }
    
//    mp::queue &get_bg_queue() {
//        return bg_queue_;
//    }
    
    
//    static g_global_mp *get_instance();
    
//    std::mutex gl_mtx_;
//private:
//    mp::queue queue_;
//    mp::queue bg_queue_;
    
    
//};

#endif
