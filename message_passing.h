/* 
 * 3dyne Legacy Engine GPL Source Code
 * 
 * Copyright (C) 2013 Matthias C. Berger & Simon Berger.
 * 
 * This file is part of the 3dyne Legacy Engine GPL Source Code ("3dyne Legacy
 * Engine Source Code").
 *   
 * 3dyne Legacy Engine Source Code is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * 
 * 3dyne Legacy Engine Source Code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * 3dyne Legacy Engine Source Code.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * In addition, the 3dyne Legacy Engine Source Code is also subject to certain
 * additional terms. You should have received a copy of these additional terms
 * immediately following the terms and conditions of the GNU General Public
 * License which accompanied the 3dyne Legacy Engine Source Code.
 * 
 * Contributors:
 *     Matthias C. Berger (mcb77@gmx.de) - initial API and implementation
 *     Simon Berger (simberger@gmail.com) - initial API and implementation
 */ 


#ifndef __message_passing_h
#define __message_passing_h

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
#include <fstream>

namespace msg {
template <typename T>
using ptr = std::unique_ptr<T>;
    
    class base {
    public:
        ~base() {}  
        
        
        template<typename T> 
        T &as_type() {
            return static_cast<T &>(*this) ;
        }
    };
    
    class stop : public base {};
    
}


namespace mp {

const static bool sender_handler_check = true;
    



class typeinfo_wrapper {
public:
    typeinfo_wrapper( const std::type_info &t, size_t size ) : t_(t), size_(size) {}

    inline bool operator==( const typeinfo_wrapper &other ) const {
        return t_ == other.t_;   
    }
    
    inline size_t hash_value() const {
        return t_.hash_code();
    }

    inline const char *name() const {
        return t_.name();
    }

    inline size_t size() const {
        return size_;
    }

    struct hash
    {
        size_t operator()(const typeinfo_wrapper & x) const
        {
            return x.t_.hash_code();
        }
    };

    
private:  
    
    const std::type_info &t_;
    size_t size_;
  
};

template<typename T>
typeinfo_wrapper make_typeinfo_wrapper() {
    return typeinfo_wrapper( typeid(T), sizeof(T) );

}

    
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

template<typename T>
class msg_fwd {
public:
    typedef std::function<void(std::unique_ptr<T>)> fwd_func_type;
    
    fwd_func_type func_;
    
    msg_fwd( fwd_func_type func ) : func_(func) {}
    
    inline void operator()( std::unique_ptr<msg::base> msg ) {
    
        func_( std::unique_ptr<T>( static_cast<T*>(msg.release())) );
    }
    
};

template<typename TRet, typename T>
class msg_ret_fwd {
public:
    typedef std::function<std::unique_ptr<TRet>(std::unique_ptr<T>)> fwd_func_type;
    
    fwd_func_type func_;
    
    msg_ret_fwd( fwd_func_type func ) : func_(func) {}
    
    inline std::unique_ptr<msg::base> operator()( std::unique_ptr<msg::base> msg ) {
    
        return func_( std::unique_ptr<T>( static_cast<T*>(msg.release())) );
    }
    
};


class queue {
    
    const size_t TOKEN_NONE = size_t(-1);
    struct q_entry_type {
        q_entry_type( typeinfo_wrapper && wrap, std::unique_ptr<msg::base> && msg, size_t token, bool return_msg ) : wrap_(wrap), msg_(std::move(msg)), return_token_(token), return_msg_(return_msg) {}
        
        typeinfo_wrapper wrap_;
        std::unique_ptr<msg::base> msg_;
        size_t return_token_;
        bool return_msg_;
        
    };
    
    typedef std::function<void(std::unique_ptr<msg::base>)> handler_type;
    typedef std::function<std::unique_ptr<msg::base>(std::unique_ptr<msg::base>)> handler_ret_type;
    
    
    struct return_entry_type {

        return_entry_type( queue *q, typeinfo_wrapper &&typeinfo ) : q_(q), typeinfo_(typeinfo) {}
        queue * q_;
        typeinfo_wrapper typeinfo_;
    };
    typedef std::chrono::high_resolution_clock clock_type;
    
    struct msg_profiling {
      
        msg_profiling() : num_calls_(0), sum_durations_(clock_type::duration::zero()) {
        }
        
        
        size_t num_calls_;
        clock_type::duration sum_durations_;
    };
  
public:
    
