#ifndef __3dyne_log_h
#define __3dyne_log_h
#include <sstream>


namespace logger {
class message {
public:
    message( const char *file, size_t line ) : file_(file), line_(line) {}
    void finish() const;
    
    template<typename T>
    message &operator<<( T& v ) {
        ss_.operator<<(v);
        return *this;
    }
    
    message &operator<<( const char *s ) {
        ss_ << s;
        return *this;
    }
    
private:
    
    std::stringstream ss_;
    const char *file_;
    size_t line_;
    
};
  

class finisher {
public:
    void operator=( const message &msg ) {
        msg.finish();   
    }
};
    
}

#define DD_LOG                              \
  ::logger::finisher() =                           \
    ::logger::message( __FILE__, __LINE__)

#endif


