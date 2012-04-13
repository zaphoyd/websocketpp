/*
 * Copyright (c) 2011, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef WEBSOCKET_DATA_MESSAGE_HPP
#define WEBSOCKET_DATA_MESSAGE_HPP

#include "../common.hpp"
#include "../utf8_validator/utf8_validator.hpp"
#include "../processors/hybi_util.hpp"

#include <boost/detail/atomic_count.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>

#include <algorithm>
#include <iostream>
#include <queue>
#include <vector>

namespace websocketpp {
namespace message {

/// message::pool impliments a reference counted pool of elements.

// element_type interface:
// constructor(ptr p, size_t index)
// - shared pointer to the managing pool
// - integer index

// get_index();
// set_live()

template <class element_type>
    class pool : public boost::enable_shared_from_this< pool<element_type> >,
                 boost::noncopyable {
public:
    typedef pool<element_type> type;
    typedef boost::shared_ptr<type> ptr;
    typedef boost::weak_ptr<type> weak_ptr;
    typedef typename element_type::ptr element_ptr;
    typedef boost::function<void()> callback_type;
    
    pool(size_t max_elements) : m_cur_elements(0),m_max_elements(max_elements) {}
    ~pool() {}
    
    // copy/assignment constructors require C++11
    // boost::noncopyable is being used in the meantime.
    // pool(pool const&) = delete;
    // pool& operator=(pool const&) = delete
    
    /// Requests a pointer to the next free element in the resource pool.
    /* If there isn't a free element a new one is created. If the maximum number
     * of elements has been created then it returns an empty/null element 
     * pointer.
     */
    element_ptr get() {
        boost::lock_guard<boost::mutex> lock(m_lock);
        
        element_ptr p;
        
        /*std::cout << "message requested (" 
                  << m_cur_elements-m_avaliable.size()
                  << "/"
                  << m_cur_elements
                  << ")"
                  << std::endl;*/
        
        if (!m_avaliable.empty()) {
            p = m_avaliable.front();
            m_avaliable.pop();
            m_used[p->get_index()] = p;
        } else {
            if (m_cur_elements == m_max_elements) {
                return element_ptr();
            }
            
            p = element_ptr(new element_type(type::shared_from_this(),m_cur_elements));
            m_cur_elements++;
            m_used.push_back(p);
            
            /*std::cout << "Allocated new data message. Count is now " 
                      << m_cur_elements
                      << std::endl;*/
        }
        
        p->set_live();
        return p;
    }
    void recycle(element_ptr p) {
        boost::lock_guard<boost::mutex> lock(m_lock);
        
        if (p->get_index()+1 > m_used.size() || m_used[p->get_index()] != p) {
            //std::cout << "error tried to recycle a pointer we don't control" << std::endl;
            // error tried to recycle a pointer we don't control
            return;
        }
        
        m_avaliable.push(p);
        m_used[p->get_index()] = element_ptr();
        
        /*std::cout << "message recycled (" 
                  << m_cur_elements-m_avaliable.size()
                  << "/"
                  << m_cur_elements
                  << ")"
                  << std::endl;*/
        
        if (m_callback && m_avaliable.size() == 1) {
            m_callback();
        }
    }
    
    // set a function that will be called when new elements are avaliable.
    void set_callback(callback_type fn) {
        boost::lock_guard<boost::mutex> lock(m_lock);
        m_callback = fn;
    }
    
private:
    size_t          m_cur_elements;
    size_t          m_max_elements;
    
    std::queue<element_ptr> m_avaliable;
    std::vector<element_ptr> m_used;
    
    callback_type   m_callback;
    
    boost::mutex      m_lock;
};

class data {
public:
    typedef boost::intrusive_ptr<data> ptr;
    typedef pool<data>::ptr pool_ptr;
    typedef pool<data>::weak_ptr pool_weak_ptr;
    
    data(pool_ptr p, size_t s);
    
    void reset(websocketpp::frame::opcode::value opcode);
    
    frame::opcode::value get_opcode() const;
    const std::string& get_payload() const;
    const std::string& get_header() const;
    
    // ##reading##
    // sets the masking key to be used to unmask as bytes are read.
    void set_masking_key(int32_t key);
    
    // read at most size bytes from a payload stream and perform unmasking/utf8
    // validation. Returns number of bytes read.
    // throws a processor::exception if the message is too big, there is a fatal
    // istream read error, or invalid UTF8 data is read for a text message
    //uint64_t process_payload(std::istream& input,uint64_t size);
    void process_payload(char * input, size_t size);
    void complete();
    void validate_payload();
    
    // ##writing##
    // sets the payload to payload. Performs max size and UTF8 validation 
    // immediately and throws processor::exception if it fails
    void set_payload(const std::string& payload);
    void append_payload(const std::string& payload);
    
    void set_header(const std::string& header);
    
    // Performs masking and header generation if it has not been done already.
    void set_prepared(bool b);
    bool get_prepared() const;
    void mask();
    
    int32_t get_masking_key() const {
        return m_masking_key.i;
    }
    
    // pool management interface
    void set_live();
    size_t get_index() const;
private:
    static const uint64_t PAYLOAD_SIZE_INIT = 1000; // 1KB
    static const uint64_t PAYLOAD_SIZE_MAX = 100000000;// 100MB
    
    typedef websocketpp::processor::hybi_util::masking_key_type masking_key_type;
    
    friend void intrusive_ptr_add_ref(const data * s) {
        ++s->m_ref_count;
    }
    
    friend void intrusive_ptr_release(const data * s) {
        boost::unique_lock<boost::mutex> lock(s->m_lock);
        
        // TODO: thread safety
        long count = --s->m_ref_count;
        if (count == 1 && s->m_live) {
            // recycle if endpoint exists
            s->m_live = false;
            
            pool_ptr pp = s->m_pool.lock();
            if (pp) {
                lock.unlock();
                pp->recycle(ptr(const_cast<data *>(s)));
            }
            
            //s->m_pool->recycle(ptr(const_cast<data *>(s)));
        } else if (count == 0) {
            lock.unlock();
            boost::checked_delete(static_cast<data const *>(s));
        }
    }
    
    // Message state
    frame::opcode::value        m_opcode;
    
    // UTF8 validation state
    utf8_validator::validator   m_validator;
    
    // Masking state
    masking_key_type            m_masking_key;
    bool                        m_masked;
    size_t                      m_prepared_key;
    
    std::string                 m_header;
    std::string                 m_payload;
    
    bool                        m_prepared;
    
    // reference counting
    size_t                              m_index;
    mutable boost::detail::atomic_count m_ref_count;
    mutable pool_weak_ptr               m_pool;
    mutable bool                        m_live;
    mutable boost::mutex                m_lock;
};

typedef boost::intrusive_ptr<data> data_ptr;
    
} // namespace message
} // namespace websocketpp

#endif // WEBSOCKET_DATA_MESSAGE_HPP
