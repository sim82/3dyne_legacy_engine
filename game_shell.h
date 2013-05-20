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



    using std::string::size;

    operator int() const ;
    operator double() const ;
    operator const std::string &() const;
    operator std::string();
private:
    type type_;
};

class interpreter {
public:
    interpreter( mp::queue &q );

    void exec( const char *buf );

    variant &env_get(const char * name);
private:
    void dispatch(const char *name);

    mp::queue &target_queue_;



    typedef std::function<void(mp::queue &)> message_sender_type;
    std::unordered_map<std::string, message_sender_type> sender_map_;


    std::unordered_map<std::string, variant> environment_map_;
};



}


#endif // GAME_SHELL_H
