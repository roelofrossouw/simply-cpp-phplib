// native_time: PHP's max_execution_time paused around native work; time_limit: a native call's own.

#include "php_glue.h"

#include <main/php_globals.h>
#include <zend_ini.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <sys/time.h>
#include <time.h>

namespace sc::php {
    namespace {
        // The seconds PHP's execution timer has left, 0 when it isn't running. PHP sets it in
        // zend_set_timeout_ex(): a per-thread POSIX timer where it has those (ZTS on Linux), else
        // setitimer() - ITIMER_PROF (CPU time) on Linux, ITIMER_REAL on macOS, where ITIMER_PROF
        // isn't reliable.
        double timer_left() {
#if defined(ZEND_MAX_EXECUTION_TIMERS)
            struct itimerspec left{};
            if (timer_gettime(EG(max_execution_timer_timer), &left) != 0) return 0;
            return static_cast<double>(left.it_value.tv_sec) + static_cast<double>(left.it_value.tv_nsec) / 1e9;
#else
            struct itimerval left{};
#if defined(__APPLE__) || defined(__CYGWIN__)
            if (getitimer(ITIMER_REAL, &left) != 0) return 0;
#else
            if (getitimer(ITIMER_PROF, &left) != 0) return 0;
#endif
            return static_cast<double>(left.it_value.tv_sec) + static_cast<double>(left.it_value.tv_usec) / 1e6;
#endif
        }
    }

    native_time::native_time() {
        // Nothing to pause: no limit, or its time is up already (PHP then stops at its next chance).
        if (EG(timeout_seconds) <= 0 || zend_atomic_bool_load(&EG(timed_out))) return;
        const double left = timer_left();
        if (left <= 0) return;
        seconds_ = EG(timeout_seconds);
        left_ = left;
        zend_unset_timeout();
    }

    native_time::~native_time() {
        if (seconds_ == 0) return;
        // Going again with what was left (whole seconds, at least one); zend_set_timeout() also
        // takes it as the limit PHP names in its error, which stays the one set.
        zend_set_timeout(std::max<zend_long>(1, static_cast<zend_long>(std::ceil(left_))), false);
        EG(timeout_seconds) = seconds_;
    }

    double default_time_limit() {
        const char *value = zend_ini_string_ex("sc.time_limit", sizeof("sc.time_limit") - 1, 0, nullptr);
        if (!value || !*value) return 300;
        const double seconds = zend_strtod(value, nullptr);
        return std::isfinite(seconds) && seconds > 0 ? seconds : 0;
    }

    time_limit::time_limit(std::string what) : time_limit(std::move(what), default_time_limit()) {
    }

    time_limit::time_limit(std::string what, const double seconds) : what_(std::move(what)), seconds_(seconds) {
        if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument{what_ + ": 'time_limit' must be 0 or more seconds"};
        restart();
    }

    void time_limit::restart() {
        end_ = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds_));
    }

    void time_limit::check() const {
        if ((PG(connection_status) & PHP_CONNECTION_ABORTED) && !PG(ignore_user_abort))
            throw std::runtime_error{what_ + ": stopped, the client has gone"};
        if (seconds_ > 0 && std::chrono::steady_clock::now() > end_) {
            char limit[32];
            std::snprintf(limit, sizeof limit, "%g", seconds_);
            throw std::runtime_error{what_ + ": stopped after its time limit of " + limit + " s (the option 'time_limit', or sc.time_limit)"};
        }
    }
}
