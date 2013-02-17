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

namespace g_res {

namespace tag {
    class gltex;
    class sound;
}

namespace resource {
class base : public boost::intrusive::list_base_hook<> {
public:
    virtual size_t type_id() const = 0;
private:
    
};

class gltex : public base {
    virtual size_t type_id() const ;
};

class sound : public base {
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



namespace loader {
class base {
public:
    virtual resource::base *make( hobj_t *obj ) = 0;
    virtual void unmake( resource::base * ) = 0;
    virtual void cache( resource::base * ) = 0;
    virtual void uncache( resource::base * ) = 0;
    
    
};



}


class manager {
    typedef std::map<std::string, resource::base *> res_map;    
public:
    
    class scope {
    public:
    
        scope() : mgr_(0) {}
        
        scope( manager *mgr ) : mgr_(mgr) {
            mgr_->push_scope(this);
            
        } 
        
        ~scope();
    private:
        void swap( scope &other ) {
            std::swap( mgr_, other.mgr_ );
            list_.swap( other.list_ );
            
        }
        
        void add( resource::base *res ) {
            if( !res->is_linked() ) {
                list_.push_back( *res );
            }
        }
        
        friend class manager;
        
        manager *mgr_;
        boost::intrusive::list<resource::base> list_;
    };
    
    
    manager() {
        scope bs( this );
        base_scope.swap( bs );
        
    }
    
    
    manager &get_instance() {
        static manager mgr;
        
        return mgr;
    }
    
    void init_from_res_obj( hobj_t *hobj ) ;
    void init_from_res_file( const char *filename ) ;
    
    
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
            if( !loader_names_[i].empty() && loader_names_[i] == type ) {
                return i;
            }
        }
        
        return -1;
    }
    
    resource::base *get_unsafe( const char *name ) {
        res_map::iterator it = res_.find( std::string( name ) );
        
        if( it == res_.end() ) {
            throw std::runtime_error( "resource not found" );
        }
        resource::base *res = it->second;
        loader_[res->type_id()]->cache( res );
        
        return res;
        
    }
    
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
    
    void push_scope( scope *s ) {
        scope_stack_.push_back(s);
    }
    
    scope *pop_scope() {
        scope *s = scope_stack_.back();
        scope_stack_.pop_back();
        
        return s;
    }
    
private:
    manager( const manager & ) {}
    manager &operator=( const manager & ) { return *this; }
    
    res_map res_;
    std::vector<loader::base *> loader_;
    std::vector<std::string> loader_names_;
    
    
    std::vector<scope *>scope_stack_;
    scope base_scope;
    
    
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
