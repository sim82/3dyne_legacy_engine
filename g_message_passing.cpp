#include <atomic>
#include "g_message_passing.h"
#include "shared/shock.h"

//std::atomic<g_global_mp*> g_mp;

//g_global_mp* g_global_mp::get_instance()
//{
//    g_global_mp *mp = g_mp.load();
    
//    if( mp == 0 ) {
     
//        g_global_mp *mp_init = mp;
        
//        mp = new g_global_mp();
        
//        if( !g_mp.compare_exchange_strong( mp_init, mp ) ) {
//            __error( "concurrent creation of g_global_mp::get_instance\n" );
//        }
        
//    }
    
//    return g_mp;
    
//}
