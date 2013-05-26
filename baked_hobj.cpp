#include "compiler_config.h"
#if !D3DYNE_OS_WIN

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
#include "minimal_external_hash.h"

class inc_alloc;




class base_dyn {
public:
    virtual ~base_dyn() {
        __named_message( "\n" );
    }
    
};


typedef std::vector<base_dyn*> auto_destructor_list_type;

static void call_const_allocator( size_t class_id, void *ptr, inc_alloc & );
static void call_const_initializer( size_t class_id, void *ptr, auto_destructor_list_type &dest_list );

class inc_alloc {
    inc_alloc( inc_alloc & ) {}
    bool static_phase_;
public:
    
    typedef std::pair<size_t,void*> id_ptr_pair;
    std::vector<id_ptr_pair> const_constructor_list_;
    
    
    inc_alloc( const char *name ) {
     
        std::ifstream is( name, std::ios_base::binary );
        
        if( !is.good() ) {
            throw std::runtime_error( "cannot open file" );
        }
        
        is.seekg( 0, std::ios::end );
        size_t size = is.tellg();
        is.seekg( 0, std::ios::beg );
        
        buf_.resize( size );
        is.read( buf_.data(), size );
        
        iter_ = buf_.begin();
        header_ = reinterpret_cast<header *>(&(*iter_));
        iter_ += sizeof( header );
        
    }
    
