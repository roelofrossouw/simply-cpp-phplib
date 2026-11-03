// SC\Mail: sc-mime's sc::mime::message (libsc-mime's shared library, from its package): an email
// read in memory, .eml or Outlook .msg, with nothing written next to it.
//
//     $mail = SC\Mail::open('/var/www/data/efile/x.eml');      // or .msg; or SC\Mail::fromString($bytes)
//     $mail->subject(); $mail->from();                          // [['name' => 'Ann', 'address' => 'ann@example.com']]
//     $html = $mail->html() ?? nl2br(htmlspecialchars($mail->text() ?? ''));
//     foreach ($mail->parts() as $part)                         // label, type, charset, filename, disposition, ...
//         if ($part['attachment']) echo $part['label'], ' ', $part['filename'], ' ', $part['size'];
//     $bytes = $mail->part('1.2');                              // one part's content, decoded
//
// Labels number the parts as mailparse does: the message's top part is "1", a multipart's parts
// "1.1", "1.2", ..., and an attached message's top part one level below it ("1.2.1"). A message
// that can't be read, or a label that isn't one, is an SC\Exception.

#include "php_glue.h"

#include <sc_mime.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    zend_class_entry *mail_class = nullptr;
    zend_object_handlers handlers;

    struct mail_object {
        sc::mime::message *mail;
        zend_object std;
    };

    mail_object *object_of(zend_object *object) {
        return reinterpret_cast<mail_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(mail_object, std));
    }

    zend_object *create(zend_class_entry *type) {
        auto *made = static_cast<mail_object *>(zend_object_alloc(sizeof(mail_object), type));
        made->mail = nullptr;
        zend_object_std_init(&made->std, type);
        object_properties_init(&made->std, type);
        made->std.handlers = &handlers;
        return &made->std;
    }

    void free_object(zend_object *object) {
        delete object_of(object)->mail;
        zend_object_std_dtor(object);
    }

    const sc::mime::message &mail_of(zval *self) {
        const auto *mail = object_of(Z_OBJ_P(self))->mail;
        if (!mail) throw std::logic_error{"SC\\Mail: not opened (SC\\Mail::open() or SC\\Mail::fromString())"};
        return *mail;
    }

    // A new SC\Mail holding mail, as the return value.
    void return_mail(zval *return_value, sc::mime::message mail) {
        object_init_ex(return_value, mail_class);
        object_of(Z_OBJ_P(return_value))->mail = new sc::mime::message(std::move(mail));
    }

    struct labelled {
        std::string label;
        sc::mime::part part;
    };

    // Every part with its label, in order: a multipart's parts below it, an attached message's top
    // part below that one.
    void walk(const sc::mime::part &part, const std::string &label, std::vector<labelled> &out, const int depth) {
        if (depth > 20) return;
        out.push_back({label, part});
        if (part.is_multipart()) {
            const auto children = part.parts();
            for (std::size_t i = 0; i < children.size(); ++i) walk(children[i], label + "." + std::to_string(i + 1), out, depth + 1);
        } else if (part.is_message()) {
            if (const auto inner = part.message()) walk(inner->body(), label + ".1", out, depth + 1);
        }
    }

    std::vector<labelled> labelled_parts(const sc::mime::message &mail) {
        std::vector<labelled> out;
        walk(mail.body(), "1", out, 0);
        return out;
    }

    sc::mime::part part_at(const sc::mime::message &mail, const std::string &label) {
        for (auto &each: labelled_parts(mail))
            if (each.label == label) return each.part;
        throw std::out_of_range{"SC\\Mail: there's no part " + label};
    }

    const char *disposition_name(const sc::mime::disposition how) {
        switch (how) {
            case sc::mime::disposition::inline_: return "inline";
            case sc::mime::disposition::attachment: return "attachment";
            default: return nullptr;
        }
    }

    void add_optional(zval *array, const char *key, const std::optional<std::string> &value) {
        if (value) add_assoc_stringl(array, key, value->data(), value->size());
        else add_assoc_null(array, key);
    }

    // [['name' => ..., 'address' => ...], ...]
    void return_mailboxes(zval *return_value, const std::vector<sc::mime::mailbox> &list) {
        array_init_size(return_value, static_cast<uint32_t>(list.size()));
        for (const auto &each: list) {
            zval entry;
            array_init_size(&entry, 2);
            add_assoc_stringl(&entry, "name", each.name.data(), each.name.size());
            add_assoc_stringl(&entry, "address", each.address.data(), each.address.size());
            add_next_index_zval(return_value, &entry);
        }
    }

    // --- opening ---

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_open, 0, 1, SC\\Mail, 0)
        ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(mail_open) {
        zend_string *path = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_PATH_STR(path)
        ZEND_PARSE_PARAMETERS_END();
        std::optional<sc::mime::message> mail;
        sc::php::guarded([&] { mail.emplace(sc::mime::message::load(std::string{sc::php::view(path)})); });
        if (EG(exception)) RETURN_THROWS();
        return_mail(return_value, std::move(*mail));
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_from_string, 0, 1, SC\\Mail, 0)
        ZEND_ARG_TYPE_INFO(0, bytes, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(mail_from_string) {
        zend_string *bytes = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(bytes)
        ZEND_PARSE_PARAMETERS_END();
        std::optional<sc::mime::message> mail;
        sc::php::guarded([&] { mail.emplace(sc::mime::message::parse(sc::php::view(bytes))); });
        if (EG(exception)) RETURN_THROWS();
        return_mail(return_value, std::move(*mail));
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_rtf, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, rtf, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(mail_rtf_to_html) {
        zend_string *rtf = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(rtf)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] { return sc::mime::rtf_to_html(sc::php::view(rtf)); });
    }

    // --- the message ---

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_string, 0, 0, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(mail_subject) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return mail_of(ZEND_THIS).subject(); });
    }

    ZEND_NAMED_FUNCTION(mail_message_id) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return mail_of(ZEND_THIS).message_id(); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_array, 0, 0, IS_ARRAY, 0)
    ZEND_END_ARG_INFO()

    template<typename Get>
    void mailboxes(zval *self, zval *return_value, Get get) {
        std::vector<sc::mime::mailbox> list;
        sc::php::guarded([&] { list = get(mail_of(self)); });
        if (EG(exception)) RETURN_THROWS();
        return_mailboxes(return_value, list);
    }

    ZEND_NAMED_FUNCTION(mail_from) {
        ZEND_PARSE_PARAMETERS_NONE();
        mailboxes(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.from(); });
    }

    ZEND_NAMED_FUNCTION(mail_to) {
        ZEND_PARSE_PARAMETERS_NONE();
        mailboxes(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.to(); });
    }

    ZEND_NAMED_FUNCTION(mail_cc) {
        ZEND_PARSE_PARAMETERS_NONE();
        mailboxes(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.cc(); });
    }

    ZEND_NAMED_FUNCTION(mail_bcc) {
        ZEND_PARSE_PARAMETERS_NONE();
        mailboxes(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.bcc(); });
    }

    ZEND_NAMED_FUNCTION(mail_reply_to) {
        ZEND_PARSE_PARAMETERS_NONE();
        mailboxes(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.reply_to(); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_date, 0, 0, IS_LONG, 1)
    ZEND_END_ARG_INFO()

    // Seconds since 1970, null when it has no date.
    ZEND_NAMED_FUNCTION(mail_date) {
        ZEND_PARSE_PARAMETERS_NONE();
        std::optional<long long> when;
        sc::php::guarded([&] {
            if (const auto date = mail_of(ZEND_THIS).date()) when = date->as_unix();
        });
        if (EG(exception)) RETURN_THROWS();
        if (!when) RETURN_NULL();
        RETURN_LONG(static_cast<zend_long>(*when));
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_header, 0, 1, IS_STRING, 1)
        ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(mail_header) {
        zend_string *name = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(name)
        ZEND_PARSE_PARAMETERS_END();
        std::optional<std::string> value;
        sc::php::guarded([&] { value = mail_of(ZEND_THIS).header(sc::php::view(name)); });
        if (EG(exception)) RETURN_THROWS();
        if (!value) RETURN_NULL();
        RETURN_STRINGL(value->data(), value->size());
    }

    ZEND_NAMED_FUNCTION(mail_headers) {
        ZEND_PARSE_PARAMETERS_NONE();
        std::vector<sc::mime::header> list;
        sc::php::guarded([&] { list = mail_of(ZEND_THIS).headers(); });
        if (EG(exception)) RETURN_THROWS();
        array_init_size(return_value, static_cast<uint32_t>(list.size()));
        for (const auto &each: list) {
            zval entry;
            array_init_size(&entry, 2);
            add_assoc_stringl(&entry, "name", each.name.data(), each.name.size());
            add_assoc_stringl(&entry, "value", each.value.data(), each.value.size());
            add_next_index_zval(return_value, &entry);
        }
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_optional_string, 0, 0, IS_STRING, 1)
    ZEND_END_ARG_INFO()

    template<typename Get>
    void optional_text(zval *self, zval *return_value, Get get) {
        std::optional<std::string> text;
        sc::php::guarded([&] { text = get(mail_of(self)); });
        if (EG(exception)) RETURN_THROWS();
        if (!text) RETURN_NULL();
        RETURN_STRINGL(text->data(), text->size());
    }

    ZEND_NAMED_FUNCTION(mail_text) {
        ZEND_PARSE_PARAMETERS_NONE();
        optional_text(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.text(); });
    }

    ZEND_NAMED_FUNCTION(mail_html) {
        ZEND_PARSE_PARAMETERS_NONE();
        optional_text(ZEND_THIS, return_value, [](const sc::mime::message &m) { return m.html(); });
    }

    // --- the parts ---

    struct part_info {
        std::string label, type;
        std::optional<std::string> charset, filename, content_id;
        const char *disposition = nullptr;
        std::size_t size = 0;
        bool multipart = false, message = false, attachment = false;
    };

    part_info info_of(const labelled &each) {
        part_info out;
        out.label = each.label;
        const auto type = each.part.type();
        out.type = type.name();
        out.charset = type.parameter("charset");
        out.filename = each.part.filename();
        const auto id = each.part.content_id();
        if (!id.empty()) out.content_id = id;
        out.disposition = disposition_name(each.part.disposition());
        out.multipart = each.part.is_multipart();
        out.message = each.part.is_message();
        out.attachment = each.part.is_attachment();
        if (!out.multipart) out.size = each.part.data().size();
        return out;
    }

    void put_part(zval *array, const part_info &info) {
        add_assoc_stringl(array, "label", info.label.data(), info.label.size());
        add_assoc_stringl(array, "type", info.type.data(), info.type.size());
        add_optional(array, "charset", info.charset);
        add_optional(array, "filename", info.filename);
        if (info.disposition) add_assoc_string(array, "disposition", info.disposition);
        else add_assoc_null(array, "disposition");
        add_optional(array, "content_id", info.content_id);
        add_assoc_long(array, "size", static_cast<zend_long>(info.size));
        add_assoc_bool(array, "multipart", info.multipart);
        add_assoc_bool(array, "message", info.message);
        add_assoc_bool(array, "attachment", info.attachment);
    }

    template<typename Keep>
    void list_parts(zval *self, zval *return_value, Keep keep) {
        std::vector<part_info> infos;
        sc::php::guarded([&] {
            for (const auto &each: labelled_parts(mail_of(self)))
                if (keep(each.part)) infos.push_back(info_of(each));
        });
        if (EG(exception)) RETURN_THROWS();
        array_init_size(return_value, static_cast<uint32_t>(infos.size()));
        for (const auto &info: infos) {
            zval entry;
            array_init(&entry);
            put_part(&entry, info);
            add_next_index_zval(return_value, &entry);
        }
    }

    // Every part: label, type, charset, filename, disposition, content_id, size (decoded bytes; 0
    // for a multipart), multipart, message, attachment.
    ZEND_NAMED_FUNCTION(mail_parts) {
        ZEND_PARSE_PARAMETERS_NONE();
        list_parts(ZEND_THIS, return_value, [](const sc::mime::part &) { return true; });
    }

    // The files: what's an attachment rather than body text (inline pictures and attached messages
    // too), not inside an attached message.
    ZEND_NAMED_FUNCTION(mail_attachments) {
        ZEND_PARSE_PARAMETERS_NONE();
        std::vector<part_info> infos;
        sc::php::guarded([&] {
            const auto all = labelled_parts(mail_of(ZEND_THIS));
            std::vector<std::string> inside; // the labels of attached messages: their parts are theirs
            for (const auto &each: all) {
                const bool nested = std::any_of(inside.begin(), inside.end(), [&](const std::string &m) { return each.label.starts_with(m + "."); });
                if (nested) continue;
                if (each.part.is_message()) inside.push_back(each.label);
                if (each.part.is_attachment()) infos.push_back(info_of(each));
            }
        });
        if (EG(exception)) RETURN_THROWS();
        array_init_size(return_value, static_cast<uint32_t>(infos.size()));
        for (const auto &info: infos) {
            zval entry;
            array_init(&entry);
            put_part(&entry, info);
            add_next_index_zval(return_value, &entry);
        }
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_part, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, label, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    // A part's content, its transfer encoding undone: a picture's or a PDF's bytes, text in its own
    // charset (partText() for UTF-8), an attached message's whole text.
    ZEND_NAMED_FUNCTION(mail_part) {
        zend_string *label = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(label)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] {
            const auto part = part_at(mail_of(ZEND_THIS), std::string{sc::php::view(label)});
            if (part.is_multipart()) throw std::invalid_argument{"SC\\Mail: part " + std::string{sc::php::view(label)} + " is a multipart; its parts have content"};
            return part.data();
        });
    }

    // A text part's content in UTF-8, "\n" line ends; RTF (text/rtf, application/rtf) as HTML.
    ZEND_NAMED_FUNCTION(mail_part_text) {
        zend_string *label = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(label)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] {
            const std::string wanted{sc::php::view(label)};
            const auto part = part_at(mail_of(ZEND_THIS), wanted);
            if (part.is("text/rtf") || part.is("application/rtf")) return sc::mime::rtf_to_html(part.data());
            if (!part.is("text")) throw std::invalid_argument{"SC\\Mail: part " + wanted + " isn't text (" + part.type().name() + ")"};
            return part.text();
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_message, 0, 1, SC\\Mail, 0)
        ZEND_ARG_TYPE_INFO(0, label, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    // An attached message (message/rfc822) as an SC\Mail of its own.
    ZEND_NAMED_FUNCTION(mail_message) {
        zend_string *label = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(label)
        ZEND_PARSE_PARAMETERS_END();
        std::optional<sc::mime::message> inner;
        sc::php::guarded([&] {
            const std::string wanted{sc::php::view(label)};
            const auto part = part_at(mail_of(ZEND_THIS), wanted);
            inner = part.message();
            if (!inner) throw std::invalid_argument{"SC\\Mail: part " + wanted + " isn't an attached message"};
        });
        if (EG(exception)) RETURN_THROWS();
        return_mail(return_value, std::move(*inner));
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(open, mail_open, arginfo_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(fromString, mail_from_string, arginfo_from_string, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(rtfToHtml, mail_rtf_to_html, arginfo_rtf, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(subject, mail_subject, arginfo_string, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(from, mail_from, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(to, mail_to, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(cc, mail_cc, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(bcc, mail_bcc, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(replyTo, mail_reply_to, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(date, mail_date, arginfo_date, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(messageId, mail_message_id, arginfo_string, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(header, mail_header, arginfo_header, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(headers, mail_headers, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(text, mail_text, arginfo_optional_string, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(html, mail_html, arginfo_optional_string, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(parts, mail_parts, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(attachments, mail_attachments, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(part, mail_part, arginfo_part, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(partText, mail_part_text, arginfo_part, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(message, mail_message, arginfo_message, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Mail", methods);
        mail_class = zend_register_internal_class(&type);
        mail_class->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NOT_SERIALIZABLE;
        mail_class->create_object = create;
        std::memcpy(&handlers, zend_get_std_object_handlers(), sizeof(handlers));
        handlers.offset = XtOffsetOf(mail_object, std);
        handlers.free_obj = free_object;
        handlers.clone_obj = nullptr;
    }
}

sc::php::area sc::php::mail_area() { return {nullptr, startup}; }