    queue( const char *name = "none" );
    
    
    template<typename T, typename... Args>
    void emplace( Args...  args ) {
        std::unique_lock<std::mutex> lock( central_mtx_ );
        
        
        auto typeinfo = make_typeinfo_wrapper<T>();
        if( sender_handler_check ) {
            if( handler_map_.find(typeinfo) == handler_map_.end() ) {
                std::cerr << "no handler registered for message type. not enqueing " << typeinfo.name() << "\n";
                abort();
                return;
            }
        }
  
        q_.emplace_back( std::move(typeinfo), make_unique<T>(std::forward<Args>(args)...), TOKEN_NONE, false );
       
        
        lock.unlock();
        q_cond_.notify_one();
    }

   
   
    void emplace_msg( typeinfo_wrapper && typeinfo, std::unique_ptr<msg::base> msg, size_t token, bool return_msg );
   
    template<typename T, typename... Args>
    size_t emplace_raw( queue *ret_queue, Args...  args ) {
        std::unique_lock<std::mutex> lock( central_mtx_ );
        
        auto typeinfo = make_typeinfo_wrapper<T>();
        if( sender_handler_check ) {
            if( handler_ret_map_.find(typeinfo) == handler_ret_map_.end() ) {
                std::cerr << "no handler registered for message type. not enqueing " << typeinfo.name() << "\n";
                abort();
                return TOKEN_NONE;
            }
        }
        
        size_t token = return_token_count_++;
        
        return_map_.emplace( token, return_entry_type(ret_queue, make_typeinfo_wrapper<T>()) );
        q_.emplace_back( std::move(typeinfo), make_unique<T>(std::forward<Args>(args)...), token, false );
        
        
        lock.unlock();
        q_cond_.notify_one();
        
        return token;
    }
    
    
    template<typename T, typename... Args>
    void emplace_return_deluxe( queue &ret_queue, std::function<void(std::unique_ptr<typename T::return_type>)> h, Args...  args ) {
        std::unique_lock<std::mutex> lock( central_mtx_ );
        
        auto typeinfo{make_typeinfo_wrapper<T>()};
        if( sender_handler_check ) {
            if( handler_ret_map_.find(typeinfo) == handler_ret_map_.end() ) {
                std::cerr << "no handler registered for message type. not enqueing " << typeinfo.name() << "\n";
                abort();
                return;
            }
        }
        
        size_t token = return_token_count_++;
        
        return_map_.emplace( token, return_entry_type(&ret_queue, make_typeinfo_wrapper<T>()) );
        q_.emplace_back( std::move(typeinfo), make_unique<T>(std::forward<Args>(args)...), token, false );
        
        
        lock.unlock();
        q_cond_.notify_one();
        
        
        ret_queue.add_token_handler<T>(token, h);
        
    }
    
    

   
    void dispatch( q_entry_type &&ent );
    
    
    void dispatch_ret( q_entry_type &&ent, return_entry_type ret_ent, size_t token );
   
    
    bool dispatch_pop();
    bool dispatch_pop_noblock();
    
    
    template<typename T>
    void add_handler_unsafe( handler_type h ) {
        std::lock_guard<std::mutex> lock( central_mtx_ );

        handler_map_.emplace( make_typeinfo_wrapper<T>(), h );
        add_profiling_entry<T>();
        add_typename_mapping<T>();
    }
    
    
    template<typename T>
    void add_handler( std::function<void(std::unique_ptr<T>)> h ) {
        std::lock_guard<std::mutex> lock( central_mtx_ );
        handler_map_.emplace( make_typeinfo_wrapper<T>(), msg_fwd<T>(h) );
        add_profiling_entry<T>();
        add_typename_mapping<T>();
    }

    
    template<typename T>
    void add_handler_ret( std::function<std::unique_ptr<typename T::return_type>(std::unique_ptr<T>)> h ) {
        std::lock_guard<std::mutex> lock( central_mtx_ );
        handler_ret_map_.emplace( make_typeinfo_wrapper<T>(), msg_ret_fwd<typename T::return_type, T>(h) );
        add_profiling_entry<T>();
        add_typename_mapping<T>();
        
    }
    
    
    template<typename T>
    void add_token_handler( size_t token, std::function<void(std::unique_ptr<typename T::return_type>)> h ) {
        std::lock_guard<std::mutex> lock( central_mtx_ );
//         std::cout << "add token handler: " << token << "\n";
        token_handler_map_.emplace( token, msg_fwd<typename T::return_type>(h) );  
    }
    