    inc_alloc( size_t size ) : buf_(size) {
        
        begin_ = align_forward<4096>( buf_.begin() );
        iter_ = begin_;
        
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
        iter_ = align_forward<8>(iter_);
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
        iter_ = align_forward<8>(iter_);
        char *ptr = &(*iter_);
        iter_ += s;
        
        if( iter_ > buf_.end() ) {
            throw std::runtime_error( "out of memory" );
            
        }
//         std::cout << "size: " << s << "\n";
        size_t class_id = T::class_id;
        if( class_id < 1024 ) {
            const_constructor_list_.push_back( std::make_pair( class_id, (void *)ptr ));
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
        return begin_;
    }
    
    std::vector<char>::iterator end() {
        return buf_.end();
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
    
    void build_const_initializers() {
        iter_ = align_forward<4096>(iter_);
        header_->dyn_base_list_.reset((dyn_list_entry*)(&(*iter_)));
        header_->dyn_base_num_ = const_constructor_list_.size();
        
        
        for( std::vector< id_ptr_pair >::iterator it = const_constructor_list_.begin(); it != const_constructor_list_.end(); ++it ) {
            //call_dyn_constructor( it->first, it->second, *this );
            void *pp = alloc_raw<dyn_list_entry>();
            new (pp) dyn_list_entry(it->first, it->second);
        }
        
        iter_ = align_forward<4096>(iter_);
        header_->dyn_segment_begin_.reset( &(*iter_));
        for( std::vector< id_ptr_pair >::iterator it = const_constructor_list_.begin(); it != const_constructor_list_.end(); ++it ) {
            call_const_allocator( it->first, it->second, *this );
            
        }
        
        header_->dyn_segment_end_.reset( &(*iter_));
        
    }
    
    
    void call_const_initializers(auto_destructor_list_type &dest_list) {
        
        header *h = (header*)(&(*begin()));
        
        dyn_list_entry *list = h->dyn_base_list_.get();
        uint32_t size = h->dyn_base_num_;
        

        std::cout << "initializers list size: " << size << "\n";
        
        for( uint32_t i = 0; i < size; ++i ) {
            dyn_list_entry *ptr = reinterpret_cast<dyn_list_entry*>(list + i);
            //std::cout << "ptr: " << ptr->ptr.get() << "\n";
            call_const_initializer( ptr->class_id, ptr->ptr.get(), dest_list );
        }
    }
    void call_dyn_destructors ( auto_destructor_list_type &dest_list ) {
    
        
        for( auto_destructor_list_type::reverse_iterator it = dest_list.rbegin(); it != dest_list.rend(); ++it ) {
            (*it)->~base_dyn();
            
        }
    }
    
    char *dyn_segment_begin() {
        return header_->dyn_segment_begin_.get();
    }
    
//     void build_const_initializer_list() {
//         header_->dyn_base_list_.reset((dyn_list_entry*)(&(*iter_)));
//         header_->dyn_base_num_ = dyn_constructor_list_.size();
//         
// //         (*dyn_base_num_
//         
//         for( std::vector< id_ptr_pair >::iterator it = dyn_constructor_list_.begin(); it != dyn_constructor_list_.end(); ++it ) {
//             void *pp = alloc_raw<dyn_list_entry>();
//             new (pp) dyn_list_entry(it->first, (base_dyn*)it->second);
//             
//         }   
//     }
    
#if 0
    void call_dyn_constructors() {
        
        iter_ = align_forward<4096>(iter_);
        for( std::vector< id_ptr_pair >::iterator it = dyn_constructor_list_.begin(); it != dyn_constructor_list_.end(); ++it ) {
            call_dyn_constructor( it->first, it->second, *this );
        }
        
        iter_ = align_forward<4096>(iter_);
        
        std::cout << "dyn const size: " << dyn_constructor_list_.size() << "\n";
//         std::cout << "dist: " << ssize_t(ssize_t(dyn_base_list_))  -  (ssize_t(&(*iter_)) ) << "\n";
        
        
        
        
    }

   void call_dyn_constructors() {
        
        header *h = (header*)(&(*begin()));
        
        dyn_list_entry *list = h->dyn_base_list_.get();
        uint32_t size = h->dyn_base_num_;
        

        std::cout << "constructor list size: " << size << "\n";
        
        for( uint32_t i = 0; i < size; ++i ) {
            dyn_list_entry *ptr = reinterpret_cast<dyn_list_entry*>(list + i);
            std::cout << "ptr: " << ptr->ptr.get() << "\n";
            call_dyn_constructor( ptr->class_id, ptr->ptr.get(), *this );
        }
   }

    
    void call_dyn_destructors() {
        
        header *h = (header*)(&(*begin()));
        
        dyn_list_entry *list = h->dyn_base_list_.get();
        uint32_t size = h->dyn_base_num_;
        

        std::cout << "destructor list size: " << size << "\n";
        
        for( uint32_t i = 0; i < size; ++i ) {
            dyn_list_entry *ptr = /*reinterpret_cast<dyn_list_entry*>*/(list + i);
            std::cout << "ptr: " << ptr->ptr.get() << "\n";
            ptr->ptr.get()->~base_dyn();
        }
//         getchar();        
//         for( std::vector< id_ptr_pair >::iterator it = dyn_constructor_list_.begin(); it != dyn_constructor_list_.end(); ++it ) {
//             //    call_dyn_constructor( it->first, it->second, *this );
//             (reinterpret_cast<base_dyn*>(it->second))->~base_dyn();
//         }
    }
    #endif
    
    struct dyn_list_entry {
        uint32_t class_id;
        rel_ptr<void> ptr;
        
        dyn_list_entry( uint32_t id, void *p ) : class_id(id), ptr(p) {}
    };
    
    struct header {
        //rel_ptr<rel_ptr<base_dyn> > dyn_base_list_;
        rel_ptr<dyn_list_entry> dyn_base_list_;
        uint32_t dyn_base_num_;
        rel_ptr<char> dyn_segment_begin_;
        rel_ptr<char> dyn_segment_end_;
        
    };
    

    
private:
    class cstr_less {
    public:
        inline bool operator()( const char *a, const char *b ) {
            return strcmp( a, b ) < 0;
        }
    };
    
    size_t num_;
    std::vector<char> buf_;
    std::vector<char>::iterator begin_;
    std::vector<char>::iterator iter_;
    
    typedef std::set<char *, cstr_less> string_dict_type;
    string_dict_type string_dict_;
    
    
    
    header *header_;
    
};



class mapped_image {
public:
    typedef inc_alloc::header header;
    typedef inc_alloc::dyn_list_entry dyn_list_entry;
    
    mapped_image( const char *name ) : fd_(-1), base_((char*)MAP_FAILED), dyn_base_((char*)MAP_FAILED) {
        fd_ = open( name, O_RDONLY );
        if( fd_ == -1 ) {
            throw std::runtime_error( "cannot open file" );
        }

        size_ = lseek( fd_, 0, SEEK_END );
        lseek( fd_, 0, SEEK_SET );
        
        
        header h;
        
        read( fd_, &h, sizeof(header));
        
        dyn_size_ = std::distance( h.dyn_segment_begin_.get(), h.dyn_segment_end_.get() );
        
        size_t size_all = dyn_size_ + size_;
        
        std::cout << "size: " << dyn_size_ << " + " << size_ << " = " << size_ + dyn_size_ << "\n";;
        
        getchar();
        
//        mf_.map();
        
        
        //
        // this is quite hacky:
        // We need: 
        // (1) const segment, mapped read-only from the baked file
        // (2) dynamic segment, ananonymous read-write
        // (3) the dynamic segement follows directly after the const segment (the const segment size is a multiple of a page size, so this is possible)
        // 
        // one way to satisfy the three requirements on linux:
        //
        // 1. mmap one big anonymous fake block for both segements
        // 2. unmap the fake block
        // 3. try to mmap the const segment to the address of the fake block
        // 4. try to mmap the dynamic segment after the const segment (=inside old fake block)
        //
        // 2, 3 and 4 would ideally be done in one atomic operation, which is not possible.
        // => it is possible that 3 or 4 fails if one of the old fake block pages is taken by someone else.
        // still it seems to work quite well
        // 
        char *fake = (char*) mmap( 0, size_all, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0 );
        if( fake == MAP_FAILED ) {
            __error( "mmap failed (fake)\n" );
        }
        
        munmap( fake, size_all );
        
        base_ = (char *) mmap( fake, size_, PROT_READ, MAP_SHARED, fd_, 0 );
        if( base_ != fake ) {
            __error( "mmap failed (const)\n" );
        }
        
        dyn_base_ = (char*) mmap( fake + size_, dyn_size_, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0 );
        if( dyn_base_ != fake + size_ ) {
            __error( "mmap failed (dyn)\n" );
        }
        
        std::cout << "mapped_image: " << (void*)base_ << " " << (void*)dyn_base_ << "\n";
        
        iter_ = base_;
        
        header_ = reinterpret_cast<header *>(&(*iter_));
        iter_ += sizeof( header );
    }
    ~mapped_image() {
        if( dyn_base_ != MAP_FAILED ) {
            munmap( dyn_base_, dyn_size_ );
            dyn_base_ = (char*)MAP_FAILED;
        }
        
        if( base_ != MAP_FAILED ) {
            munmap( base_, size_ );
            base_ = (char*)MAP_FAILED;
        }
        
        if( fd_ != -1 ) {
            close( fd_ );
        }
    }
#if 0
size_t size() const {
        return size_;
    }
    
    void unmap() {
        assert( base_ != 0 );
        munmap( base_, size_ );
        base_ = 0;
    }
    void map() {
        assert( base_ == 0 );
        int prot = PROT_READ;
        if( read_write_ ) {
            prot = PROT_READ | PROT_WRITE;
        }
        
        base_ = mmap( 0, size_, prot, MAP_SHARED, fd_, 0 );
        assert( base_ != 0 );
        
        if( read_write_ ) {
            madvise( base_, size_, MADV_SEQUENTIAL );
        }
    }

#endif
    
    
    
    char *begin() {
        return base_;
    }
    char *end() {
        return base_ + size_;
    }
    char *iter() {
        return iter_;
    }
    void call_const_initializers(auto_destructor_list_type &dest_list) {
        
        header *h = (header*)begin();
        
        dyn_list_entry *list = h->dyn_base_list_.get();
        uint32_t size = h->dyn_base_num_;
        

        std::cout << "initializers list size: " << size << "\n";
        
        for( uint32_t i = 0; i < size; ++i ) {
            dyn_list_entry *ptr = reinterpret_cast<dyn_list_entry*>(list + i);
            //std::cout << "ptr: " << ptr->ptr.get() << "\n";
            call_const_initializer( ptr->class_id, ptr->ptr.get(), dest_list );
        }
    }
    void call_dyn_destructors ( auto_destructor_list_type &dest_list ) {
    
        
        for( auto_destructor_list_type::reverse_iterator it = dest_list.rbegin(); it != dest_list.rend(); ++it ) {
            (*it)->~base_dyn();
            
        }
    }
    
private:
    //meh::mapped_file mf_;
    
    int fd_;
    
    char *base_;
    char *dyn_base_;
    
    char *iter_;
    size_t size_;
    size_t dyn_size_;
    
    
    inc_alloc::header *header_;
    
};

class blinken_thing {
  
public:
    char xxx_[4];
    
    blinken_thing() {
       strcpy( xxx_, "on!" );
    }
    
    ~blinken_thing() {
        strcpy( xxx_, "off" );
    }
};

class hpair;

class hpair_dyn: public base_dyn {
public:
    const static size_t class_id = 1024 + 1;
    
    hpair_dyn(const hpair *p) :  v_(0xdeadbeef), p_(p), bla_("testxxxxx") {}
    
    hpair_dyn() {}
    
    
    virtual ~hpair_dyn() {
        __named_message("this: %p const ptr: %p\n", this, p_.get());
        
    }
    
    uint32_t v_;
    rel_ptr<const hpair> p_;
    std::string bla_;
    blinken_thing blink_;
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
    
//     void const_contructor( inc_alloc &inc ) {
//         //__named_message("%p\n", this);
//         hpair_dyn *ptr_raw = inc.alloc<hpair_dyn>();
//         hpair_dyn *ptr = new(ptr_raw) hpair_dyn(this); 
//         dyn_test.reset( ptr );
//         
//         __named_message( "%p: %p -> %p\n", this, ptr_raw, ptr );
//         //dyn_test.reset( inc.alloc<hpair_dyn>() );
//     }
    
    
    
    void const_allocator( inc_alloc &inc ) {
        dyn_test.reset( inc.alloc<hpair_dyn>() );   
    }
    void const_initializer( auto_destructor_list_type &auto_destruct_list ) const {
        void *ptr = new(dyn_test.get()) hpair_dyn(this); 
        auto_destruct_list.push_back( dyn_test.get() );
        
        if( ptr != dyn_test.get() ) {
            throw std::runtime_error( "ptr != dyn_test.get()" );
        }
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

    void const_allocator( inc_alloc &inc ) {
        //__named_message("%p\n", this);
    }
    void const_initializer( std::vector<base_dyn*> &auto_destruct_list ) const {
        
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


static void call_const_allocator( size_t class_id, void *ptr, inc_alloc &inc ) {
    switch( class_id ) {
    case 1:
        (reinterpret_cast<hpair*>(ptr))->const_allocator(inc);
        
        break;
        
    case 2:
        (reinterpret_cast<hobj*>(ptr))->const_allocator(inc);
        break;
        

    default:
        std::cerr << "class_id: " << class_id << "\n";
        throw std::runtime_error( "class_id not found" );
    }
}

static void call_const_initializer( size_t class_id, void *ptr, auto_destructor_list_type &auto_destruct_list ) {
    switch( class_id ) {
    case 1:
        (reinterpret_cast<hpair*>(ptr))->const_initializer(auto_destruct_list);
        
        break;
        
    case 2:
        (reinterpret_cast<hobj*>(ptr))->const_initializer(auto_destruct_list);
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
    {
        inc_alloc xalloc( 1024 * 1024 * 16);
        std::cout << "parent: " << o->parent << "\n";
        std::vector<char>::iterator it1 = xalloc.begin();
        
        copy_hobj( o, 0, xalloc );
        xalloc.build_const_initializers();
        if( false )
        {
            auto_destructor_list_type dest_list;
            xalloc.call_const_initializers( dest_list );
        }
        #if 0
        getchar();
        
        auto_destructor_list_type dest_list;
        xalloc.call_const_initializers( dest_list );
        getchar();
        //     xalloc.call_
        
        
        
        print_hobj( ow, 0 );
        #endif
        
        std::cout << "baked size: " << std::distance( it1, xalloc.iter() ) << "\n";
        
        {
            std::ofstream os( "baked.bin", std::ios::binary );
        
            os.write( &(*it1), std::distance( it1, xalloc.iter() ) );
        }
        
        {
            std::ofstream os( "baked_nodyn.bin", std::ios::binary );
        
            os.write( &(*it1), std::distance( &(*it1), xalloc.dyn_segment_begin() ) );
        }
        
        #if 0
        getchar();
        xalloc.call_dyn_destructors( dest_list );
        
        getchar();
        
        #endif
        
    }
    getchar();
    {
        mapped_image xalloc2( "baked_nodyn.bin" );
        xalloc2.begin();
        
        auto_destructor_list_type dest_list;
        xalloc2.call_const_initializers( dest_list );
        getchar();
    
        hobj *ow2 = reinterpret_cast<hobj*>(&(*xalloc2.iter()));
    
        print_hobj( ow2, 0 );
        
        
#if 0
        getchar();
        
        std::ofstream os( "baked2.bin", std::ios::binary );
        
        os.write( &(*it1), std::distance( it1, xalloc2.end() ) );
        
        xalloc2.call_dyn_destructors( dest_list );
        
        getchar();
#endif
    }
}
#endif
