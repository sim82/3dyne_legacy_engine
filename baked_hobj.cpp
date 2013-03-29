
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <cstring>
#include "rel_ptr.h"
#include "u_hobj.h"
#include "interfaces.h"
#if 0

class hobj_oven {
public:
    hobj_oven( hobj_t *root ) {
        size_t num_obj = 0;
        size_t num_pair = 0;
        
        count( root, num_obj, num_pair );
        
        std::cout << "count: " << num_obj << " " << num_pair << "\n";
    }
    
    void count_pairs( hpair_t *p, size_t &num_hpair ) {
        
        if( p != 0 ) {
            ++num_hpair;
            count_pairs( p->next, num_hpair );
        }
    }
    
    void count( hobj_t *o, size_t &num_hobj, size_t &num_hpair ) {
        if( o != 0 ) {
            ++num_hobj;
            count_pairs( o->pairs, num_hpair );
            count( o->hobjs, num_hobj, num_hpair );
            count( o->next, num_hobj, num_hpair );
        }
    }
   
   
#if 1 
    size_t write_hpair( hpair_t *p ) {
        if( p == 0 ) {
            return 0;
        }
        std::cout << "hpair\n";
        // TODO: writing the value in front of the hpair for simplicity. check if that is good performance wise (i.e. confuses prefetcher too much etc.)
        
        size_t value_base = buf_.size();
        
        char *v = p->value;
        while( (*v) != 0 ) {
            buf_.push_back( *v );
            ++v;
        }
        buf_.push_back( 0 );
        
        while( !is_aligned( buf_.size() ) ) {
            buf_.push_back( 0 );
        }
        
        size_t base = buf_.size();
        
        buf_.resize( base + sizeof( hpair_t ) );
        
        hpair_t wp = *p;
        wp.value = (char *)value_base;

        reloc_.push_back( base + size_t( &((hpair_t *)0)->value ));
        reloc_.push_back( base + size_t( &((hpair_t *)0)->next ));
        
        wp.next = (hpair_t*)write_hpair( p->next );
        
        std::copy( (char *)&wp, (char *)((&wp)+1), buf_.begin() + base );
        
        return base;
    }
    
    size_t write_hobj( hobj_t *o, hobj_t *parent ) {
        if( o == 0 ) {
            return 0;
        }
        std::cout << "hobj\n";
        while( !is_aligned( buf_.size() ) ) {
            buf_.push_back( 0 );
        }
        
        size_t base = buf_.size();
        
        buf_.resize( base + sizeof( hobj_t ) );
        
        hobj_t ow = *o;
        ow.pairs = (hpair_t*)write_hpair( o->pairs );
        ow.hobjs = (hobj_t*)write_hobj( o->hobjs, 0 );
        
        ow.parent = parent;
        ow.next = (hobj_t*)write_hobj( o->next, (hobj_t*)base );
        
        reloc_.push_back( base + size_t( &((hobj_t *)0)->pairs ));
        reloc_.push_back( base + size_t( &((hobj_t *)0)->hobjs ));
        reloc_.push_back( base + size_t( &((hobj_t *)0)->next ));
        
        
        std::copy( (char *)&ow, (char *)((&ow)+1), buf_.begin() + base );
        
        return base;
    }
#endif

    void write( std::ostream &os ) {
        os.write( buf_.data(), buf_.size() );
        
        os.write( (char*)reloc_.data(), reloc_.size() * sizeof( size_t ) );
        
    }

private:
    template<typename T>
    bool is_aligned( const T& v ) {
        const size_t alignment = 4;
        return (v % alignment) == 0;
    }
    
    std::vector<char> buf_;
    std::vector<size_t> reloc_;
};


void bake_hobj( hobj_t *o ) {
    hobj_oven oven(o);
    oven.write_hobj(o, 0);
    
    {
        std::ofstream os( "baked.bin" );
        oven.write( os );
    }
    
    getchar();
    
    
}

#endif


class hpair {
public:
//     char        type[HPAIR_TYPE_SIZE];
//     char        key[HPAIR_KEY_SIZE];
    rel_ptr<char> type;
    rel_ptr<char> key;
    
//  char        value[HPAIR_VALUE_SIZE];
    rel_ptr<char>  value;
    
    rel_ptr<hpair> next;
};



class hobj
{
public:
//     char        type[HOBJ_TYPE_SIZE];
//     char        name[HOBJ_NAME_SIZE];
    
    rel_ptr<char> type;
    rel_ptr<char> name;
    
    rel_ptr<hpair>  pairs;     // list
    rel_ptr<hobj>   hobjs;     // list

    rel_ptr<hobj>  parent;
    rel_ptr<hobj>   next;

