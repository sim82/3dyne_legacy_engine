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



// g_resources.h

#ifndef __g_resources
#define __g_resources
#include <stdexcept>
#include <cassert>
#include <map>
#include <string>
#include <vector>

#include <boost/intrusive/list.hpp>

#include "g_resourcesdefs.h"
#include "shock.h"
namespace g_res {

namespace tag {
    class gltex;
    class sound;
    class lump;
}

namespace resource {
class base : public boost::intrusive::list_base_hook<> {
public:
    virtual size_t type_id() const = 0;
    virtual ~base() {}
private:
    
};

class gltex : public base {
    virtual size_t type_id() const ;
};

class sound : public base {
    virtual size_t type_id() const ;
};

class lump : public base {
    virtual size_t type_id() const ;
};

}



template<typename tag>
class traits {
    
};

template<>
class traits<tag::gltex> {
public:
    typedef resource::gltex type;
    const static size_t id = 1;
    const static char *name;// = "gltex";
};

template<>
class traits<tag::sound> {
public:
    typedef resource::sound type;
    const static size_t id = 2;
    const static char *name;// = "sound";
};

template<>
class traits<tag::lump> {
public:
    typedef resource::lump type;
    const static size_t id = 3;
    const static char *name;// = "sound";
};


namespace loader {
class base {
public:
    virtual resource::base *make( hobj_t *obj ) = 0;
    virtual void unmake( resource::base * ) = 0;
    virtual void cache( resource::base * ) = 0;
    virtual void uncache( resource::base * ) = 0;
    
    
};

class gltex : public base {
public:
    virtual resource::base *make( hobj_t *obj ) {
        return new resource::gltex();
    }
    virtual void unmake( resource::base * ) {
    }
    virtual void cache( resource::base * ) {
        __named_message( "\n" );
    }
    virtual void uncache( resource::base * ) {
    }
};

class sound : public base {
public:
    virtual resource::base *make( hobj_t *obj ) {
        return new resource::sound();
    }
    virtual void unmake( resource::base * ) {
    }
    virtual void cache( resource::base * ) {
        __named_message( "\n" );
    }
    virtual void uncache( resource::base * ) {
    }
};

class lump : public base {
public:
    virtual resource::base *make( hobj_t *obj ) {
        return new resource::lump();
    }
    virtual void unmake( resource::base * ) {
    }
    virtual void cache( resource::base * ) {
        __named_message( "\n" );
    }
    virtual void uncache( resource::base * ) {
    }
};
}


class manager {
    typedef std::map<std::string, resource::base *> res_map;    
public:
    
    class scope_guard {
    public:
        scope_guard( manager *mgr ) : mgr_(mgr) {
            id_ = mgr_->push_scope();
        }
        
        ~scope_guard() {
            size_t id = mgr_->pop_scope();
            
            if( id != id_ ) {
                throw std::runtime_error( "scope id mismatch on pop" );
            }
        }
        
    private:
        manager *mgr_;
        size_t id_;
    };
    
    
    manager() {
        push_scope();
        
    }
    ~manager() {
        size_t id = pop_scope();
        if( id != 0 ) {
            __named_message( "could not pop root scope. possible resource leak.\n" );
        }
    }
    
    
    
    void init_from_res_obj( hobj_t *hobj ) ;
    void init_from_res_file( const char *filename ) ;
    void dump_scopes() ;
    
    template<typename TAG>
    void add_loader( loader::base *loader ) {
        if( loader_.size() <= traits<TAG>::id ) {
            loader_.resize( traits<TAG>::id + 1 );
            loader_names_.resize( traits<TAG>::id + 1 );
        }
        
        assert( loader_[traits<TAG>::id] == 0 );
        loader_[traits<TAG>::id] = loader;
        loader_names_[traits<TAG>::id] = traits<TAG>::name;
    }
    
    inline size_t loader_id( const char *type ) {
        for( size_t i = 0; i < loader_names_.size(); ++i ) {
//             std::cout << "loader: " << loader_names_[i] << "\n";
            
            if( !loader_names_[i].empty() && loader_names_[i] == type ) {
                return i;
            }
        }
        
        return -1;
    }
    
    resource::base *get_unsafe( const char *name ) ;
    
    template<typename TAG> 
    typename traits<TAG>::type *get( const char * name ) {
        resource::base *res = get_unsafe( name );
        if( res->type_id() == traits<TAG>::id ) {
            return static_cast<typename traits<TAG>::type *>(res);
        } else {
            throw std::runtime_error( "wrong res type" );
        }
    }
    
    void uncache( resource::base *res ) {
        loader_[res->type_id()]->uncache( res );
    }
    
    size_t push_scope() {
        scope_internal *s = new scope_internal( scope_stack_.size() );
        
        scope_stack_.push_back(s);
        
        return s->scope_id();
    }
    
    size_t pop_scope() {
        scope_internal *s = scope_stack_.back();
        scope_stack_.pop_back();
        
        
        size_t id = s->scope_id();
        delete s;
        
        return id;
     
    }
    
    static manager &get_instance() ;
    
private:
    manager( const manager & ) {}
    manager &operator=( const manager & ) { return *this; }
    
    res_map res_;
    std::vector<loader::base *> loader_;
    std::vector<std::string> loader_names_;
    class scope_internal {
    public:
    
        scope_internal( size_t id ) : mgr_(0), scope_id_(id) {}
        
        
        ~scope_internal();
        
        size_t scope_id() const {
            return scope_id_;
        }
    private:
//         void swap( scope_internal &other ) {
//             std::swap( mgr_, other.mgr_ );
//             list_.swap( other.list_ );
//             
//         }
        
        void add( resource::base *res ) {
            if( !res->is_linked() ) {
                list_.push_back( *res );
            }
        }
        
        friend class manager;
        
        manager *mgr_;
        boost::intrusive::list<resource::base> list_;    
        size_t scope_id_;
    };
    
    std::vector<scope_internal *>scope_stack_;
   
    
    
};



}


g_resources_t * G_NewResources( void );
void G_FreeResources( g_resources_t *res );
void G_ResourcesDump( g_resources_t *res );
void G_ResourceDump( g_resource_t *res );

void G_ResourceTypeRegister( g_resources_t *rp, const char *type_name,
			     g_resource_t * (*register_func)(hobj_t *resobj),
			     void (*unregister_func)(g_resource_t *res),
			     void (*chach_func)(g_resource_t *res),
			     void (*uncache_func)(g_resource_t *res) );

g_resource_t * G_NewResource( char *res_name, char *res_type );

void G_ResourceFromResObj( g_resources_t *res, hobj_t *cls );
void G_ResourceFromClass( g_resources_t *res, const char *name );

bool_t G_ResourceCheck( g_resources_t *res, char *name );
g_resource_t * G_ResourceSearch( g_resources_t *res, const char *name );
g_resource_t * G_ResourceAttach( g_resources_t *res, char *name, char *user_name, int (*user_callback_func)(g_resource_t *r, int reason) );
void G_ResourceDetach( g_resources_t *res, char *name, char *user_name );

void G_CallResourceUser( g_res_user_t *ru, g_resource_t *r, int reason );
void G_CallResourceUsers( g_resource_t *r, int reason );

void G_ResourcesForEach( g_resources_t *res, void (*action_func)(g_resource_t *r) );

#endif
