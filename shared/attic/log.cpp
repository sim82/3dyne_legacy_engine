#include <cstring>
#include "Shared/shock.h"

#include "s_config.h"
#include "log.h"
namespace logger
{

message::message ( const char* file, size_t line ) : file_ ( file ), line_ ( line ) {
    //const char *cmsg = smsg.c_str();
    
    const char *slash_pos = strrchr(file, '/');
    
    file = (slash_pos != 0) ? slash_pos + 1 : file;
    
    (*this) << file << ":" << line << " ";
    
}

    
// void message::finish() const
// {
// //    cprintf( "%s:%d ", file_, line_, ss_.str().c_str() );
//     
//     
//     const std::string &str = basic_stringstream<char>::str();
//     const char *msg = str.c_str();
// 
//     cprintf( "%s:%d %s", file_, int(line_), msg );
//     
//     if( str.length() == 0 || (str.length() > 0 && msg[str.length()-1] != '\n' ) ) {
//         printf( "\n" );
//     }
//     //__error( "finish\n" );
// }

void finisher::operator= ( const std::basic_ostream< char >& omsg )
{
//    msg.finish();
    
    const std::basic_stringstream<char> &msg = static_cast<const std::basic_stringstream<char> &>(omsg);
    
    const std::string &smsg = msg.str();

    bool have_newline = (smsg.size() > 0) && (*(smsg.rbegin()) == '\n');
    
    
    
    if( have_newline ) {
        cprintf( "%s", smsg.c_str() );   
    } else {
        cprintf( "%s\n", smsg.c_str() );   
    }
    //cprintf( "%s:%d %s", file_, int(line_), msg );
}



} // namespace logger

