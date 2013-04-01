
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <cstring>
#include <set>
#include "shock.h"
#include "rel_ptr.h"
#include "u_hobj.h"
#include "interfaces.h"


class inc_alloc;

static void call_dyn_constructor( size_t class_id, void *ptr, inc_alloc &alloc );

class base_dyn {
public:
    virtual ~base_dyn() {}
    
};


class inc_alloc {
    inc_alloc( inc_alloc & ) {}
    bool static_phase_;
public:
    
    typedef std::pair<size_t,void*> id_ptr_pair;
    std::vector<id_ptr_pair> const_constructor_list_;
    std::vector<id_ptr_pair> dyn_constructor_list_;
    
    inc_alloc( size_t size ) : buf_(size), iter_(buf_.begin()) {
        
        header_ = alloc_raw<header>();
        
        
//         std::cout << "inc_alloc: " << dyn_base_list_ << " " << dyn_base_num_ << "\n";
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
    T* alloc_raw() {
        size_t s = sizeof(T);
        
        char *ptr = &(*iter_);
        iter_ += s;
        
        if( iter_ > buf_.end() ) {
            throw std::runtime_error( "out of memory" );
            
        }
//         std::cout << "size: " << s << "\n";
        

        return (T*)ptr;
    }
    
    template<typename T>
    T* alloc() {
        size_t s = sizeof(T);
        
        char *ptr = &(*iter_);
        iter_ += s;
        
        if( iter_ > buf_.end() ) {
            throw std::runtime_error( "out of memory" );
            
        }
//         std::cout << "size: " << s << "\n";
        size_t class_id = T::class_id;
        if( class_id < 1024 ) {
            const_constructor_list_.push_back( std::make_pair( class_id, (void *)ptr ));
        } else {
            dyn_constructor_list_.push_back( std::make_pair( class_id, (void *)ptr ));
        }


        return (T*)ptr;
    }
    
    char *dup_cstr( const char *str ) {
        
        string_dict_type::iterator it = string_dict_.find(const_cast<char *>(str)); // hmm hmm
        
        if( it != string_dict_.end() ) {
            return *it;
        }
            
        
        size_t len = strlen(str);
        
        
        size_t s = len + 1;
        
        iter_ = align_forward( iter_ );
        
        char *ptr = &(*iter_);
        iter_ += s;
        
        if( iter_ > buf_.end() ) {
            throw std::runtime_error( "out of memory" );
        }
        
        
        strcpy( ptr, str );
        
        string_dict_.insert( ptr );
        
        return ptr;
        
    }
    
    const std::vector<char> &buf() const {
        return buf_;
    }
    
    std::vector<char>::iterator iter() const {
        return iter_;
    }
    
    std::vector<char>::iterator begin() {
        return buf_.begin();
    }
    
    template<size_t alignment, typename T>
    inline bool is_aligned( const T &v ) {
        return (size_t(&(*v)) % alignment) == 0;
    }
//     template<typename T>
//     inline bool is_aligned( const T &v ) {
//         return is_aligned
//     }
    
    
    
    template<size_t alignment, typename T>
    inline T align_forward( const T &v ) {
        T vout = v;
        while( !is_aligned<alignment>(vout) ) {
            ++vout;
        }
        
        return vout;
    }
    
    template<typename T>
    inline T align_forward( const T &v ) {
        return align_forward<16, T>(v);
    }
    
    void call_const_constructors() {
        static_phase_ = false;
        
        iter_ = align_forward<4096>(iter_);
        for( std::vector< id_ptr_pair >::iterator it = const_constructor_list_.begin(); it != const_constructor_list_.end(); ++it ) {
            call_dyn_constructor( it->first, it->second, *this );
            
        }
    }
    
    void call_dyn_constructors() {
        
        iter_ = align_forward<4096>(iter_);
        for( std::vector< id_ptr_pair >::iterator it = dyn_constructor_list_.begin(); it != dyn_constructor_list_.end(); ++it ) {
            call_dyn_constructor( it->first, it->second, *this );
        }
        
        iter_ = align_forward<4096>(iter_);
        
        std::cout << "dyn const size: " << dyn_constructor_list_.size() << "\n";
//         std::cout << "dist: " << ssize_t(ssize_t(dyn_base_list_))  -  (ssize_t(&(*iter_)) ) << "\n";
        
        header_->dyn_base_list_.reset((rel_ptr<base_dyn>*)(&(*iter_)));
        header_->dyn_base_num_ = dyn_constructor_list_.size();
        
//         (*dyn_base_num_
        
        for( std::vector< id_ptr_pair >::iterator it = dyn_constructor_list_.begin(); it != dyn_constructor_list_.end(); ++it ) {
            rel_ptr<base_dyn> *pp = alloc_raw<rel_ptr<base_dyn> >();
            new (pp) rel_ptr<base_dyn>((base_dyn*)it->second);
            
        }
        
        
    }
    
    
    void call_dyn_destructors() {
        
        header *h = (header*)(&(*begin()));
        
        rel_ptr<base_dyn> *list = h->dyn_base_list_.get();
        uint32_t size = h->dyn_base_num_;
        

        std::cout << "destructor list size: " << size << "\n";
        
        for( uint32_t i = 0; i < size; ++i ) {
            rel_ptr<base_dyn> *ptr = reinterpret_cast<rel_ptr<base_dyn>*>(list + i);
            std::cout << "ptr: " << ptr->get() << "\n";
            ptr->get()->~base_dyn();
        }
        getchar();        
//         for( std::vector< id_ptr_pair >::iterator it = dyn_constructor_list_.begin(); it != dyn_constructor_list_.end(); ++it ) {
//             //    call_dyn_constructor( it->first, it->second, *this );
//             (reinterpret_cast<base_dyn*>(it->second))->~base_dyn();
//         }
    }
private:
    class cstr_less {
    public:
        inline bool operator()( const char *a, const char *b ) {
            return strcmp( a, b ) < 0;
        }
    };
    
    size_t num_;
    std::vector<char> buf_;
    std::vector<char>::iterator iter_;
    
    typedef std::set<char *, cstr_less> string_dict_type;
    string_dict_type string_dict_;
    
    struct header {
        rel_ptr<rel_ptr<base_dyn> > dyn_base_list_;
        uint32_t dyn_base_num_;
        
    };
    header *header_;
    
};






class hpair;

class hpair_dyn: public base_dyn {
public:
    const static size_t class_id = 1024 + 1;
    
