#pragma once

// An area is one file: its functions (zend_function_entry, ending with ZEND_FE_END) and, when it
// has classes, a startup() that registers them. sc_php.cpp lists the areas; adding one is a file
// and a line there.

#include <php.h>
#include <zend_exceptions.h>

#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace sc::php {
    extern zend_class_entry *exception_class;

    // Runs body; a C++ exception becomes an SC\Exception. PHP's own errors don't longjmp out of
    // this (since PHP 8 argument errors are exceptions), so C++ objects in body are destroyed
    // as usual. After it, a function returns when EG(exception) is set (RETURN_THROWS()).
    template<typename Body>
    void guarded(Body body) {
        try {
            body();
        } catch (const std::exception &error) {
            zend_throw_exception(exception_class, error.what(), 0);
        } catch (...) {
            zend_throw_exception(exception_class, "unknown error", 0);
        }
    }

    // guarded(make), then the string it made as the function's return value.
    template<typename Make>
    void return_guarded_string(zval *return_value, Make make) {
        std::string out;
        guarded([&] { out = make(); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_STRINGL(out.data(), out.size());
    }

    // PHP's max_execution_time paused while native work runs (native_time.cpp). PHP can't stop C++
    // when its time is up: it waits hard_timeout and then ends the whole process (exit 124, a
    // php-fpm worker gone, the request a 503). So for a long call (a render, a merge, a request)
    // its timer is stopped here and set going again afterwards with the time it had left: the PHP
    // code around the call stays limited as before. A call's own limit is time_limit below.
    class native_time {
    public:
        native_time();
        ~native_time();
        native_time(const native_time &) = delete;
        native_time &operator=(const native_time &) = delete;

    private:
        zend_long seconds_ = 0; // EG(timeout_seconds) as it was; 0: nothing paused
        double left_ = 0;       // seconds its timer had left
    };

    // guarded(body) with PHP's timer paused (native_time).
    template<typename Body>
    void guarded_native(Body body) {
        native_time paused;
        guarded(body);
    }

    // A native call's own time limit (wall clock): the ini setting sc.time_limit (seconds, default
    // 300; 0 for none), or seconds given. check() throws once it has passed, or when the client has
    // gone (a connection PHP found aborted, unless ignore_user_abort): a runaway call stops with an
    // SC\Exception, not a worker that hangs.
    class time_limit {
    public:
        explicit time_limit(std::string what);                // sc.time_limit
        time_limit(std::string what, double seconds);         // 0: none
        void restart();                                       // counting again from now
        void check() const;
        [[nodiscard]] double seconds() const { return seconds_; }

    private:
        std::string what_;
        double seconds_ = 0;
        std::chrono::steady_clock::time_point end_;
    };

    // The ini setting sc.time_limit, in seconds.
    double default_time_limit();

    inline std::string_view view(const zend_string *text) { return {ZSTR_VAL(text), ZSTR_LEN(text)}; }

    inline zval *find(HashTable *table, const std::string_view key) { return zend_hash_str_find(table, key.data(), key.size()); }

    // A value as text (numbers and booleans converted, as PHP would).
    inline std::string text_of(zval *value) {
        zend_string *text = zval_get_string(value);
        std::string out{view(text)};
        zend_string_release(text);
        return out;
    }

    struct area {
        const zend_function_entry *functions; // ending with ZEND_FE_END; nullptr for none
        void (*startup)();                    // registers its classes; nullptr for none
    };

    area base64_area();
    area svg_area();
    area html2pdf_area();
    area pdf_area();
    area excel_area();
    area passkey_area();
    area bootstrap_email_area();
    area qr_code_area();
    area rest_area();
    area mail_area();
}