    //void        *extra;     // a pointer for user class rep
};



class inc_alloc {
    inc_alloc( inc_alloc & ) {}
    
public:
    
    
    
    inc_alloc( size_t size ) : buf_(size), iter_(buf_.begin()) {
        
        
    }
    
//     void *alloc( size_t s ) {
//         char *ptr = &(*iter_);
//         iter_ += s;
//         
//         if( iter_ > buf_.end() ) {
//             throw std::runtime_error( "out of memory" );
//             
//         }
//         
//         return ptr;
//     }
    
    
    template<typename T>
    T* alloc() {
        size_t s = sizeof(T);
        
        char *ptr = &(*iter_);
        iter_ += s;
        
        if( iter_ > buf_.end() ) {
            throw std::runtime_error( "out of memory" );
            
        }
//         std::cout << "size: " << s << "\n";
        return (T*)ptr;
    }
    
    char *dup_cstr( const char *str ) {
        size_t len = strlen(str);
        
        
        size_t s = len + 1;
        
        char *ptr = &(*iter_);
        iter_ += s;
        
        if( iter_ > buf_.end() ) {
            throw std::runtime_error( "out of memory" );
        }
        
        
        strcpy( ptr, str );
        
        return ptr;
        
    }
    
    const std::vector<char> &buf() const {
        return buf_;
    }
    
    std::vector<char>::iterator iter() const {
        return iter_;
    }
    
    
private:
    size_t num_;
    std::vector<char> buf_;
    std::vector<char>::iterator iter_;
    
};





hpair *copy_hpair( hpair_t *p, inc_alloc &xalloc ) {
    hpair *ow = new (xalloc.alloc<hpair>() ) hpair;
    
//     memcpy( ow->type, p->type, sizeof( ow->type ));
//     memcpy( ow->key, p->key, sizeof( ow->key ));
    
    ow->type.reset(xalloc.dup_cstr( p->type ));
    ow->key.reset(xalloc.dup_cstr( p->key ));
    
    
    ow->value.reset( xalloc.dup_cstr(p->value) );
    
    if( p->next != 0 ) {
        ow->next.reset( copy_hpair( p->next, xalloc ));
    }
    
    return ow;
}

hobj *copy_hobj( hobj_t *o, hobj *parent, inc_alloc &xalloc ) {
 
    hobj *ow = new (xalloc.alloc<hobj>() ) hobj;
//     memcpy( ow->type, o->type, sizeof( ow->type ));
//     memcpy( ow->name, o->name, sizeof( ow->name ));
    
    ow->type.reset(xalloc.dup_cstr( o->type ));
    ow->name.reset(xalloc.dup_cstr( o->name ));
    
    if( o->pairs != 0 ) {
        ow->pairs.reset(copy_hpair( o->pairs, xalloc ));
    }
    
    if( o->hobjs != 0 ) {
        ow->hobjs.reset(copy_hobj( o->hobjs, ow, xalloc ));
    }
 
    if( o->next != 0 ) {
        ow->next.reset(copy_hobj(o->next, ow, xalloc ));
    }
    
    if( parent != 0 ) {
       ow->parent.reset( parent );
    }
    
    return ow;
}



void print_hpairs( hpair *p, int indent ) {
    while( p != 0 ) {
        for( int i = 0; i < indent; ++i ) {
            std::cout << " ";
        }
        
        
        std::cout << p->key.get() << " = " << p->value.get() << "\n";
        
        if( p->next.valid() ) {
            p = p->next.get();
        } else {
            p = 0;
        }
    }
}

void print_hobj( hobj *o, int indent ) {
    
    
    while( o != 0 ) {
        
        if( o->pairs.valid() ) {
            print_hpairs( o->pairs.get(), indent + 1 );
        }
        
        if( o->hobjs.valid() ) {
            print_hobj( o->hobjs.get(), indent + 1 );
        }
    
    
        if( o->next.valid() ) {
            o = o->next.get();
        } else {
            o = 0;
        }
    }
}

void bake_hobj( hobj_t *o ) {
    inc_alloc xalloc( 1024 * 1024 * 16);
    std::cout << "parent: " << o->parent << "\n";
    std::vector<char>::iterator it1 = xalloc.iter();
    
    hobj *ow = copy_hobj( o, 0, xalloc );
    
    print_hobj( ow, 0 );
    
    std::cout << "baked size: " << std::distance( it1, xalloc.iter() ) << "\n";
    
    std::ofstream os( "baked.bin", std::ios::binary );
    
    os.write( &(*it1), std::distance( it1, xalloc.iter() ) );
    
    getchar();
    
}