    hpair_dyn(hpair *p) :  v_(0xdeadbeef), p_(p) {}
    
    hpair_dyn() {}
    
    
    virtual ~hpair_dyn() {
        __named_message("\n");
        
    }
    
    uint32_t v_;
    rel_ptr<hpair> p_;
};

class hpair {
public:
    const static size_t class_id = 1;
//     char        type[HPAIR_TYPE_SIZE];
//     char        key[HPAIR_KEY_SIZE];
    rel_ptr<char> type;
    rel_ptr<char> key;
    
//  char        value[HPAIR_VALUE_SIZE];
    rel_ptr<char>  value;
    
    rel_ptr<hpair> next;
    
    rel_ptr<hpair_dyn> dyn_test;
    
    void const_contructor( inc_alloc &inc ) {
        __named_message("%p\n", this);
        
        dyn_test.reset( new(inc.alloc<hpair_dyn>()) hpair_dyn(this));
        //dyn_test.reset( inc.alloc<hpair_dyn>() );
    }
    
};



class hobj
{
public:
    const static size_t class_id = 2;
    
//     char        type[HOBJ_TYPE_SIZE];
//     char        name[HOBJ_NAME_SIZE];
    
    rel_ptr<char> type;
    rel_ptr<char> name;
    
    rel_ptr<hpair>  pairs;     // list
    rel_ptr<hobj>   hobjs;     // list

    rel_ptr<hobj>  parent;
    rel_ptr<hobj>   next;

    void const_contructor( inc_alloc &inc ) {
        __named_message("%p\n", this);
    }
    
    //void        *extra;     // a pointer for user class rep
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
static void call_dyn_constructor( size_t class_id, void *ptr, inc_alloc &alloc ) {
    
    switch( class_id ) {
    case 1:
        (reinterpret_cast<hpair*>(ptr))->const_contructor(alloc);
        
        break;
        
    case 2:
        (reinterpret_cast<hobj*>(ptr))->const_contructor(alloc);
        break;
        
        
    case (1024 + 1):
        //(reinterpret_cast<hpair_dyn*>(ptr))->dynamic_contructor(alloc);
        new (ptr)hpair_dyn;
        
        break;
    default:
        std::cerr << "class_id: " << class_id << "\n";
        throw std::runtime_error( "class_id not found" );
    }
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
    std::vector<char>::iterator it1 = xalloc.begin();
    
    hobj *ow = copy_hobj( o, 0, xalloc );
    xalloc.call_const_constructors();
    xalloc.call_dyn_constructors();
//     xalloc.call_
    getchar();
    xalloc.call_dyn_destructors();
    
    
    print_hobj( ow, 0 );
    
    std::cout << "baked size: " << std::distance( it1, xalloc.iter() ) << "\n";
    
    std::ofstream os( "baked.bin", std::ios::binary );
    
    os.write( &(*it1), std::distance( it1, xalloc.iter() ) );
    
    getchar();
    
}