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

#ifndef WSCMD_HPP
#define WSCMD_HPP

#include <map>
#include <string>

namespace wscmd {
    // Parses a wscmd string.
    
    // command structure
    // command:arg1=val1;arg2=val2;arg3=val3;
    
    // commands
    // ack: messages to ack
    // example: `ack:e3458d0aceff8b70a3e5c0afec632881=38;e3458d0aceff8b70a3e5c0afec632881=42;`
    
    // send: [vals]
    //       message; opcode=X; payload="X"
    //       frame; [fuzzer stuff]
    
    // close:code=1000;reason=msg;
    // (instructs the opposite end to close with given optional code/msg)
    typedef std::map<std::string,std::string> arg_list;
    
    struct cmd {
        // TODO: move semantics
        std::string command;
        arg_list args;
    };
    
    wscmd::cmd parse(const std::string& m);
    
    wscmd::cmd parse(const std::string& m) {
        cmd command;
        std::string::size_type start;
        std::string::size_type end;
        
        start = m.find(":",0);
        
        if (start != std::string::npos) {
            command.command = m.substr(0,start);
            
            start++; // skip the colon
            end = m.find(";",start);
            
            // find all semicolons
            while (end != std::string::npos) {
                std::string arg;
                std::string val;
                
                std::string::size_type sep = m.find("=",start);
                
                if (sep != std::string::npos) {
                    arg = m.substr(start,sep-start);
                    val = m.substr(sep+1,end-sep-1);
                } else {
                    arg = m.substr(start,end-start);
                    val = "";
                }
                
                command.args[arg] = val;
                
                start = end+1;
                end = m.find(";",start);
            }
        }
        
        return command;
    }
} // namespace wscmd

#endif // WSCMD_HPP
