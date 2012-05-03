#include <cstdlib>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <map>
#include "parser.hpp"


namespace parserlib {


//internal map from rules to parse procs
typedef std::map<rule *, parse_proc> _parse_proc_map_t;


//the one and only parse proc map.
static _parse_proc_map_t *_parse_proc_map = 0;


//on exit, it deletes the parse proc map
static void _delete_parse_proc_map() {
    delete _parse_proc_map;
}


//initializes the parse proc map.
static void _init_parse_proc_map() {
    if (_parse_proc_map) return;
    _parse_proc_map = new _parse_proc_map_t;
    atexit(_delete_parse_proc_map);
}


//get the parse proc from the map
static parse_proc _get_parse_proc(rule *r) {
    if (!_parse_proc_map) return 0;
    _parse_proc_map_t::iterator it = _parse_proc_map->find(r);
    if (it == _parse_proc_map->end()) return 0;
    return it->second;
}


//internal private class that manages access to the public classes' internals.
class _private {
public:
    //get the internal expression object from the expression.
    static _expr *get_expr(const expr &e) {
        return e.m_expr;
    }

    //create new expression from given expression
    static expr construct_expr(_expr *e) {
        return e;
    }

    //get the internal expression object from the rule.
    static _expr *get_expr(rule &r) {
        return r.m_expr;
    }

    //get the internal parse proc from the rule.
    static parse_proc get_parse_proc(rule &r) {
        return r.m_parse_proc;
    }
};


class _context;


//parser state
class _state {
public:
    //position
    pos m_pos;

    //size of match vector
    size_t m_matches;

    //constructor
    _state(_context &con);
};


//match
class _match {
public:
    //rule matched
    rule *m_rule;

    //begin position
    pos m_begin;

    //end position
    pos m_end;

    //null constructor
    _match() {}

    //constructor from parameters
    _match(rule *r, const pos &b, const pos &e) :
        m_rule(r),
        m_begin(b),
        m_end(e)
    {
    }
};


//match vector
typedef std::vector<_match> _match_vector;


//parsing context
class _context {
public:
    //rule that parses whitespace
    rule &m_ws;

    //current position
    pos m_pos;

    //error position
    pos m_error_pos;
    
    //input begin
    input::iterator m_begin;

    //input end
    input::iterator m_end;

    //matches
    _match_vector m_matches;

    //constructor
    _context(input &i, rule &ws) :
        m_ws(ws),
        m_pos(i),
        m_error_pos(i),
        m_begin(i.begin()),
        m_end(i.end())
    {
    }

    //check if the end is reached
    bool end() const {
        return m_pos.m_it == m_end;
    }

    //get the current symbol
    int symbol() const {
        assert(!end());
        return *m_pos.m_it;
    }

    //set the longest possible error
    void set_error_pos() {
        if (m_pos.m_it > m_error_pos.m_it) {
            m_error_pos = m_pos;
        }
    }

    //next column
    void next_col() {
        ++m_pos.m_it;
        ++m_pos.m_col;
    }

    //next line
    void next_line() {
        ++m_pos.m_line;
        m_pos.m_col = 1;
    }

    //restore the state
    void restore(const _state &st) {
        m_pos = st.m_pos;
        m_matches.resize(st.m_matches);
    }

    //parse non-term rule.
    bool parse_non_term(rule &r);

    //parse term rule.
    bool parse_term(rule &r);

    //parse whitespace terminal
    bool parse_ws() { return parse_term(m_ws); }

    //execute all the parse procs
    void do_parse_procs(void *d) const {
        for(_match_vector::const_iterator it = m_matches.begin();
            it != m_matches.end();
            ++it)
        {
            const _match &m = *it;
            parse_proc p = _private::get_parse_proc(*m.m_rule);
            p(m.m_begin, m.m_end, d);
        }
    }
    
private:
    //parse non-term rule.
    bool _parse_non_term(rule &r);

    //parse term rule.
    bool _parse_term(rule &r);    
};


//base class for expressions
class _expr {
public:
    //destructor.
    virtual ~_expr() {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const = 0;

    //parse terminal
    virtual bool parse_term(_context &con) const = 0;
};


//single character expression.
class _char : public _expr {
public:
    //constructor.
    _char(int c) :
        m_char(c)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return _parse(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        return _parse(con);
    }

private:
    //character
    int m_char;

