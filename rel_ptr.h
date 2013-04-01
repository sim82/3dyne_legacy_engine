#ifndef rel_ptr_h__
#define rel_ptr_h__
#include <iostream>
#include <stdexcept>
#include <limits>
#include <numeric>
#include <stdint.h>

#if 0
template<typename T>
class rel_ptr {
public:
    rel_ptr() : off_(0) {
        
    }
    
    rel_ptr( const rel_ptr &other ) {
        reset( other.get() );
    }
    
    rel_ptr operator=( const rel_ptr &other ) {
        reset( other.get() );
    }
    
    rel_ptr( T *ptr ) {
        reset( ptr );
    }
    
    
    void reset( T *ptr ) {
        size_t dest = size_t(ptr);
        size_t src = size_t(this);
        
        off_ = off_t(dest - src);    
    }
    
    inline T* get() const {
        if( off_ == 0 ) {
            throw std::runtime_error( "bad relative ptr" );
        }
        
        size_t src = size_t(this);
        size_t dest = src + off_;
        
        return (T*)(dest);
    }
    
    inline bool valid() const {
        return off_ != 0;
    }
    
    void test() const {
        std::cout << "rel ptr: " << this << " + " << off_ << " = " << get() << "\n";
    }
    
private:
    ssize_t off_;
    
};
#else
template<typename T>
class rel_ptr {
public:
    rel_ptr() : off_(0) {
        
    }
    
    rel_ptr( const rel_ptr &other ) {
        reset( other.get() );
    }
    
    rel_ptr operator=( const rel_ptr &other ) {
        reset( other.get() );
    }
    
    rel_ptr( T *ptr ) {
        reset( ptr );
    }
    
    
    void reset( T *ptr ) {
        size_t dest = size_t(ptr);
        size_t src = size_t(this);
        
        ssize_t xoff = off_t(dest - src);
        
        
        
        if( xoff > std::numeric_limits<int32_t>::max() || xoff < std::numeric_limits<int32_t>::min() ) {
            std::cerr << "bad: " << size_t(ptr) << " - " << size_t(this)  << " = " << xoff << "\n";
            throw std::runtime_error( "relative pointer out of range" );
        }
        
            
            
        off_ = xoff;
    }
    
    inline T* get() const {
        if( off_ == 0 ) {
            throw std::runtime_error( "bad relative ptr" );
        }
        
        size_t src = size_t(this);
        size_t dest = src + off_;
        
        return (T*)(dest);
    }
    
    inline bool valid() const {
        return off_ != 0;
    }
    
    void test() const {
        std::cout << "rel ptr: " << this << " + " << off_ << " = " << get() << "\n";
    }
    
    
    inline T* operator->() {
        return get();
    }
private:
    int32_t off_;
    
};

#endif

#endif
