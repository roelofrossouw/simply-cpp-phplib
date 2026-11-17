// SC\Bankserv: Bankserv (ACB) files, sc-bankserv's sc::bankserv. A bank's load report (FNB's
// FTPTCPIP.FNBLOAD.G####V00) read into its parts: the header and trailer fields with the bank's
// reasons, the received, accepted and rejected totals, the data set's result, and per transaction
// status its records (with reject codes and 1web's OWD/OWC ids) and totals. Amounts are whole cents
// (integers); text is UTF-8. A line it doesn't place is kept in 'unparsed'; nothing throws on content.
//
//     $report = SC\Bankserv::loadReport(file_get_contents($path));
//     $report = SC\Bankserv::loadReportFile($path);
//     if ($report['outcome']['rejected']) ...;
//     foreach ($report['reports'] as $section)
//         foreach ($section['records'] as $record)
//             if ($record['own_kind'] === 'OWD' && $record['reject_code'] !== null) ...;

#include "php_glue.h"

#include <sc/bankserv/load_report.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
    void add_string(zval *array, const char *key, const std::string &value) { add_assoc_stringl(array, key, value.data(), value.size()); }

    void add_optional_string(zval *array, const char *key, const std::string &value) {
        if (value.empty()) add_assoc_null(array, key);
        else add_string(array, key, value);
    }

    void add_tally(zval *array, const char *key, const sc::bankserv::tally &t) {
        zval entry;
        array_init_size(&entry, 2);
        add_assoc_long(&entry, "count", static_cast<zend_long>(t.count));
        add_assoc_long(&entry, "cents", static_cast<zend_long>(t.cents));
        add_assoc_zval(array, key, &entry);
    }

    void add_optional_tally(zval *array, const char *key, const std::optional<sc::bankserv::tally> &t) {
        if (t) add_tally(array, key, *t);
        else add_assoc_null(array, key);
    }

    void record_array(zval *out, const sc::bankserv::record &r) {
        array_init_size(out, 16);
        add_assoc_long(out, "sequence", static_cast<zend_long>(r.sequence));
        add_string(out, "name", r.name);
        add_string(out, "account_type", r.account_type);
        add_string(out, "branch", r.branch);
        add_string(out, "account", r.account);
        add_string(out, "entry_class", r.entry_class);
        add_assoc_stringl(out, "side", &r.side, 1);
        add_assoc_long(out, "cents", static_cast<zend_long>(r.cents));
        add_string(out, "action_date", r.action_date);
        add_string(out, "reference", r.reference);
        add_optional_string(out, "own_kind", r.own_kind);
        if (r.own_kind.empty()) add_assoc_null(out, "own_id");
        else add_assoc_long(out, "own_id", static_cast<zend_long>(r.own_id));
        if (r.contra_batch) add_assoc_long(out, "contra_batch", static_cast<zend_long>(*r.contra_batch));
        else add_assoc_null(out, "contra_batch");
        add_optional_string(out, "reject_code", r.reject_code);
        add_optional_string(out, "reject_description", r.reject_description);
        zval notes;
        array_init_size(&notes, static_cast<uint32_t>(r.notes.size()));
        for (const auto &n: r.notes) add_next_index_stringl(&notes, n.data(), n.size());
        add_assoc_zval(out, "notes", &notes);
    }

    void report_array(zval *out, const sc::bankserv::report &r) {
        array_init_size(out, 32);
        const auto kind = sc::bankserv::to_string(r.kind);
        add_assoc_stringl(out, "kind", kind.data(), kind.size());
        add_string(out, "title", r.title);
        add_string(out, "marker", r.marker);
        add_string(out, "company", r.company);
        add_string(out, "bank", r.bank);
        add_string(out, "branch", r.branch);
        add_string(out, "service", r.service);
        add_string(out, "run_date", r.run_date);
        add_string(out, "run_time", r.run_time);
        add_string(out, "effective_date", r.effective_date);
        add_assoc_long(out, "pages", r.pages);
        add_optional_string(out, "number", r.number);
        add_assoc_long(out, "first_line", r.first_line);

        zval fields;
        array_init_size(&fields, static_cast<uint32_t>(r.fields.size()));
        for (const auto &f: r.fields) {
            zval entry;
            array_init_size(&entry, 5);
            add_string(&entry, "name", f.name);
            add_string(&entry, "header", f.header);
            add_optional_string(&entry, "header_reason", f.header_reason);
            add_string(&entry, "trailer", f.trailer);
            add_optional_string(&entry, "trailer_reason", f.trailer_reason);
            add_next_index_zval(&fields, &entry);
        }
        add_assoc_zval(out, "fields", &fields);
        add_optional_string(out, "generation", r.generation);
        add_optional_string(out, "next_generation", r.next_generation);
        add_optional_tally(out, "received", r.received);
        add_optional_tally(out, "accepted", r.accepted);
        add_optional_tally(out, "rejected", r.rejected);
        add_optional_string(out, "data_set", r.data_set);
        zval messages;
        array_init_size(&messages, static_cast<uint32_t>(r.messages.size()));
        for (const auto &m: r.messages) {
            zval entry;
            array_init_size(&entry, 2);
            add_string(&entry, "label", m.label);
            add_string(&entry, "text", m.text);
            add_next_index_zval(&messages, &entry);
        }
        add_assoc_zval(out, "messages", &messages);

        add_optional_string(out, "status", r.status);
        add_optional_string(out, "service_type", r.service_type);
        add_optional_string(out, "customer_service", r.customer_service);
        add_optional_string(out, "user_code", r.user_code);
        add_optional_string(out, "batch", r.batch);
        add_optional_string(out, "nominated_account", r.nominated_account);
        add_optional_string(out, "nominated_branch", r.nominated_branch);
        zval records;
        array_init_size(&records, static_cast<uint32_t>(r.records.size()));
        for (const auto &rec: r.records) {
            zval entry;
            record_array(&entry, rec);
            add_next_index_zval(&records, &entry);
        }
        add_assoc_zval(out, "records", &records);
        zval summaries;
        array_init_size(&summaries, static_cast<uint32_t>(r.summaries.size()));
        for (const auto &s: r.summaries) {
            zval entry;
            array_init_size(&entry, 5);
            add_string(&entry, "level", s.level);
            add_string(&entry, "key", s.key);
            add_optional_string(&entry, "counts", s.counts);
            add_tally(&entry, "debit", s.debit);
            add_tally(&entry, "credit", s.credit);
            add_next_index_zval(&summaries, &entry);
        }
        add_assoc_zval(out, "summaries", &summaries);
    }

    void load_report_array(zval *out, const sc::bankserv::load_report &file) {
        array_init_size(out, 4);
        const auto &o = file.outcome;
        zval outcome;
        array_init_size(&outcome, 8);
        add_assoc_bool(&outcome, "realtime", o.realtime);
        add_assoc_bool(&outcome, "rejected", o.rejected);
        add_optional_string(&outcome, "user_code", o.user_code);
        add_optional_string(&outcome, "generation", o.generation);
        add_assoc_long(&outcome, "received", static_cast<zend_long>(o.received));
        add_assoc_long(&outcome, "accepted", static_cast<zend_long>(o.accepted));
        add_assoc_long(&outcome, "rejected_count", static_cast<zend_long>(o.rejected_count));
        add_assoc_long(&outcome, "adjusted", static_cast<zend_long>(o.adjusted));
        add_assoc_zval(out, "outcome", &outcome);
        zval reports;
        array_init_size(&reports, static_cast<uint32_t>(file.reports.size()));
        for (const auto &r: file.reports) {
            zval entry;
            report_array(&entry, r);
            add_next_index_zval(&reports, &entry);
        }
        add_assoc_zval(out, "reports", &reports);
        zval unparsed;
        array_init_size(&unparsed, static_cast<uint32_t>(file.unparsed.size()));
        for (const auto &u: file.unparsed) {
            zval entry;
            array_init_size(&entry, 3);
            if (u.report < 0) add_assoc_null(&entry, "report");
            else add_assoc_long(&entry, "report", u.report);
            add_assoc_long(&entry, "line", u.line);
            add_string(&entry, "text", u.text);
            add_next_index_zval(&unparsed, &entry);
        }
        add_assoc_zval(out, "unparsed", &unparsed);
        add_assoc_long(out, "lines", file.lines);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_load_report, 0, 1, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, contents, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(bankserv_load_report) {
        zend_string *contents = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(contents)
        ZEND_PARSE_PARAMETERS_END();
        sc::bankserv::load_report file;
        sc::php::guarded_native([&] { file = sc::bankserv::read_load_report(sc::php::view(contents)); });
        if (EG(exception)) RETURN_THROWS();
        load_report_array(return_value, file);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_load_report_file, 0, 1, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(bankserv_load_report_file) {
        zend_string *path = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_PATH_STR(path)
        ZEND_PARSE_PARAMETERS_END();
        sc::bankserv::load_report file;
        sc::php::guarded_native([&] {
            const std::string name{sc::php::view(path)};
            std::ifstream in{name, std::ios::binary};
            if (!in) throw std::runtime_error{"SC\\Bankserv: can't read " + name};
            std::ostringstream contents;
            contents << in.rdbuf();
            file = sc::bankserv::read_load_report(contents.str());
        });
        if (EG(exception)) RETURN_THROWS();
        load_report_array(return_value, file);
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(loadReport, bankserv_load_report, arginfo_load_report, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(loadReportFile, bankserv_load_report_file, arginfo_load_report_file, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Bankserv", methods);
        auto *bankserv_class = zend_register_internal_class(&type);
        bankserv_class->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NOT_SERIALIZABLE | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    }
}

sc::php::area sc::php::bankserv_area() { return {nullptr, startup}; }