    //internal parse
    bool _parse(_context &con) const {
        if (!con.end()) {
            int ch = con.symbol();
            if (ch == m_char) {
                con.next_col();
                return true;
            }
        }
        con.set_error_pos();
        return false;
    }
};


//string expression.
class _string : public _expr {
public:
    //constructor from ansi string.
    _string(const char *s) :
        m_string(s, s + strlen(s))
    {
    }

    //constructor from wide string.
    _string(const wchar_t *s) :
        m_string(s, s + wcslen(s))
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return _parse(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        return _parse(con);
    }

private:
    //string
    std::vector<int> m_string;

    //parse the string
    bool _parse(_context &con) const {
        for(std::vector<int>::const_iterator it = m_string.begin(),
            end = m_string.end();;)
        {
            if (it == end) return true;
            if (con.end()) break;
            if (con.symbol() != *it) break;
            ++it;
            con.next_col();
        }
        con.set_error_pos();
        return false;
    }
};


//set expression.
class _set : public _expr {
public:
    //constructor from ansi string.
    _set(const char *s) {
        for(; *s; ++s) {
            _add(*s);
        }
    }

    //constructor from wide string.
    _set(const wchar_t *s) {
        for(; *s; ++s) {
            _add(*s);
        }
    }

    //constructor from range.
    _set(int min, int max) {
        assert(min >= 0);
        assert(min <= max);
        m_set.resize((size_t)max + 1U);
        for(; min <= max; ++min) {
            m_set[(size_t)min] = true;
        }
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return _parse(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        return _parse(con);
    }

private:
    //set is kept as an array of flags, for quick access
    std::vector<bool> m_set;

    //add character
    void _add(size_t i) {
        if (i >= m_set.size()) {
            m_set.resize(i + 1);
        }
        m_set[i] = true;
    }

    //internal parse
    bool _parse(_context &con) const {
        if (!con.end()) {
            size_t ch = con.symbol();
            if (ch < m_set.size() && m_set[ch]) {
                con.next_col();
                return true;
            }
        }
        con.set_error_pos();
        return false;
    }
};


//base class for unary expressions
class _unary : public _expr {
public:
    //constructor.
    _unary(_expr *e) :
        m_expr(e)
    {
    }

    //destructor.
    virtual ~_unary() {
        delete m_expr;
    }

protected:
    //expression
    _expr *m_expr;
};


//terminal
class _term : public _unary {
public:
    //constructor.
    _term(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return m_expr->parse_term(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        return m_expr->parse_term(con);
    }
};


//loop 0
class _loop0 : public _unary {
public:
    //constructor.
    _loop0(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        //if parsing of the first fails, restore the context and stop
        con.parse_ws();
        _state st(con);
        if (!m_expr->parse_non_term(con)) {
            con.restore(st);
            return true;
        }

        //parse the rest
        for(;;) {
            con.parse_ws();
            _state st(con);
            if (!m_expr->parse_non_term(con)) {
                con.restore(st);
                break;
            }
        }

        return true;
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        //if parsing of the first fails, restore the context and stop
        _state st(con);
        if (!m_expr->parse_term(con)) {
            con.restore(st);
            return true;
        }

        //parse the rest until no more parsing is possible
        for(;;) {
            _state st(con);
            if (!m_expr->parse_term(con)) {
                con.restore(st);
                break;
            }
        }

        return true;
    }
};


//loop 1
class _loop1 : public _unary {
public:
    //constructor.
    _loop1(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        //parse the first; if the first fails, stop
        con.parse_ws();
        if (!m_expr->parse_non_term(con)) return false;

        //parse the rest until no more parsing is possible
        for(;;) {
            con.parse_ws();
            _state st(con);
            if (!m_expr->parse_non_term(con)) {
                con.restore(st);
                break;
            }
        }

        return true;
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        //parse the first; if the first fails, stop
        if (!m_expr->parse_term(con)) return false;

        //parse the rest until no more parsing is possible
        for(;;) {
            _state st(con);
            if (!m_expr->parse_term(con)) {
                con.restore(st);
                break;
            }
        }

        return true;
    }
};


//optional
class _optional : public _unary {
public:
    //constructor.
    _optional(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        _state st(con);
        if (!m_expr->parse_non_term(con)) con.restore(st);
        return true;
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        _state st(con);
        if (!m_expr->parse_term(con)) con.restore(st);
        return true;
    }
};


//and
class _and : public _unary {
public:
    //constructor.
    _and(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        _state st(con);
        bool ok = m_expr->parse_non_term(con);
        con.restore(st);
        return ok;
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        _state st(con);
        bool ok = m_expr->parse_term(con);
        con.restore(st);
        return ok;
    }
};


//not
class _not : public _unary {
public:
    //constructor.
    _not(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        _state st(con);
        bool ok = !m_expr->parse_non_term(con);
        con.restore(st);
        return ok;
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        _state st(con);
        bool ok = !m_expr->parse_term(con);
        con.restore(st);
        return ok;
    }
};


//newline
class _nl : public _unary {
public:
    //constructor.
    _nl(_expr *e) :
        _unary(e)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        if (!m_expr->parse_non_term(con)) return false;
        con.next_line();
        return true;
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        if (!m_expr->parse_term(con)) return false;
        con.next_line();
        return true;
    }
};


//base class for binary expressions
class _binary : public _expr {
public:
    //constructor.
    _binary(_expr *left, _expr *right) :
        m_left(left), m_right(right)
    {
    }

