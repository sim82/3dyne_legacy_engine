#ifndef GAME_SHELL_H
#define GAME_SHELL_H
#include <unordered_map>
#include <functional>
namespace mp {
class queue;
}

namespace gs {
class variant : private std::string {
public:
    enum type {
        vt_string,
        vt_int,
        vt_double
    };

    variant() : type_(vt_string) {}

    void set( const std::string &v, type t ) {
        std::string::operator=(v);
        type_ = t;
    }
    const std::string &get() const {
        return *this;
    }
    using std::string::size;

    operator int() const ;
    operator double() const ;
    operator const std::string &() const;
    operator std::string();
private:
    type type_;
};
class environment {

public:
    variant &get(const std::string &name);

    void print() const;
private:

    std::unordered_map<std::string, variant> global_map_;
};

class interpreter {
public:
    interpreter( mp::queue &q );

    void exec( const char *buf );

    variant &env_get(const char * name);

    void dispatch(const std::string &name);
private:


    mp::queue &target_queue_;



    typedef std::function<void(mp::queue &)> message_sender_type;
    std::unordered_map<std::string, message_sender_type> sender_map_;
    environment env_;


};



}


#endif // GAME_SHELL_H
