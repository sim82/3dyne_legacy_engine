#include <iostream>
#include <typeinfo>
#include <vector>
#include <unordered_map>
#include <memory>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <unistd.h>
#include "message_passing.h"




#if DD_USE_DLT
#include "dlt.h"
DLT_DECLARE_CONTEXT(mycontext);
static bool s_register_dlt_context = true;
#endif

namespace mp {

queue::queue( const char *name ) : name_(name), return_token_count_ ( 0 ), do_stop_ ( false ) {
#if DD_USE_DLT
    if( s_register_dlt_context ) {
        DLT_REGISTER_CONTEXT(mycontext,"TEST","Test Context for Logging");   
        s_register_dlt_context = false;
    }
#endif
    
}
void queue::emplace_msg ( typeinfo_wrapper&& typeinfo, std::unique_ptr< msg::base > msg, size_t token, bool return_msg )
{
    std::unique_lock<std::mutex> lock ( central_mtx_ );
    q_.emplace_back ( std::move ( typeinfo ), std::move ( msg ), token, return_msg );

    lock.unlock();
    q_cond_.notify_one();
}
void queue::dispatch ( queue::q_entry_type&& ent )
{
    auto it = handler_map_.find ( ent.wrap_ );

    if ( it == handler_map_.end() ) {
        std::cerr << "no handler for type: " << ent.wrap_.name() << "\n";
        return;
    }

    
#if DD_USE_DLT
    DLT_LOG(mycontext,DLT_LOG_INFO,DLT_STRING(name().c_str()),DLT_STRING(ent.wrap_.name()));
#endif    
    it->second ( std::move ( ent.msg_ ) );


}
void queue::dispatch_ret ( queue::q_entry_type&& ent, queue::return_entry_type ret_ent, size_t token )
{
    auto it = handler_ret_map_.find ( ent.wrap_ );

    if ( it == handler_ret_map_.end() ) {
        std::cerr << "no handler_ret for type: " << ent.wrap_.name() << "\n";
        return;
    }

    std::unique_ptr<msg::base> ret_msg = it->second ( std::move ( ent.msg_ ) );
    ret_ent.q_->emplace_msg ( std::move ( ret_ent.typeinfo_ ), std::move ( ret_msg ), token, true );

}
bool queue::dispatch_pop()
{


    std::unique_lock<std::mutex> lock ( central_mtx_ );

//         if( q_.empty() ) {
//             return false;
//         }

    while ( q_.empty() && !do_stop_ ) {
        q_cond_.wait ( lock );
    }

    if ( do_stop_ ) {
        return false;
    }

    
    // q_ can not be empty
    dispatch_pop_internal( lock );
    return true;
}

bool queue::dispatch_pop_noblock() {
    std::unique_lock<std::mutex> lock ( central_mtx_ );
    
    if( !q_.empty() ) {
        dispatch_pop_internal( lock );
        return true;
        
    } else {
        return false;
        
    }
}


bool queue::dispatch_pop_internal( std::unique_lock<std::mutex> &lock ) {
    // this function _can_ release the lock before returning
    
    assert( !q_.empty() );
    
    q_entry_type ent {std::move ( q_.front() ) };
    q_.pop_front();
    
    const bool do_profiling = true;


    clock_type::time_point tp_start;
    
    auto it = profiling_map_.end(); 

    // find profiling map entry now while 'ent' is still valid (move below!). prevents having to store it temporarily (and does not count profiling_map_ overhead)
    if( do_profiling ) {
        it = profiling_map_.find( ent.wrap_ );
        tp_start = clock_type::now();
    }
    

    log_message( ent.wrap_, *ent.msg_ );

    if ( ent.return_token_ == TOKEN_NONE ) {

        lock.unlock();
        dispatch ( std::move ( ent ) );
    } else {

        if ( !ent.return_msg_ ) {
            auto it = return_map_.find ( ent.return_token_ );

            if ( it == return_map_.end() ) {
                std::cerr << "no target queue for return token: " << ent.return_token_ << "\n";
                return true;
            }

            auto ret_entry = it->second;
            return_map_.erase ( it );

            lock.unlock();

            dispatch_ret ( std::move ( ent ), ret_entry, ent.return_token_ );
        } else {



            auto it = token_handler_map_.find ( ent.return_token_ );

            if ( it == token_handler_map_.end() ) {
                std::cerr << "no token return handler: " << ent.return_token_ << "\n";
                return true;
            }

            auto func = it->second;
            token_handler_map_.erase ( it );

            lock.unlock();

            func ( std::move ( ent.msg_ ) );

        }
    }

    // profiling
    if( do_profiling )
    {
        lock.lock();
        if( it != profiling_map_.end() ) {
    
            it->second.sum_durations_ += (clock_type::now() - tp_start);
            ++it->second.num_calls_;
        }
        
    }
    

    return true;

}
void queue::stop()
{
    std::unique_lock<std::mutex> lock ( central_mtx_ );
    do_stop_ = true;

    lock.unlock();
    q_cond_.notify_all();
}
bool queue::is_stopped() const
{
    std::lock_guard<std::mutex> lock ( central_mtx_ );
    return do_stop_;

}
void queue::add_default_stop_handler()
{
    add_handler<msg::stop> ( [this] ( std::unique_ptr<msg::stop> m ) {
        stop();
    } );
}

void queue::print_profiling(std::ostream& os) {
    os << "====  message queue profiling ==========================\n";
    
    for( auto & ent : profiling_map_ ) {
        
        if( ent.second.num_calls_ == 0 ) {
            continue;   
        }
        std::chrono::microseconds us{std::chrono::duration_cast<std::chrono::microseconds>(ent.second.sum_durations_)};
        
        double seconds = (us.count() / 1000000.0);
        
        os << ent.first.name() << ":\t" << ent.second.num_calls_ << "\t" << seconds << "s\t(" << (seconds / ent.second.num_calls_) << "s/call)\n"; ;
    
        ent.second = msg_profiling();
        
    }
    
    
    
}


void queue::log_message( const typeinfo_wrapper &t, const msg::base & msg ) {
    if( false && os_log_.good() ) {
        auto ts = clock_type::now() - log_start_time_;

        os_log_ << t.name() << "\n";
        os_log_ << std::dec << std::chrono::duration_cast<std::chrono::microseconds>(ts).count() << "\n";
        os_log_ << t.size() << "\n";
        //os_log_.write( (char *) &msg, t.size() );
        const unsigned char *msg_start = (unsigned char*)&msg;
        const unsigned char *msg_end = msg_start + t.size();
        std::for_each( msg_start, msg_end, [&](unsigned char v) { os_log_ << std::hex << size_t(v) << " "; });

        os_log_ << std::endl;
    }
}



} // namespace mp