    //destructor.
    virtual ~_binary() {
        delete m_left;
        delete m_right;
    }

protected:
    //left and right expressions
    _expr *m_left, *m_right;
};


//sequence
class _seq : public _binary {
public:
    //constructor.
    _seq(_expr *left, _expr *right) :
        _binary(left, right)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        if (!m_left->parse_non_term(con)) return false;
        con.parse_ws();
        return m_right->parse_non_term(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        if (!m_left->parse_term(con)) return false;
        return m_right->parse_term(con);
    }
};


//choice
class _choice : public _binary {
public:
    //constructor.
    _choice(_expr *left, _expr *right) :
        _binary(left, right)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        _state st(con);
        if (m_left->parse_non_term(con)) return true;
        con.restore(st);
        return m_right->parse_non_term(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        _state st(con);
        if (m_left->parse_term(con)) return true;
        con.restore(st);
        return m_right->parse_term(con);
    }
};


//reference to rule
class _ref : public _expr {
public:
    //constructor.
    _ref(rule &r) :
        m_rule(r)
    {
    }

    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return con.parse_non_term(m_rule);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        return con.parse_term(m_rule);
    }

private:
    //reference
    rule &m_rule;
};


//eof
class _eof : public _expr {
public:
    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return parse_term(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        return con.end();
    }
};


//any
class _any : public _expr {
public:
    //parse with whitespace
    virtual bool parse_non_term(_context &con) const {
        return parse_term(con);
    }

