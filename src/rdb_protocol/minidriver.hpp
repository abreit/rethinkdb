#ifndef RDB_PROTOCOL_MINIDRIVER_HPP_
#define RDB_PROTOCOL_MINIDRIVER_HPP_

#include <string>
#include <vector>
#include <utility>
#include <algorithm>

#include "rdb_protocol/env.hpp"
#include "rdb_protocol/ql2.pb.h"
#include "rdb_protocol/datum.hpp"
#include "rdb_protocol/counted_term.hpp"

#if defined(__clang__)
#if __has_extension(cxx_rvalue_references)
#define RVALUE_THIS &&
#else
#define RVALUE_THIS
#endif
#elif __GNUC__ > 4 || (__GNUC__ == 4 && \
    (__GNUC_MINOR__ > 8 || (__GNUC_MINOR__ == 8 && \
                            __GNUC_PATCHLEVEL__ > 1)))
#define RVALUE_THIS &&
#else
#define RVALUE_THIS
#endif


namespace ql {

namespace r {

/** reql_t
 *
 * A thin wrapper around scoped_ptr_t<Term> that allows building Terms
 * using the ReQL syntax.
 *
 * Move semantics are used for non-const reql_t to avoid copying the inner Term.
 *
 * Example:
 *
 *  const reql_t a = expr(1);
 *  reql_t b = a + 1;
 *  reql_t c = array(1,2,3).nth(a);
 *
 * If a was not const, the third line would throw a runtime error.
 *
 **/

class reql_t {
public:

    explicit reql_t(scoped_ptr_t<Term> &&term_);
    explicit reql_t(const double val);
    explicit reql_t(const std::string &val);
    explicit reql_t(const datum_t &d);
    explicit reql_t(const counted_t<const datum_t> &d);
    explicit reql_t(const Datum &d);
    explicit reql_t(const Term &t);
    explicit reql_t(std::vector<reql_t> &&val);

    reql_t(reql_t &&other);
    reql_t &operator= (reql_t &&other);

    reql_t copy();

    template <class ... T>
    reql_t(Term_TermType type, T &&... args) : term(make_scoped<Term>()) {
        term->set_type(type);
        add_args(std::forward<T>(args) ...);
    }

    template <class ... T>
    reql_t call(Term_TermType type, T &&... args) RVALUE_THIS {
        return reql_t(type, std::move(*this), std::forward<T>(args) ...);
    }

    Term* release();

    Term &get();

    const Term &get() const;

    protob_t<Term> release_counted();

    void swap(Term &t);

#define REQL_METHOD(name, termtype)                             \
    template<class ... T>                                       \
    reql_t name(T &&... a) RVALUE_THIS                          \
    { return std::move(*this).call(Term::termtype, std::forward<T>(a) ...); }

    REQL_METHOD(operator +, ADD)
    REQL_METHOD(operator ==, EQ)
    REQL_METHOD(operator (), FUNCALL)
    REQL_METHOD(operator >, GT)
    REQL_METHOD(operator <, LT)
    REQL_METHOD(operator >=, GE)
    REQL_METHOD(operator <=, LE)
    REQL_METHOD(operator &&, ALL)
    REQL_METHOD(count, COUNT)
    REQL_METHOD(map, MAP)
    REQL_METHOD(operator [], GET_FIELD)
    REQL_METHOD(nth, NTH)
    REQL_METHOD(pluck, PLUCK)

#undef REQL_METHOD

private:

    void set_datum(const datum_t &d);

    template <class ... T>
    void add_args(T &&... args) {
        UNUSED int _[] = { (add_arg(std::forward<T>(args)), 1) ... };
    }

    template<class T>
    void add_arg(T &&a) {
        reql_t it(std::forward<T>(a));
        term->mutable_args()->AddAllocated(it.term.release());
    }

    scoped_ptr_t<Term> term;
};

/** var_t
 *
 * A reql_t representing an argument to a function.
 * var_t(id) represents the VAR(id) protobuf term but
 * represents the id variable when passed as one of the
 * first arguments to fun. When constructed from an
 * env_t, it calls gensym.
 *
 * Example:
 *
 *  const var_t x(env);
 *  reql_t inc = fun(x, x + 1);
 *
 **/

class var_t {
public:
    int id;
    explicit var_t(env_t *env);
    explicit var_t(int id_);
};

template <>
inline void reql_t::add_arg(std::pair<std::string, reql_t> &&kv) {
    auto ap = make_scoped<Term_AssocPair>();
    ap->set_key(kv.first);
    ap->mutable_val()->Swap(kv.second.term.get());
    term->mutable_optargs()->AddAllocated(ap.release());
}

template <class T>
reql_t expr(T &&d) {
    return reql_t(std::forward<T>(d));
}

reql_t var(var_t);

reql_t boolean(bool b);

reql_t fun(reql_t &&body);
reql_t fun(const var_t &a, reql_t &&body);
reql_t fun(const var_t &a, const var_t &b, reql_t &&body);

template<class ... Ts>
reql_t array(Ts &&... xs) {
    return reql_t(Term::MAKE_ARRAY, std::forward<Ts>(xs) ...);
}

reql_t null();

std::pair<std::string, reql_t> optarg(const std::string &key, reql_t &&value);

reql_t db(const std::string &name);

template <class T>
reql_t error(T &&message) {
    return reql_t(Term::ERROR, std::forward<T>(message));
}

template<class Cond, class Then, class Else>
reql_t branch(Cond &&a, Then &&b, Else &&c) {
    return reql_t(Term::BRANCH,
                  std::forward<Cond>(a),
                  std::forward<Then>(b),
                  std::forward<Else>(c));
}

} // namepsace r

} // namespace ql

#endif // RDB_PROTOCOL_MINIDRIVER_HPP_