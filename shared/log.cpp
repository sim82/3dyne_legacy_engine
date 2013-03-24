#include "shock.h"

#include "s_config.h"
#include "log.h"
namespace logger
{
    
void message::finish() const
{
//    cprintf( "%s:%d ", file_, line_, ss_.str().c_str() );
    
    
    const std::string &str = ss_.str();
    const char *msg = str.c_str();

    cprintf( "%s:%d %s", file_, int(line_), msg );
    
    if( str.length() == 0 || (str.length() > 0 && msg[str.length()-1] != '\n' ) ) {
        printf( "\n" );
    }
    //__error( "finish\n" );
}



} // namespace logger