    //parse terminal
    virtual bool parse_term(_context &con) const {
        if (!con.end()) {
            con.next_col();
            return true;
        }
        con.set_error_pos();
        return false;
    }
};


//exception thrown when left recursion terminates successfully
struct _lr_ok {
    rule *m_rule;
    _lr_ok(rule *r) : m_rule(r) {}
};


//constructor
_state::_state(_context &con) :
    m_pos(con.m_pos),
    m_matches(con.m_matches.size())
{
}


//parse non-term rule.
bool _context::parse_non_term(rule &r) {
    //save the state of the rule
    rule::_state old_state = r.m_state;
    
    //success/failure result
    bool ok;
    
    //compute the new position
    size_t new_pos = m_pos.m_it - m_begin;
    
    //check if we have left recursion
    bool lr = new_pos == r.m_state.m_pos;
    
    //update the rule's state
    r.m_state.m_pos = new_pos;
    
    //handle the mode of the rule
    switch (r.m_state.m_mode) {
        //normal parse
        case rule::_PARSE:
            if (lr) {
                //first try to parse the rule by rejecting it, so alternative branches are examined                
                r.m_state.m_mode = rule::_REJECT;
                ok = _parse_non_term(r);
                
                //if the first try is successful, try accepting the rule,
                //so other elements of the sequence are parsed
                if (ok) {
                    r.m_state.m_mode = rule::_ACCEPT;
                    
                    //loop until no more parsing can be done
                    for(;;) {
                        //store the correct state, in order to backtrack if the call fails
                        _state st(*this);
                        
                        //update the rule position to the current position,
                        //because at this state the rule is resolving the left recursion
                        r.m_state.m_pos = m_pos.m_it - m_begin;
                    
                        //if parsing fails, restore the last good state and stop
                        if (!_parse_non_term(r)) {
                            restore(st);
                            break;
                        }
                    }
                    
                    //since the left recursion was resolved successfully,
                    //return via a non-local exit 
                    r.m_state = old_state;
                    throw _lr_ok(r.this_ptr());
                }
            }
            else {
                try {
                    ok = _parse_non_term(r);
                }
                catch (const _lr_ok &ex) {                
                    //since left recursions may be mutual, we must test which rule's left recursion
                    //was ended successfully
                    if (ex.m_rule == r.this_ptr()) {
                        ok = true;
                    }
                    else {
                        r.m_state = old_state;
                        throw;
                    }
                }
            }
            break;
            
        //reject the left recursive rule
        case rule::_REJECT:
            if (lr) {
                ok = false;
            }
            else {
                r.m_state.m_mode = rule::_PARSE;
                ok = _parse_non_term(r);
                r.m_state.m_mode = rule::_REJECT;
            }
            break;
            
        //accept the left recursive rule
        case rule::_ACCEPT:
            if (lr) {
                ok = true;
            }
            else {
                r.m_state.m_mode = rule::_PARSE;
                ok = _parse_non_term(r);
                r.m_state.m_mode = rule::_ACCEPT;
            }
            break;
    }
    
    //restore the rule's state
    r.m_state = old_state;
    
    return ok;
}


//parse term rule.
bool _context::parse_term(rule &r) {
    //save the state of the rule
    rule::_state old_state = r.m_state;
    
    //success/failure result
    bool ok;
    
    //compute the new position
    size_t new_pos = m_pos.m_it - m_begin;
    
    //check if we have left recursion
    bool lr = new_pos == r.m_state.m_pos;
    
    //update the rule's state
    r.m_state.m_pos = new_pos;
    
    //handle the mode of the rule
    switch (r.m_state.m_mode) {
        //normal parse
        case rule::_PARSE:
            if (lr) {
                //first try to parse the rule by rejecting it, so alternative branches are examined                
                r.m_state.m_mode = rule::_REJECT;
                ok = _parse_term(r);
                
                //if the first try is successful, try accepting the rule,
                //so other elements of the sequence are parsed
                if (ok) {
                    r.m_state.m_mode = rule::_ACCEPT;
                    
                    //loop until no more parsing can be done
                    for(;;) {
                        //store the correct state, in order to backtrack if the call fails
                        _state st(*this);
                        
                        //update the rule position to the current position,
                        //because at this state the rule is resolving the left recursion
                        r.m_state.m_pos = m_pos.m_it - m_begin;
                    
                        //if parsing fails, restore the last good state and stop
                        if (!_parse_term(r)) {
                            restore(st);
                            break;
                        }
                    }
                    
                    //since the left recursion was resolved successfully,
                    //return via a non-local exit 
                    r.m_state = old_state;
                    throw _lr_ok(r.this_ptr());
                }
            }
            else {
                try {
                    ok = _parse_term(r);
                }
                catch (const _lr_ok &ex) {                
                    //since left recursions may be mutual, we must test which rule's left recursion
                    //was ended successfully
                    if (ex.m_rule == r.this_ptr()) {
                        ok = true;
                    }
                    else {
                        r.m_state = old_state;
                        throw;
                    }
                }
            }
            break;
            
        //reject the left recursive rule
        case rule::_REJECT:
            if (lr) {
                ok = false;
            }
            else {
                r.m_state.m_mode = rule::_PARSE;
                ok = _parse_term(r);
                r.m_state.m_mode = rule::_REJECT;
            }
            break;
            
        //accept the left recursive rule
        case rule::_ACCEPT:
            if (lr) {
                ok = true;
            }
            else {
                r.m_state.m_mode = rule::_PARSE;
                ok = _parse_term(r);
                r.m_state.m_mode = rule::_ACCEPT;
            }
            break;
    }
    
    //restore the rule's state
    r.m_state = old_state;
    
    return ok;
}


//parse non-term rule internal.
bool _context::_parse_non_term(rule &r) {
    bool ok;
    if (_private::get_parse_proc(r)) {
        pos b = m_pos;
        ok = _private::get_expr(r)->parse_non_term(*this);
        if (ok) {
            m_matches.push_back(_match(r.this_ptr(), b, m_pos));
        }
    }
    else {
        ok = _private::get_expr(r)->parse_non_term(*this);
    }
    return ok;
}


//parse term rule internal.
bool _context::_parse_term(rule &r) {
    bool ok;
    if (_private::get_parse_proc(r)) {
        pos b = m_pos;
        ok = _private::get_expr(r)->parse_term(*this);
        if (ok) {
            m_matches.push_back(_match(r.this_ptr(), b, m_pos));
        }
    }
    else {
        ok = _private::get_expr(r)->parse_term(*this);
    }
    return ok;
}


//get the next position
static pos _next_pos(const pos &p) {
    pos r = p;
    ++r.m_it;
    ++r.m_col;
    return r;
}


//get syntax error
static error _syntax_error(_context &con) {
    std::wstring str = L"syntax error: ";
    str += (wchar_t)*con.m_error_pos.m_it;
    return error(con.m_error_pos, _next_pos(con.m_error_pos), ERROR_SYNTAX_ERROR);
}


//get eof error
static error _eof_error(_context &con) {
    return error(con.m_error_pos, con.m_error_pos, ERROR_INVALID_EOF);
}


/** constructor from input.
    @param i input.
 */
pos::pos(input &i) :
    m_it(i.begin()),
    m_line(1),
    m_col(1)
{
}


/** character terminal constructor.
    @param c character.
 */
expr::expr(int c) :
    m_expr(new _char(c))
{
}


/** null-terminated string terminal constructor.
    @param s null-terminated string.
 */
expr::expr(const char *s) :
    m_expr(new _string(s))
{
}


/** null-terminated wide string terminal constructor.
    @param s null-terminated string.
 */
expr::expr(const wchar_t *s) :
    m_expr(new _string(s))
{
}


/** rule reference constructor.
    @param r rule.
 */
expr::expr(rule &r) :
    m_expr(new _ref(r))
{
}


/** creates a zero-or-more loop out of this expression.
    @return a zero-or-more loop expression.
 */
expr expr::operator *() const {
    return _private::construct_expr(new _loop0(m_expr));
}


/** creates a one-or-more loop out of this expression.
    @return a one-or-more loop expression.
 */
expr expr::operator +() const {
    return _private::construct_expr(new _loop1(m_expr));
}


/** creates an optional out of this expression.
    @return an optional expression.
 */
expr expr::operator -() const {
    return _private::construct_expr(new _optional(m_expr));
}


/** creates an AND-expression.
    @return an AND-expression.
 */
expr expr::operator &() const {
    return _private::construct_expr((new _and(m_expr)));
}


/** creates a NOT-expression.
    @return a NOT-expression.
 */
expr expr::operator !() const {
    return _private::construct_expr(new _not(m_expr));
}


/** constructor.
    @param b begin position.
    @param e end position.
 */
input_range::input_range(const pos &b, const pos &e) :
    m_begin(b),
    m_end(e)
{
}


/** constructor.
    @param b begin position.
    @param e end position.
    @param t error type.
 */
error::error(const pos &b, const pos &e, int t) :
    input_range(b, e),
    m_type(t)
{
}


/** compare on begin position.
    @param e the other error to compare this with.
    @return true if this comes before the previous error, false otherwise.
 */
bool error::operator < (const error &e) const {
    return m_begin.m_it < e.m_begin.m_it;
}


/** character terminal constructor.
    @param c character.
 */
rule::rule(int c) :
    m_expr(new _char(c))
{
    m_parse_proc = _get_parse_proc(this);
}


/** null-terminated string terminal constructor.
    @param s null-terminated string.
 */
rule::rule(const char *s) :
    m_expr(new _string(s))
{
    m_parse_proc = _get_parse_proc(this);
}


/** null-terminated wide string terminal constructor.
    @param s null-terminated string.
 */
rule::rule(const wchar_t *s) :
    m_expr(new _string(s))
{
    m_parse_proc = _get_parse_proc(this);
}


/** constructor from expression.
    @param e expression.
 */
rule::rule(const expr &e) :
    m_expr(_private::get_expr(e))
{
    m_parse_proc = _get_parse_proc(this);
}


/** constructor from rule.
    @param r rule.
 */
rule::rule(rule &r) :
    m_expr(new _ref(r)),
    m_parse_proc(0)
{
    m_parse_proc = _get_parse_proc(this);
}


/** invalid constructor from rule (required by gcc).
    @param r rule.
    @exception std::logic_error always thrown.
 */
rule::rule(const rule &r) {
    throw std::logic_error("invalid operation");
}


/** deletes the internal object that represents the expression.
 */
rule::~rule() {
    delete m_expr;
}


/** creates a zero-or-more loop out of this rule.
    @return a zero-or-more loop rule.
 */
expr rule::operator *() {
    return _private::construct_expr(new _loop0(new _ref(*this)));
}


/** creates a one-or-more loop out of this rule.
    @return a one-or-more loop rule.
 */
expr rule::operator +() {
    return _private::construct_expr(new _loop1(new _ref(*this)));
}


/** creates an optional out of this rule.
    @return an optional rule.
 */
expr rule::operator -() {
    return _private::construct_expr(new _optional(new _ref(*this)));
}


/** creates an AND-expression out of this rule.
    @return an AND-expression out of this rule.
 */
expr rule::operator &() {
    return _private::construct_expr(new _and(new _ref(*this)));
}


/** creates a NOT-expression out of this rule.
    @return a NOT-expression out of this rule.
 */
expr rule::operator !() {
    return _private::construct_expr(new _not(new _ref(*this)));
}


/** sets the parse procedure.
    @param p procedure.
 */
void rule::set_parse_proc(parse_proc p) {
    assert(p);
    m_parse_proc = p;
    _init_parse_proc_map();
    (*_parse_proc_map)[this] = p;
}


/** creates a sequence of expressions.
    @param left left operand.
    @param right right operand.
    @return an expression which parses a sequence.
 */
expr operator >> (const expr &left, const expr &right) {
    return _private::construct_expr(
        new _seq(_private::get_expr(left), _private::get_expr(right)));
}


/** creates a choice of expressions.
    @param left left operand.
    @param right right operand.
    @return an expression which parses a choice.
 */
expr operator | (const expr &left, const expr &right) {
    return _private::construct_expr(
        new _choice(_private::get_expr(left), _private::get_expr(right)));
}


/** converts a parser expression into a terminal.
    @param e expression.
    @return an expression which parses a terminal.
 */
expr term(const expr &e) {
    return _private::construct_expr(
        new _term(_private::get_expr(e)));
}


/** creates a set expression from a null-terminated string.
    @param s null-terminated string with characters of the set.
    @return an expression which parses a single character out of a set.
 */
expr set(const char *s) {
    return _private::construct_expr(new _set(s));
}


/** creates a set expression from a null-terminated wide string.
    @param s null-terminated string with characters of the set.
    @return an expression which parses a single character out of a set.
 */
expr set(const wchar_t *s) {
    return _private::construct_expr(new _set(s));
}


/** creates a range expression.
    @param min min character.
    @param max max character.
    @return an expression which parses a single character out of range.
 */
expr range(int min, int max) {
    return _private::construct_expr(new _set(min, max));
}


/** creates an expression which increments the line counter
    and resets the column counter when the given expression
    is parsed successfully; used for newline characters.
    @param e expression to wrap into a newline parser.
    @return an expression that handles newlines.
 */
expr nl(const expr &e) {
    return _private::construct_expr(new _nl(_private::get_expr(e)));
}


/** creates an expression which tests for the end of input.
    @return an expression that handles the end of input.
 */
expr eof() {
    return _private::construct_expr(new _eof());
}


/** creates a not expression.
    @param expr expression.
    @return the appropriate expression.
 */
expr not(const expr &expr) {
    return !expr;
}


/** creates an and expression.
    @param expr expression.
    @return the appropriate expression.
 */
expr and(const expr &expr) {
    return &expr;
}


/** creates an expression that parses any character.
    @return the appropriate expression.
 */
expr any() {
    return _private::construct_expr(new _any());
}


/** parses the given input.
    The parse procedures of each rule parsed are executed
    before this function returns, if parsing succeeds.
    @param i input.
    @param g root rule of grammar.
    @param ws whitespace rule.
    @param el list of errors.
    @param d user data, passed to the parse procedures.
    @return true on parsing success, false on failure.
 */
bool parse(input &i, rule &g, rule &ws, error_list &el, void *d) {
    //prepare context
    _context con(i, ws);

    //parse initial whitespace
    con.parse_term(con.m_ws);

    //parse grammar
    if (!con.parse_non_term(g)) {
        el.push_back(_syntax_error(con));
        return false;
    }

    //parse whitespace at the end
    con.parse_term(con.m_ws);

    //if end is not reached, there was an error
    if (!con.end()) {
        if (con.m_error_pos.m_it < con.m_end) {
            el.push_back(_syntax_error(con));
        }
        else {
            el.push_back(_eof_error(con));
        }
        return false;
    }

    //success; execute the parse procedures
    con.do_parse_procs(d);
    return true;
}


} //namespace parserlib