    void stop();
    
    bool is_stopped() const;
    
    void add_default_stop_handler();
    
    
    void print_profiling( std::ostream &os );
    const std::string &name() {
        return name_;   
    }

    void open_logfile( const char *name ) {
        log_start_time_ = clock_type::now();
        os_log_.open( name, std::ios::binary );
    }

private:
    
    template<typename T>
    void add_profiling_entry() {
        profiling_map_.emplace( make_typeinfo_wrapper<T>(), msg_profiling() );
        
    }
    
    template<typename T>
    void add_typename_mapping() {
        typeinfo_wrapper t{make_typeinfo_wrapper<T>()};
        typename_typeinfo_map_.emplace( t.name(), std::move(t) );
    }


    bool dispatch_pop_internal(std::unique_lock< std::mutex >& lock);

    void log_message( const typeinfo_wrapper &t, const msg::base & msg );
    
    mutable std::mutex central_mtx_;
    std::condition_variable q_cond_;
    
    std::string name_;
    size_t return_token_count_;
    bool do_stop_;
    
    std::deque<q_entry_type> q_;
    
    
    
    std::unordered_map<typeinfo_wrapper,handler_type,typeinfo_wrapper::hash> handler_map_;
    std::unordered_map<typeinfo_wrapper,handler_ret_type,typeinfo_wrapper::hash> handler_ret_map_;
    
    std::unordered_map<size_t,handler_type> token_handler_map_;
    
    std::unordered_map<size_t, return_entry_type> return_map_;
    
    
    std::unordered_map<typeinfo_wrapper, msg_profiling,typeinfo_wrapper::hash> profiling_map_;

    std::unordered_map<std::string,typeinfo_wrapper> typename_typeinfo_map_;
    
    std::ofstream os_log_;
    clock_type::time_point log_start_time_;

};


class timer_source {
public:
    typedef std::chrono::high_resolution_clock clock_type;
    
    
    timer_source() : do_stop_(false) {
        thread_ = std::thread( [this]() {
            thread_function();
        });
        
    }
    ~timer_source() {
        std::unique_lock<std::mutex> lock(mtx_);
        do_stop_ = true;
        lock.unlock();
        cond_.notify_all();
        thread_.join();
    }
    
    void thread_function() {
        std::unique_lock<std::mutex> lock( mtx_ );
        while( !do_stop_ ) {
            
            clock_type::time_point next = next_timeout();
            
            cond_.wait_until( lock, next );
            
            
            trigger_timers();
            
        }
        
    }
    
    
    void trigger_timers() {
        clock_type::time_point now = clock_type::now();
        
        
        for( entry & ent : entries_ ) {
            if( ent.next_trigger_ < now ) {
                ent.handler_();
                
                if( ent.repeat_ ) {
                    ent.next_trigger_ += ent.duration_;   
                } else {
                    ent.remove_ = true;   
                }
           }
        }

        
        // remove expired timers
        
        auto it = std::remove_if( entries_.begin(), entries_.end(), []( const entry &ent ) {
            return ent.remove_;
        });
        
        entries_.erase( it, entries_.end() );
        
    }
    
    clock_type::time_point next_timeout() {
     
        clock_type::time_point smallest{clock_type::time_point::max()};
        
        for( entry & ent : entries_ ) {
            smallest = std::min( smallest, ent.next_trigger_ );
        }
        
        return smallest;
    }
    
    template<typename T>
    void add_timer( queue *q, const clock_type::duration &dur, bool repeat ) {
        std::unique_lock<std::mutex> lock( mtx_ );
        
        entries_.push_back(entry());
        
        auto &ent = entries_.back();
        
        ent.handler_ = [=]() {
            q->emplace<T>();
        };
        ent.duration_ = dur;
        ent.repeat_ = repeat;
        ent.next_trigger_ = clock_type::now() + dur;
        
        
        lock.unlock();
        cond_.notify_one();
    }
    
private:
    bool do_stop_;
    std::mutex mtx_;
    std::condition_variable cond_;
    
    std::thread thread_;
    
    struct entry {
    public:
        entry() : remove_(false) {}
        std::function<void()> handler_;
        
        bool repeat_;
        clock_type::duration duration_;
        
        clock_type::time_point next_trigger_;
        
        bool remove_;
    };
    
    std::vector<entry> entries_;
    
    
    
};    

}

/*
namespace std {
    
    
    using mp::hash<typeinfo_wrapper>;
    
}*/


#endif
