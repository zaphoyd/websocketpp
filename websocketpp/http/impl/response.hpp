/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
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

#ifndef HTTP_PARSER_RESPONSE_IMPL_HPP
#define HTTP_PARSER_RESPONSE_IMPL_HPP

#include <algorithm>
#include <istream>
#include <sstream>
#include <string>

#include <websocketpp/http/parser.hpp>

namespace websocketpp {
namespace http {
namespace parser {

inline size_t response::consume(char const * buf, size_t len, lib::error_code & ec) {
    if (m_state == state::DONE) {
        // the response is already complete. End immediately without reading.
        ec.clear();
        return 0;
    }

    if (m_state == state::BODY) {
        // The headers are complete, but we are still expecting more body
        // bytes. Process body bytes.
        return this->process_body(buf,len,ec);
    }

    // at this point we have an incomplete response still waiting for headers

    // copy new candidate bytes into our local buffer. This buffer may have
    // leftover bytes from previous calls. Not all of these bytes are 
    // necessarily header bytes (they might be body or even data after this
    // request entirely for a keepalive request)
    m_buf->append(buf,len);

    // Search for delimiter in buf. If found read until then. If not read all
    std::string::iterator begin = m_buf->begin();
    std::string::iterator end = begin;


    for (;;) {
        // search for delimiter
        end = std::search(
            begin,
            m_buf->end(),
            http_crlf,
            http_crlf + sizeof(http_crlf) - 1
        );

        if (end == m_buf->end()) {
            // we didn't find the delimiter

            // check that the confirmed header bytes plus the outstanding
            // candidate bytes do not put us over the header size limit.
            if (m_header_bytes + (end - begin) > max_header_size) {
                ec = error::make_error_code(error::request_header_fields_too_large);
                return 0;
            }

            // We are out of bytes but not over any limits yet. Discard the
            // processed bytes and copy the remaining unprecessed bytes to the 
            // beginning of the buffer in prep for another call to consume.

            // If there are no processed bytes in the buffer right now don't
            // copy the unprocessed ones over themselves.
            if (begin != m_buf->begin()) {
                std::copy(begin,end,m_buf->begin());
                m_buf->resize(static_cast<std::string::size_type>(end-begin));
            }

            ec.clear();
            return len;
        }

        // at this point we have found a delimiter and the range [begin,end)
        // represents a line to be processed

        // update count of header bytes read so far
        m_header_bytes += (end-begin+sizeof(http_crlf));
        
        if (m_header_bytes > max_header_size) {
            // This read exceeded max header size
            ec = error::make_error_code(error::request_header_fields_too_large);
            return 0;
        }


        if (end-begin == 0) {
            // we got a blank line, which indicates the end of the headers

            // If we are still looking for a response line then this request
            // is incomplete
            if (m_state == state::RESPONSE_LINE) {
                ec = error::make_error_code(error::incomplete_request);
                return 0;
            }

			if (!prepare_body(ec))
				return 0;

			if (m_body_encoding == body_encoding::unknown) {
				m_state = state::DONE;
				m_buf.reset();
            	ec.clear();
				return len; // pretend we read everything!
			}

            // transition state to reading the response body
            m_state = state::BODY;

            // calculate how many bytes in the local buffer are bytes we didn't
            // use for the headers. 
            size_t read = (
                len - static_cast<std::string::size_type>(m_buf->end() - end)
                + sizeof(http_crlf) - 1
            );

            // if there were bytes left process them as body bytes.
            // read is incremented with the number of body bytes processed.
            // It is possible that there are still some bytes not read. These
            // will be 'returned' to the caller by having the return value be
            // less than len.
            if (read < len) {
                read += this->process_body(buf+read,(len-read),ec);
            }
            if (ec) {
				m_state = state::DONE;
                return 0;
            }

			if (m_body_bytes_needed == 0) {
				m_state = state::DONE;
			}

            // frees memory used temporarily during header parsing
            m_buf.reset();

            ec.clear();
            return read;
        } else {
            // we got a line 
            if (m_state == state::RESPONSE_LINE) {
                ec = this->process(begin,end);
                m_state = state::HEADERS;
            } else {
                ec = this->process_header(begin,end);
            }
            if (ec) {
                return 0;
            }
        }

        // if we got here it means there is another header line to read.
        // advance our cursor to the first character after the most recent
        // delimiter found.
        begin = end+(sizeof(http_crlf) - 1);
    }
}

inline size_t response::consume(std::istream & s, lib::error_code & ec) {
    char buf[istream_buffer];
    size_t bytes_read;
    size_t bytes_processed;
    size_t total = 0;

    while (s.good()) {
        s.getline(buf,istream_buffer);
        bytes_read = static_cast<size_t>(s.gcount());

        if (s.fail() || s.eof()) {
            bytes_processed = this->consume(buf,bytes_read,ec);
            total += bytes_processed;

            if (ec) { return total; }

            if (bytes_processed != bytes_read) {
                // we read more data from the stream than we needed for the
                // HTTP response. This extra data gets thrown away now.
                // Returning it to the caller is complicated so we alert the
                // caller at least. This whole method has been deprecated
                // because this convenience method doesnt really add useful
                // functionality to the library, but makes it difficult to
                // recover from error cases.
                ec = error::make_error_code(error::istream_overread);
                return total;
            }
        } else if (s.bad()) {
            // problem
            break;
        } else {
            // the delimiting newline was found. Replace the trailing null with
            // the newline that was discarded, since our raw consume function
            // expects the newline to be be there.
            buf[bytes_read-1] = '\n';
            bytes_processed = this->consume(buf,bytes_read,ec);
            total += bytes_processed;

            if (ec) { return total; }

            if (bytes_processed != bytes_read) {
                // we read more data from the stream than we needed for the
                // HTTP response. This extra data gets thrown away now.
                // Returning it to the caller is complicated so we alert the
                // caller at least. This whole method has been deprecated
                // because this convenience method doesnt really add useful
                // functionality to the library, but makes it difficult to
                // recover from error cases.
                ec = error::make_error_code(error::istream_overread);
                return total;
            }
        }
    }

    return total;
}

inline std::string response::raw() const {
    // TODO: validation. Make sure all required fields have been set?

    std::stringstream ret;

    ret << get_version() << " " << m_status_code << " " << m_status_msg;
    ret << "\r\n" << raw_headers() << "\r\n";

    ret << m_body;

    return ret.str();
}

inline lib::error_code response::set_status(status_code::value code) {
    // In theory the type of status_code::value should prevent setting any
    // invalid values. Messages are canned and looked up and known to be
    // valid.
    // TODO: Is there anything else that would need validation here?
    m_status_code = code;
    m_status_msg = get_string(code);
    return lib::error_code();
}

inline lib::error_code response::set_status(status_code::value code,
    std::string const & msg)
{
    // In theory the type of status_code::value should prevent setting any
    // invalid values.
    // TODO: Is there anything else that would need validation here?
    // length or content of message?
    // Per RFC2616
    // Reason-Phrase  = *<TEXT, excluding CR, LF>
    // TEXT = = <any OCTET except CTLs,but including LWS>
    // CTL = <any US-ASCII control character (octets 0 - 31) and DEL (127)>
    // LWS = [CRLF] 1*( SP | HT )
    m_status_code = code;
    m_status_msg = msg;
    return lib::error_code();
}

inline lib::error_code response::process(std::string::iterator begin,
    std::string::iterator end)
{
    std::string::iterator cursor_start = begin;
    std::string::iterator cursor_end = std::find(begin,end,' ');

    if (cursor_end == end) {
        return error::make_error_code(error::incomplete_status_line);
    }

    set_version(std::string(cursor_start,cursor_end));

    cursor_start = cursor_end+1;
    cursor_end = std::find(cursor_start,end,' ');

    if (cursor_end == end) {
        return error::make_error_code(error::incomplete_status_line);
    }

    int code;

    std::istringstream ss(std::string(cursor_start,cursor_end));

    if ((ss >> code).fail()) {
        return error::make_error_code(error::incomplete_status_line);
    }

    // todo: validation of status code? Technically there are limits on what
    // status codes can be. Right now we follow Postel's law and check only
    // that the valid is an integer and let the next layer decide what to do.
    // Is this reasonable or should we be more aggressive?

    // validation of the status message will pass through
    return set_status(status_code::value(code),std::string(cursor_end+1,end));
}

inline size_t response::process_body(char const * buf, size_t len, lib::error_code & ec) {
	size_t processed = 0;
	do
	{
		const size_t processed_chunk = parser::process_body(buf, len, ec);
		buf += processed_chunk;
		len -= processed_chunk;
		processed += processed_chunk;
	} while (!ec && m_body_bytes_needed && len);

    if (ec || m_body_bytes_needed == 0)
		m_state = state::DONE;

	return processed;
}

} // namespace parser
} // namespace http
} // namespace websocketpp

#endif // HTTP_PARSER_RESPONSE_IMPL_HPP
