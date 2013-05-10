
#include "g_message_passing.h"


g_global_mp *g_mp = 0;

g_global_mp* g_global_mp::get_instance()
{
    if( g_mp == 0 ) {
     
        g_mp = new g_global_mp();
    }
    
    return g_mp;
    
}