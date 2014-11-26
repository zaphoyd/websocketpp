#ifndef WEBSOCKETPP_LOGGER_SYSLOG_HPP
#define WEBSOCKETPP_LOGGER_SYSLOG_HPP

#include <syslog.h>

#include <websocketpp/logger/basic.hpp>

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/logger/levels.hpp>

namespace websocketpp {
namespace log {

/// Basic logger that outputs to syslog
template <typename concurrency, typename names>
class syslog : public basic<concurrency, names> {
public:
    syslog<concurrency,names>(channel_type_hint::value h =
        channel_type_hint::access)
      : basic<concurrency,names>(h), channel_type_hint_(h) {}

    syslog<concurrency,names>(level c, channel_type_hint::value h =
        channel_type_hint::access)
      : basic<concurrency,names>(c, h), channel_type_hint_(h) {}

    void write(level channel, std::string const & msg) override {
        write(channel, msg.c_str());
    }

    void write(level channel, char const * msg) override {
        scoped_lock_type lock(basic<concurrency,names>::m_lock);
        if (!this->dynamic_test(channel)) { return; }
        ::syslog(syslog_priority(channel), "[%s] %s", names::channel_name(channel), msg);
    }

private:
    typedef typename basic<concurrency,names>::scoped_lock_type scoped_lock_type;
    const int kDefaultSyslogLevel = LOG_INFO;

    const int syslog_priority(level channel) const {
        if (channel_type_hint_ == channel_type_hint::access) {
            return syslog_priority_access(channel);
        } else {
            return syslog_priority_error(channel);
        }
    }

    const int syslog_priority_error(level channel) const {
        switch (channel) {
            case elevel::devel:
                return LOG_DEBUG;
            case elevel::library:
                return LOG_DEBUG;
            case elevel::info:
                return LOG_INFO;
            case elevel::warn:
                return LOG_WARNING;
            case elevel::rerror:
                return LOG_ERR;
            case elevel::fatal:
                return LOG_CRIT;
            default:
                return kDefaultSyslogLevel;
        }
    }

    _WEBSOCKETPP_CONSTEXPR_TOKEN_ int syslog_priority_access(level channel) const {
        return kDefaultSyslogLevel;
    }

    channel_type_hint::value channel_type_hint_;
};

} // log
} // websocketpp

#endif // WEBSOCKETPP_LOGGER_SYSLOG_HPP
