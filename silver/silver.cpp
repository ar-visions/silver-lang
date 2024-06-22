#include <silver/silver.hpp>

/// use mx internally
#include <mx/mx.hpp>

namespace ion {

using string = str;

/// we have the definition of the object, and some template args.
/// this does rather mean we could allow for templates on structs and enums but thats just adding flare to something that doesnt want to stand out
/// leave it C99-compatible for those

struct silver_t:A {
    string ref_name; /// we should store this as vector<ident> or a tokens type
    object def; 
    vector<object> template_args; /// array of vector<token> replace def at design
    silver_t(null_t = null) : A(typeof(silver_t)) { }
    silver_t(string ref_name, object def, vector<object> template_args = {}) : 
        A(typeof(silver_t)), ref_name(ref_name), def(def), template_args(template_args) { }
};

struct silver {
    UA_decl(silver, silver_t)
};

UA_impl(silver, silver_t)
// i want this naming across, no more UpperCase stuff just call that upper_case_t
// the pointer has the _t; its easier to remember

/// does . this-ever-need-to-be-separate . or
struct Ident:A {
    string value;
    string fname;
    int line_num;

    Ident(null_t = null) : A(typeof(Ident)) { }
    Ident(object m, string file = "none", int line = 0) : Ident() {
        assert (m.type() == typeof(String));
        value = m.to_string();
        fname    = file;
        line_num = line;
    }
    String *to_string() override {
        return (String*)value->hold();
    }
    int compare(const object& m) override {
        bool same;
        if (m.type() == typeof(Ident)) {
            Ident  &b = m;
            bool same = value == b.value;
        } else {
            String& b = m;
            same = (String&)value == b;
        }
        return same ? 0 : -1;
    }
    u64 hash() override {
        u64 h = OFFSET_BASIS;
            h *= FNV_PRIME;
            h ^= value->hash();
        return h;
    }
    cstr cs() { return value->cs(); }
    
    Vector<string> *split(object obj) {
        return value->split(obj);
    }

    bool operator==(const string &s) const {
        return value == s;
    }
    char &operator[](int i) const { return value[i]; }
    operator bool() const { return bool(value); }
};

struct ident {
    char &operator[](int i) const { return (*a)[i]; }
    UA_decl(ident, Ident)
};
UA_impl(ident, Ident)

void assertion(bool cond, const string& m, const array& a = {}) {
    if (!cond)
        console.fault(m, a);
};

/// we want the generic, or template keyword to be handled by translation at run-time
/// we do not want the code instantiated a bunch of times; we can perform lookups at runtime for the T'emplate args
/// its probably quite important not to re-instantiate the code a bunch of times.  once only, even with templates
/// you simply host your own object-based types

/// why cant templated keywords simply be object, then?

static vector<string> keywords = { "class", "struct", "import", "return", "asm", "if", "switch", "while", "for", "do" };

struct EClass;
struct EStruct;
struct silver;
struct silver_t;

/// twins; just data and user
struct var_bind;
struct Var_Bind:A {
    ident       name;
    silver_t*   utype; /// silver types are created here from our library of modules
    bool        read_only;
    Var_Bind() : A(typeof(var_bind)) { }
};

struct var_bind {
    UA_decl(var_bind, Var_Bind);
};

UA_impl(var_bind, Var_Bind);

using var_binds = vector<var_bind>;

struct enode {
    UA_decl(enode, ENode)
};


struct emember;
struct ENode:A {
    enums(Type, Undefined,
        Undefined, 
        Statements, Assign, AssignAdd, AssignSub, AssignMul, AssignDiv, AssignOr, AssignAnd, AssignXor, AssignShiftR, AssignShiftL, AssignMod,
        If, For, While, DoWhile, Break,
        LiteralReal, LiteralInt, LiteralStr, LiteralStrInterp, Array, AlphaIdent, Var, Add, Sub, Mul, Div, Or, And, Xor, MethodCall, MethodReturn)
   
    Type    etype;
    object  value;
    vector<enode> operands;
    var_binds vars;

    static enode create_operation(Type etype, const vector<enode>& operands, var_binds vars = {}) {
        ENode* op = new ENode;
        op->etype    = etype;
        op->operands = operands;
        op->vars     = vars;
        return op;
    }

    static enode create_value(Type etype, const object& value) {
        ENode* op = new ENode;
        op->etype    = etype;
        op->value    = value;
        return op;
    }

    /// @brief invocation of enode-based method
    /// @param method - the module member's eclass's emember
    /// @param args   - array of enodes for solving the arguments
    /// @return 
    static enode method_call(vector<string> method, vector<enode> args) {
        /// each in args is an enode
        /// perhaps a value given, or perhaps a method with operand args
        ENode* op = new ENode;
        op->etype = ENode::Type::MethodCall;
        op->value = method;
        /// method->translation is an enode to the method, 
        /// however we need context of method since that has args to validate against, and perform type conversions from
        /// the arg type are there for that reason
        for (ENode* arg_op: args)
            op->operands += arg_op; // make sure the ref count increases here
        return op;
    }

    ENode() : A(typeof(ENode)) { }

    ENode(ident& id_var) : ENode() {
        etype = Type::Var;
        value = id_var;
    }

    static object lookup(const vector<map> &stack, ident id, bool top_only, bool &found) {
        for (int i = stack->len() - 1; i >= 0; i--) {
            map &m = stack[i];
            Field *f = m->fetch(id);
            if (f) {
                found = true;
                return f->value;
            }
            if (top_only)
                break;
        }
        found = false;
        return null;
    }

    static string string_interpolate(const object &m_input, const vector<map> &stack) {
        string input = string(m_input);
        string output = input->interpolate([&](const string &arg) -> string {
            ident f = arg;
            bool found = false;
            object m = lookup(stack, f, false, found);
            if (!found)
                return arg;
            return m.to_string();
        });
        return output;
    }
    
    static var exec(const enode &op, const vector<map> &stack) {
        switch (op->etype) {
            // we need 
            case Type::LiteralInt:
            case Type::LiteralReal:
            case Type::LiteralStr:
                return var(op->value);
            case Type::LiteralStrInterp:
                return var(string_interpolate(op->value, stack));
            case Type::Array: {
                array res(op->operands->len());
                for (enode& operand: op->operands) {
                    var v = exec(operand, stack);
                    res->push(v);
                }
                return var(res);
            }
            case Type::Var: {
                assert(op->value.type() == typeof(Ident));
                bool found = false;
                object m = lookup(stack, op->value, false, found);
                if (found)
                    return var(m); /// in key results, we return its field
                console.fault("variable not declared: {0}", { op->value });
                throw Type(Type::Var);
                break;
            }
            case Type::Add: return exec(op->operands[0], stack) +  exec(op->operands[1], stack);
            case Type::Sub: return exec(op->operands[0], stack) -  exec(op->operands[1], stack);
            case Type::Mul: return exec(op->operands[0], stack) *  exec(op->operands[1], stack);
            case Type::Div: return exec(op->operands[0], stack) /  exec(op->operands[1], stack);
            case Type::And: return exec(op->operands[0], stack) && exec(op->operands[1], stack);
            case Type::Or:  return exec(op->operands[0], stack) || exec(op->operands[1], stack);
            case Type::Xor: return exec(op->operands[0], stack) ^  exec(op->operands[1], stack);
            default:
                break;
        }
        return var(null);
    }

    operator bool() { return etype != null; }
};

enums(Access, Public,
    Public, Intern)

UA_impl(enode, ENode)

struct Index {
    int i;
    Index(int i) : i(i) { }
    operator bool() {
        return i >= 0;
    }
    operator int() {
        return i;
    }
};

struct TypeIdent;


struct EMember;
struct emember {
    UA_decl(emember, EMember)
};

using tokens = vector<ident>;
struct silver_t;

// clean this terrible place up  
struct EMember:A {
    enums(Type, Undefined,
        Undefined, Variable, Lambda, Method, Constructor)
    bool            is_template;
    bool            intern; /// intern is not reflectable
    bool            is_static;
    lambda<void()>  resolve;
    Type            member_type; /// this could perhaps be inferred from the members alone but its good to have an enum
    type_t          runtime; /// some allocated runtime for method translation (proxy remote of sort); needs to be a generic-lambda type
    string          name;
    silver          type;  /// for lambda, this is the return result
    string          base_class;
    string          type_string;
    vector<ident>   type_tokens;
    vector<ident>   group_tokens;
    tokens          value; /// code for methods go here; for lambda, this is the default lambda instance; lambdas of course can be undefined
    vector<emember> args;  /// args for both methods and lambda; methods will be on the class model alone, not in the 'instance' memory of the class
    var_binds       arg_vars; /// these are pushed into the parser vspace stack when translating a method; lambdas will need another set of read vars
    tokens          base_forward;
    bool            is_ctr;
    enode           translation;
    EMember() : A(typeof(EMember)) { }
    String* to_string() {
        return name; /// needs weak reference to class
    }
    operator bool() { return bool(name); }
};


UA_impl(emember, EMember)

struct Parser {
    static inline vector<ident> assign = {":", "+=", "-=", "*=", "/=", "|=", "&=", "^=", ">>=", "<<=", "%="};
    vector<ident> tokens;
    vector<var_binds> bind_stack;
    int cur = 0;

    Parser(vector<ident> tokens) : tokens(tokens) { }

    Parser(string input, string fname) {
        string        sp         = "$,<>()![]/+*:\"\'#"; /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
        char          until      = 0; /// either ) for $(script) ", ', f or i
        sz_t          len        = input->len();
        char*         origin     = input->cs();
        char*         start      = 0;
        char*         cur        = origin - 1;
        int           line_num   = 1;
        bool          new_line   = true;
        bool          token_type = false;
        bool          found_null = false;
        bool          multi_comment = false;
        tokens = vector<ident>(256 + input->len() / 8);
        ///
        while (*(++cur)) {
            bool is_ws = false;
            if (!until) {
                if (new_line)
                    new_line = false;
                /// ws does not work with new lines
                if (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
                    is_ws = true;
                    ws(cur);
                }
            }
            if (!*cur) break;
            bool add_str = false;
            char *rel = cur;
            if (*cur == '#') { // comment
                if (cur[1] == '#')
                    multi_comment = !multi_comment;
                while (*cur && *cur != '\n')
                    cur++;
                found_null = !*cur;
                new_line = true;
                until = 0; // requires further processing if not 0
            }
            if (until) {
                if (*cur == until && *(cur - 1) != '/') {
                    add_str = true;
                    until = 0;
                    cur++;
                    rel = cur;
                }
            }
            if (!until && !multi_comment) {
                int type = sp->index_of(*cur);
                new_line |= *cur == '\n';

                if (start && (is_ws || add_str || (token_type != (type >= 0) || token_type) || new_line)) {
                    tokens += parse_token(start, (sz_t)(rel - start), fname, line_num);
                    Ident *ident0 = tokens[0].a;
                    if (!add_str) {
                        if (*cur == '$' && *(cur + 1) == '(') // shell
                            until = ')';
                        else if (*cur == '"') // double-quote
                            until = '"';
                        else if (*cur == '\'') // single-quote
                            until = '\'';
                    }
                    if (new_line) {
                        start = null;
                    } else {
                        ws(cur);
                        start = cur;
                        token_type = (type >= 0);
                    }
                }
                else if (!start && !new_line) {
                    start = cur;
                    token_type = (type >= 0);
                }
            }
            if (new_line) {
                until = 0;
                line_num++;
                if (found_null)
                    break;
            }
        }
        if (start && (cur - start))
            tokens += parse_token(start, (sz_t)(cur - start), fname, line_num);
    }

    static void ws(char *&cur) {
        while (*cur == ' ' || *cur == '\t') {
            ++cur;
        }
    }

    static ident parse_token(char *start, sz_t len, string fname, int line_num) {
        while (start[len - 1] == '\t' || start[len - 1] == ' ')
            len--;
        string all { start, len };
        char t = all[0];
        bool is_number = (t == '-' || (t >= '0' && t <= '9'));
        return ident(all, fname, line_num); /// mr b: im a token!  line-num can be used for breakpoints (need the file path too)
    }

    vector<ident> parse_raw_block() {
        if (next() != "[")
            return vector<ident> { pop() };
        assertion(next() == "[", "expected beginning of block [");
        vector<ident> res;
        res += pop();
        int level = 1;
        for (;;) {
            if (next() == "[") {
                level++;
            } else if (next() == "]") {
                level--;
            }
            res += pop();
            if (level == 0)
                break;
        }
        return res;
    }

    emember parse_member(A* object, silver type_context, bool template_mode);

    /// parse members from a block
    vector<emember> parse_args(A* object, bool template_mode) {
        vector<emember> result;
        assertion(pop() == "[", "expected [ for arguments");
        /// parse all symbols at level 0 (levels increased by [, decreased by ]) until ,

        /// # [ int arg, int arg2[int, string] ]
        /// # args look like this, here we have a lambda as a 2nd arg
        /// 
        while (next() && next() != "]") {
            emember a = parse_member(object, null, template_mode); /// we do not allow type-context in args but it may be ok to try in v2
            ident n = next();
            assertion(n == "]" || n == ",", ", or ] in arguments");
            if (n == "]")
                break;
            pop();
        }
        assertion(pop() == "]", "expected end of args ]");
        return result;
    }

    ident token_at(int rel) {
        if ((cur + rel) >= tokens->len())
            return {};
        return tokens[cur + rel];
    }

    ident next() {
        return token_at(0);
    }

    ident pop() {
        if (cur >= tokens->len())
            return {};
        return tokens[cur++];
    }

    void consume() { cur++; }

    ENode::Type is_numeric(ident& token) {
        string t = token;
        return (t[0] >= '0' && t[0] <= '9') ? (strchr(t->cs(), '.') ? 
            ENode::Type::LiteralReal : ENode::Type::LiteralInt) : ENode::Type::Undefined;
    };

    ENode::Type is_string(ident& token) {
        char t = token[0];
        return t == '"' ? ENode::Type::LiteralStr : t == '\'' ? ENode::Type::LiteralStrInterp : ENode::Type::Undefined;
    };

    Index expect(ident& token, const vector<ident> &tokens) {
        Index i = tokens->index_of(token);
        return i;
    };

    ENode::Type is_alpha_ident(ident token) { /// type, var, or method (all the same); its a name that isnt a keyword
        if (!token)
            return ENode::Type(ENode::Type::Undefined);
        char t = token[0];
        if (isalpha(t) && keywords->index_of(token) == -1) {
            /// lookup against variable table; declare if in isolation
            return ENode::Type::AlphaIdent; /// will be Var, or Type after
        }
        return ENode::Type(ENode::Type::Undefined);
    };

    ident &next(int rel = 0) const {
        return tokens[cur + rel];
    }

    ENode::Type is_assign(const ident& token) {
        int id = assign->index_of(token);
        if (id >= 0)
            return ENode::Type((int)ENode::Type::Assign + id);
        return ENode::Type::Undefined;
    }

    enode parse_statements() {
        vector<enode> block;
        bool multiple = next() == "[";
        if (multiple) {
            pop(); /// add space for variables (both cases, actually, since a singular expression can create temporaries)
        }
        var_binds vars {};
        bind_stack->push(vars); /// statements alone constitute new variable space
        while (next()) {
            enode n = parse_statement();
            assertion(n, "expected statement or expression");
            block += n;
            if (!multiple)
                break;
            else if (next() == "]")
                break;
        }
        bind_stack->pop();
        if (multiple) {
            assertion(next() == "]", "expected end of block ']'");
            consume();
        }
        return ENode::create_operation(ENode::Type::Statements, block, vars);
    }

    enode parse_expression() {
        return parse_add();
    }

    enode parse_statement() {
        ident t0 = next(0);
        if (is_alpha_ident(t0)) {
            ident t1 = next(1);
            ENode::Type assign = is_assign(t1);
            if (assign) {
                consume();
                consume();
                return ENode::create_operation(assign, { t0, parse_expression() });
            } else if (t1 == "[") {
                /// method call / array lookup
                /// determine if its a method
                bool is_static;
                //type = lookup_type(t0, is_static);
                //vspaces
            }
            return parse_expression();
        } else if (t0 == "return") {
            consume();
            enode result = parse_expression();
            return ENode::create_operation(ENode::Type::MethodReturn, { result });
        } else if (t0 == "break") {
            consume();
            enode levels;
            if (next() == "[") {
                consume();
                levels = parse_expression();
                assertion(pop() == "]", "expected ] after break[expression...");
            }
            return ENode::create_operation(ENode::Type::Break, { levels });
        } else if (t0 == "for") {
            consume();
            assertion(next() == "[", "expected condition expression '['"); consume();
            enode statement = parse_statements();
            assertion(next() == ";", "expected ;"); consume();
            bind_stack->push(statement->vars);
            enode condition = parse_expression();
            assertion(next() == ";", "expected ;"); consume();
            enode post_iteration = parse_expression();
            assertion(next() == "]", "expected ]"); consume();
            enode for_block = parse_statements();
            bind_stack->pop(); /// the vspace is manually pushed above, and thus remains for the parsing of these
            enode for_statement = ENode::create_operation(ENode::Type::For, vector<enode> { statement, condition, post_iteration, for_block });
            return for_statement;
        } else if (t0 == "while") {
            consume();
            assertion(next() == "[", "expected condition expression '['"); consume();
            enode condition  = parse_expression();
            assertion(next() == "]", "expected condition expression ']'"); consume();
            enode statements = parse_statements();
            return ENode::create_operation(ENode::Type::While, { condition, statements });
        } else if (t0 == "if") {
            consume();
            assertion(next() == "[", "expected condition expression '['"); consume();
            enode condition  = parse_expression();
            assertion(next() == "]", "expected condition expression ']'"); consume();
            enode statements = parse_statements();
            enode else_statements;
            bool else_if = false;
            if (next() == "else") { /// if there is no 'if' following this, then there may be no other else's following
                consume();
                else_if = next() == "if";
                else_statements = parse_statements();
                assertion(!else_if && next() == "else", "else proceeding else");
            }
            return ENode::create_operation(ENode::Type::If, { condition, statements, else_statements });
        } else if (t0 == "do") {
            consume();
            enode statements = parse_statements();
            assertion(next() == "while", "expected while");                consume();
            assertion(next() == "[", "expected condition expression '['"); consume();
            enode condition  = parse_expression();
            assertion(next() == "]", "expected condition expression '['");
            consume();
            return ENode::create_operation(ENode::Type::DoWhile, { condition, statements });
        } else {
            return parse_expression();
        }
    }

    i64 parse_numeric(ident &token) {
        char *e;
        i64   num = strtoll(token->cs(), &e, 10);
        return num;
    }

    ENode::Type is_var(ident &token) { /// type, var, or method (all the same); its a name that isnt a keyword
        char t = token[0];
        if (isalpha(t) && keywords->index_of(token) == -1) {
            /// lookup against variable table; declare if in isolation
            return ENode::Type::Var;
        }
        return ENode::Type::Undefined;
    }

    enode parse_add() {
        enode left = parse_mult();
        while (next() == "+" || next() == "/") {
            ENode::Type etype = next() == "+" ? ENode::Type::Add : ENode::Type::Sub;
            consume();
            enode right = parse_mult();
            left = ENode::create_operation(etype, { left, right });
        }
        return left;
    }

    enode parse_mult() {
        enode left = parse_primary();
        while (next() == "*" || next() == "/") {
            ENode::Type etype = next() == "*" ? ENode::Type::Mul : ENode::Type::Div;
            consume();
            enode right = parse_primary();
            left = ENode::create_operation(etype, { left, right });
        }
        return left;
    }

    enode parse_primary() {
        ident id = next();
        console.log("parse_primary: {0}", { id });
        ENode::Type n = is_numeric(id);
        if (n) {
            ident f = next();

            cstr cs = f->cs();

            bool is_int = n == ENode::Type::LiteralInt;
            consume();
            return ENode::create_value(n, object::from_string(cs, is_int ? typeof(i64) : typeof(double)));
        }
        ENode::Type s = is_string(id);
        if (s) {
            ident f = next();
            cstr cs = f->cs();
            consume();
            struct id* t = typeof(string);
            auto v = object::from_string(cs, t);
            string str_literal = v;
            assert(str_literal->len() >= 2);
            str_literal = str_literal->mid(1, str_literal->len() - 2);
            return ENode::create_value(s, str_literal);
        }
        ENode::Type i = is_var(id); /// its a variable or method (same thing; methods consume optional args)
        if (i) {
            ident f = next();
            consume();
            return ENode::create_value(i, f); /// the entire Ident is value
        }

        if (next() == "[") {
            // now we must resolve id to a member in class
            // the id can be complex;
            // it could be my-member.another.method-call-on-instance [ another-member.another-method[] ]
            //             id -------------------------------------- args ------------------------------
            //             
            // the stack can have an emember and a current value; its reference can be a place in an enode
            // we dont exactly return yet (it was implemented before, in another module)
            vector<string> emember_path = id->split(".");
            
            assert(i == ENode::Type::MethodCall);
            consume();
            vector<enode> enode_args;
            for (;;) {
                enode op = parse_expression(); // do not read the , [verify this]
                enode_args += op;
                if (next() == ",")
                    pop();
                else
                    break;
            }
            assertion(next() == "]", "expected ] after method invocation");
            consume();

            enode method_call = ENode::method_call(emember_path, enode_args);
            return method_call;
        } else {
            assert(i != ENode::Type::MethodCall);
        }
        return {};
    }

    bool expect(const ident &token) {
        if (token != tokens[cur])
            console.fault("expected token: {0}", {token});
        return true;
    }

};

struct EProp:A {
    string          name;
    Access          access;
    vector<ident>   type;
    vector<ident>   value;

    EProp() : A(typeof(EProp)) { }
    operator bool() { return bool(name); }
};

struct eprop {
    UA_decl(eprop, EProp)
};
UA_impl(eprop, EProp)

struct EnumSymbol:A {
    string          name;
    int             value;
    EnumSymbol() : A(typeof(EnumSymbol)) { }
    EnumSymbol(string name, int value) : A(typeof(EnumSymbol)), name(name), value(value) { }
    operator bool() { return bool(name); }
};

struct enum_symbol {
    UA_decl(enum_symbol, EnumSymbol)
};
UA_impl(enum_symbol, EnumSymbol)

struct EnumDef:A {
    string              name;
    bool                intern;
    vector<enum_symbol> symbols;

    EnumDef(bool intern = false) : A(typeof(EnumDef)), intern(intern) { }

    EnumDef(Parser &parser, bool intern) : EnumDef(intern) {
        ident token_name = parser.pop();
        assertion(parser.is_alpha_ident(token_name), "expected qualified name for enum, found {0}", { token_name });
        name = token_name;
        assertion(parser.pop() == "[", "expected [ in enum statement");
        i64  prev_value = 0;
        for (;;) {
            ident symbol = parser.pop();
            ENode* exp = parser.parse_expression(); /// this will pop tokens until a valid expression is made
            if (symbol == "]")
                break;
            assertion(parser.is_alpha_ident(symbol),
                "expected identifier in enum, found {0}", { symbol });
            ident peek = parser.next();
            if (peek == ":") {
                parser.pop();
                enode enum_expr = parser.parse_expression();
                object enum_value = ENode::exec(enum_expr, {});
                assertion(enum_value.type()->traits & traits::integral,
                    "expected integer value for enum symbol {0}, found {1}", { symbol, enum_value });
                prev_value = i64(enum_value);
                assertion(prev_value >= INT32_MIN && prev_value <= INT32_MAX,
                    "integer out of range in enum {0} for symbol {1} ({2})", { name, symbol, prev_value });
            } else {
                prev_value += 1;
            }
            symbols += enum_symbol(symbol, prev_value);

        }
    }
    operator bool() { return bool(name); }
};

struct enum_def {
    UA_decl(enum_def, EnumDef)
};
UA_impl(enum_def, EnumDef)

struct emodule;
struct EModule;
struct EClass;

enums(EMembership, normal, normal, internal, inlay)

/// this is the basis for the modeling of data
/// mods use alloated reference by default, but can embed inside primitive data by specifying a primitive type
/// this lets us sub-class our basic mod types for numerics and boolean
enums(EModel, allocated,
    allocated,
    boolean_32,
    unsigned_8, unsigned_16, unsigned_32, unsigned_64,
    signed_8, signed_16, signed_32, signed_64,
    real_32, real_64, real_128)

/// this was class. now its eclass, which is very confusing compared to emodule
struct eclass { UA_decl(eclass, EClass) };
struct EClass:A {
    vector<emember> template_args; /// if nothing in here, no template args [ template ... ctr ] -- lets parse type variables
    string          name;
    bool            intern;
    EModel          model; /// model types are primitive types of bit length
    string          from; /// inherits from
    vector<emember> members;
    EModule*        module;
    EClass*         composed_last;
    silver          composed_type;  /// 
    ident           composed_ident; /// these are the classes we conform to
    type_t          base_type; /// if classes have a base type, we likely wont support members, as 'this' becomes the type of base*

    eclass composed();

    vector<ident>   friends; // these friends have to exist in the module
    type_t          runtime = null; // we create our own run-time type, in our language (this is not registered in actual A types, though)
    /// different modules can have the same named mods, but those are different
    /// of course, anyone can augment a class already made; for this we fetching the EMod made, not a new one!
    /// mod module-name.mod-inside ... override functionality this way, precedence goes closest to user

    EClass(bool intern = false)         : A(typeof(EClass)) { }
    EClass(Parser &parser, EMembership membership, vector<emember> &templ_args) : EClass(intern) { // replace intern with enum for normal, inlay, intern -- its a membership type
        /// parse class members
        assertion(parser.pop() == "class", "expected mod");
        assertion(parser.is_alpha_ident(parser.next()), "expected class identifier");
        name = parser.pop();
        template_args = templ_args;

        if (parser.next() == ":") {
            parser.consume();
            if (parser.next() == ":") {
                parser.consume();
                model = string(parser.pop());
                /// boolean-32 is a model-type, integer-i32, integer-u32, object is another (implicit); 
                /// one can inherit over a model-bound mod; this is essentially the allocation size for 
                /// its membership identity; for classes that is pointer-based, but with boolean, integers, etc we use the value alone
                /// mods have a 'model' for storage; the models change what forms its identity, by value or reference
                /// [u8 1] [u8 2]   would be values [ 1, 2 ] in a u8 array; these u8's have callable methods on them
                /// so we can objectify anything if we have facilities around how the object is referenced, inline or by allocation
                /// this means there is no reference counts on these value models
            } else
                from = parser.pop();
        }
        if (parser.next() == "[") {
            assertion(parser.pop() == "[", "expected beginning of class");
            for (;;) {
                ident t = parser.next();
                if (!t || t == "]")
                    break;
                /// expect intern, or type-name token
                bool intern = false;
                if (parser.next() == "intern") {
                    parser.pop();
                    intern = true;
                }
                bool is_static = false;
                if (parser.next() == "static") {
                    parser.pop();
                    is_static = true;
                }

                ident m0 = parser.next();
                assertion(parser.is_alpha_ident(m0), "expected type identifier");
                bool is_construct = m0 == ident(name);
                bool is_cast = false;

                if (!is_construct) {
                    is_cast = m0 == "cast";
                    if (is_cast)
                        parser.pop();
                }

                auto set_attribs = [&](emember last) {
                    last->intern = intern;
                    last->is_static = is_static;
                };
                members += parser.parse_member(this, null, false);
                emember mlast = members->last();
                set_attribs(mlast);

                for (;;) {
                    if (parser.next() != ",")
                        break;
                    assert(mlast->type);
                    parser.pop();
                    members += parser.parse_member(this, mlast->type, false);
                    set_attribs(members->last());
                }

                /// int [] name [int arg] [ ... ]
                /// int [] array
                /// int [int] map
            }
            ident n = parser.pop();
            assertion(n == "]", "expected end of class");
        }
        /// classes and structs
    }
    operator bool() { return bool(name); }

    /// called when it translates
    type_t create_method(emember& member) {
        type_t graphed = (type_t)calloc(1, sizeof(id));

        graphed->method = new method_info {
            .args      = (id**)calloc(graphed->method->arg_count, sizeof(id*)),
            .r_type    = member->runtime,
            .arg_count = member->args->len()
        };

        /// fill out args from the types we have translated the runtime for the given emember it references
        for (int a = 0; a < graphed->method->arg_count; a++) {
            assert ( member->args[a]->runtime );
            graphed->method->args[a] = member->args[a]->runtime; /// assert this is set
        }

        /// *a is the instance; in silver we only have instance, default (when called via class only -- singular instance) or otherwise by instance
        graphed->method->call = [member](void* a, object* args, int arg_count) mutable -> A* {
            object a_object = ((A*)a)->hold();
            assert(a_object.type());
            const vector<map> stack = vector<map> {
                map { field { "this", a_object } } /// A-class pointer; its 'this' location [ we must have type info on this stack value ]
            };
            A* a_result = ENode::exec(member->translation, stack);
            return a_result;
        };

        return graphed;
    }

    /// assemble a class with methods that are invoke the translation with arguments
    type_t create_type();
};
UA_impl(eclass, EClass)

struct EModuleMember:A {
    string      name;
    bool        intern;

    EModuleMember(type_t type) : A(type) { }
};

struct EStruct;

struct EStruct:EModuleMember {
    map         members;

    EStruct(bool intern = false) : EModuleMember(typeof(EStruct)) {
        EModuleMember::intern = intern;
    }
    EStruct(Parser &parser, bool intern) : EStruct(intern) {
        /// parse struct members, which includes eprops
        assertion(parser.pop() == "struct", "expected struct");
        name = parser.pop();
    }
    operator bool() { return bool(name); }
};

emember Parser::parse_member(A* obj_type, silver type_context, bool template_mode) {
    EStruct* st = null;
    EClass*  cl = null;
    string parent_name;

    if (obj_type) {
        if (obj_type->type == typeof(EStruct))
            parent_name = ((EStruct*)obj_type)->name;
        else if (obj_type->type == typeof(EClass))
            parent_name = ((EClass*)obj_type)->name;
    }
    emember result;
    bool is_ctr = false;


    if (!type_context && next() == ident(parent_name)) {
        assertion(obj_type->type == typeof(EClass), "expected class when defining constructor");
        result->name = pop();
        is_ctr = true;
    } else {
        if (type_context) {
            result->type_string = type_context->ref_name;
        } else {

            auto read_type_tokens = [&]() -> vector<ident> {
                vector<ident> res;
                for (;;) {
                    assertion(is_alpha_ident(next()), "expected type identifier");
                    res += pop();
                    if (token_at(0) == "::") {
                        res += pop();
                        continue;
                    }
                    /// keep reading tokens of alpha-numeric
                    if (token_at(0) == ":" && token_at(1) == ":") {
                        res += pop();
                        res += pop();
                        continue;
                    }
                    break;
                }
                return res;
            };

            /// read type -- they only consist of-symbols::another::and::another
            /// i dont see the real point of embedding types
            result->type_tokens = read_type_tokens();

            if (template_mode) {
                /// templates do not always define a variable name (its used for replacement)
                if (next() == ":") { /// its a useful feature to not allow the :: for backward namespace; we dont need it because we reduced our ability to code to module plane
                    pop();
                    result->group_tokens = read_type_tokens();
                } else if (is_alpha_ident(next())) {
                    /// this name is a replacement variable; we wont use them until we have expression blocks (tapestry)
                    result->name = pop();
                    if (next() == ":") {
                        pop();
                        result->group_tokens = read_type_tokens();
                    }
                }
            }
            result->type_string = "";
            for (ident &token: result->type_tokens) {
                if (result->type_string)
                    result->type_string += string(" ");
                result->type_string += string(token);
            }
        }

        if (!result->name) {
            result->type = type_context;
            assertion(is_alpha_ident(next()), "  > {1}:{2}: expected identifier for member, found {0}", { next(), next()->fname, next()->line_num });
            result->name = pop();
        }
    }
    ident n = next();
    assertion((n == "[" && is_ctr) || !is_ctr, "invalid syntax for constructor; expected [args]");
    if (n == "[") {
        // [args] this is a lambda or method
        result->args = parse_args(obj_type, false);
        //var_bind args_vars;
        //for (emember& member: result->args) {
        //    args_vars->vtypes += member->member_type;
        //    args_vars->vnames += member->name;
        //}
        if (is_ctr) {
            if (next() == ":") {
                pop();
                ident class_name = pop();
                assertion(class_name == ident(cl->from) || class_name == ident(parent_name), "invalid constructor base call");
                result->base_class = class_name; /// should be assertion checked above
                result->base_forward = parse_raw_block();
            }
            ident n = next();
            assertion(n == "[", "expected [constructor code block], found {0}", { n });
            result->member_type = EMember::Type::Constructor;
            result->value = parse_raw_block();
        } else {
            if (next() == ":" || next() != "[") {
                result->member_type = EMember::Type::Lambda;
                if (next() == ":")
                    pop();
            } else {
                result->member_type = EMember::Type::Method;
            }
            ident n = next();
            if (result->member_type == EMember::Type::Method || n == "[") {
                assertion(n == "[", "expected [method code block], found {0}", { n });
                result->value = parse_raw_block();
            }
        }
    } else if (n == ":") {
        pop();
        result->value = parse_raw_block();
    } else {
        // not assigning variable
    }

    return result;
}

struct EVar;
struct evar { UA_decl(evar, EVar) };
struct EVar:A {
    EVar(bool intern = false) : A(typeof(EVar)), intern(intern) { }
    EVar(Parser &parser, bool intern) : EVar(intern) {
        type  = parser.pop();
        assertion(parser.is_alpha_ident(type), "expected type identifier, found {0}", { type });
        name = parser.pop();
        assertion(parser.is_alpha_ident(name), "expected variable identifier, found {0}", { name });
        if (parser.next() == ":") {
            parser.consume();
            initializer = parser.parse_statements(); /// this needs to work on initializers too
            /// we need to think about the data conversion from type to type; in C++ there is operator and constructors battling
            /// i would like to not have that ambiguity, although thats a very easy ask and tall order
            /// 
        }
    }
    string      name;
    ident       type;
    enode       initializer;
    bool        intern;
    operator bool() { return bool(name); }
};
UA_impl(evar, EVar)

struct ISpec;
struct ispec {
    UA_decl(ispec, ISpec)
};
struct ISpec:A {
    bool   inlay;
    object value;
    ISpec() : A(typeof(ISpec)) { };
    ISpec(bool inlay, object value) : A(typeof(ISpec)), inlay(inlay), value(value) { };
};
UA_impl(ispec, ISpec)

struct EImport;
struct eimport { UA_decl(eimport, EImport) };
struct EImport:A {
    EImport() : A(typeof(EImport)) { }
    EImport(Parser& parser) : EImport() {
        assertion(parser.pop() == "import", "expected import");
        ident mod = parser.pop();
        ident as = parser.next();
        if (as == "as") {
            parser.consume();
            isolate_namespace = parser.pop();
        }
        //assertion(parser.is_string(mod), "expected type identifier, found {0}", { type });
        name = mod;
        assertion(parser.is_alpha_ident(name), "expected variable identifier, found {0}", { name });
    }
    string      name;
    string      isolate_namespace;
    path        module_path; // we need includes
    vector<path> includes;
    struct EModule* module;
    operator bool() { return bool(name); }
};
UA_impl(eimport, EImport)

struct eincludes { UA_decl(eincludes, EIncludes) };
struct EIncludes:A {
    EIncludes() : A(typeof(EIncludes)) { }
    EIncludes(Parser& parser) : EIncludes() {
        assertion(parser.pop() == "includes", "expected includes");
        assertion(parser.pop() == "<", "expected < after includes");
        for (;;) {
            ident inc = parser.pop();
            assertion(parser.is_alpha_ident(inc), "expected include file, found {0}", { inc });
            path include = string(inc);
            includes += include;
            if (parser.next() == ",") {
                parser.pop();
                continue;
            } else {
                assertion(parser.pop() == ">", "expected > after includes");
                break;
            }
        }
        /// read optional fields for defines, libs
    }
    vector<string>  library; /// optional library identifiers (these will omit the lib prefix & ext, as defined regularly in builds)
    vector<path> includes;
    map          defines;
    operator bool() { return bool(includes); }
};
UA_impl(eincludes, EIncludes)


/// lookup type into 

struct EModule;
struct emodule { UA_decl(emodule, EModule) };
struct EModule:A {
    EModule() : A(typeof(EModule)) { }
    vector<eimport>   imports;
    vector<eincludes> includes;
    eclass            app;
    map               implementation;
    bool              translated = false;
    hashmap           cache; /// silver_t cache
    /// resolving type involves designing, so we may choose to forge a type that wont forge at all.  thats just a null
    static silver forge_type(EModule* module_instance, string ref_name) {
        if (module_instance->cache->contains(ref_name)) {
            return module_instance->cache[ref_name]; // object translation is not heavy..
        }

        /// cannot remember if i am ready to implement this one yet.  i believe we have all enablers
        /// resolve from string to array of classes and their template args (different from a string of class::class2::class3)
        /// example type: array::int
        /// we want to have a single enode instance; no need to copy and have a 'design' context i think
        /// we would not allow for constexpr design-mode -- the idea of silver is to have less in-memory and less C99 than we would ever have in C++
        vector<silver>  resolved_args; /// element = silver_t
        int    rc = 0;
        vector<string> sp = ref_name->split("::");
        int  cursor = 0;
        int  remain = sp->len();
        auto pull_sp = [&]() mutable -> string {
            if (cursor >= sp->len()) return string();
            remain--;
            return sp[cursor++];
        };

        lambda<silver(string&)> resolve;
        resolve = [&](string& key_name) -> silver {
            /// pull the type requested, and the template args for it at depth
            string class_name = pull_sp();
            eclass class_def  = module_instance->find_class(class_name);
            assertion(class_def, "class not found: {0}", { class_name });
            int class_t_args = class_def->template_args->len();
            assertion(class_t_args <= remain, "template args mismatch");
            key_name += class_name;
            vector<object> template_args;
            for (int i = 0; i < class_t_args; i++) {
                string k;
                eclass class_from_arg = resolve(k);
                assert(k);
                template_args += class_from_arg;
                key_name += "::";
                key_name += k;
            }
            if (class_def->module) {
                if (class_def->module->cache->contains(key_name))
                    return class_def->module->cache[key_name];
            }
            silver res = silver(key_name, class_def, template_args);
            class_def->module->cache[key_name] = res;
            return res;
        };

        string key;
        silver res = resolve(key);
        assert(key);
        module_instance->cache[key] = res;
        assert(module_instance->cache->contains(key));
        assert(module_instance->cache[key] == res);
        return res;
    }

    emodule find_module(string name) {
        for (eimport &i: imports)
            if (i->name == name)
                return i->module;
        return { };
    }

    ispec find_implement(ident name) {
        EModule* m = this;
        vector<string> sp = name->split(".");
        int n_len = sp->len();
        if (n_len == 1 || n_len == 2) {
            string module_ref = n_len > 1 ? string(sp[0]) : "";
            string name_ref   = n_len > 1 ? string(sp[1]) : string(sp[0]); 
            if (module_ref) {
                for (eimport i: imports) {
                    if (i->name == name_ref && i->module)
                        return i->module->implementation->contains(name_ref) ?
                            ispec(i->module->implementation->value(name_ref)) : ispec();
                }
            }
            if (!module_ref && implementation->contains(name_ref)) {
                return implementation->value(name_ref);
            }
        }
        return ispec();
    }

    eclass find_class(ident name) {
        // for referencing classes and types:
        // its not possible to have anymore than module.class
        // thats by design and it reduces the amount that we acn be overwhelmed with
        // arbitrary trees are not good on the human brain
        ispec impl = find_implement(name);
        if (impl->value.type() == typeof(EClass))
            return eclass((const object &)impl->value);
        return eclass();
    }

    bool run() {
        /// app / run method must translate
        /// static object method(T& obj, const struct string &name, const vector<object> &args);
        /// parser must have ability to run any method we describe too.. here this is a user invocation
        if (app) {
            /// create a type called app under a namespace
            type_t app_type = app->create_type();
            object app_inst = object::create(app_type);
            object::method(app_inst, "run", {});
        } else
            printf("no app with run method defined\n");
        return true;
    }
    
    bool graph() {
        lambda<void(EModule*)> graph;
        graph = [&](EModule *module) {
            if (module->translated)
                return;
            for (eimport& import: module->imports)
                graph(import->module);

            for (Field& f: module->implementation) {
                if (f.value.type() == typeof(EClass)) {
                    eclass cl = f.value;
                    cl->module = module;
                    for (emember& member: cl->members) {
                        auto convert_enode = [&](emember& member) -> enode {
                            vector<ident> code = member->value;
                            Parser parser(code);
                            console.log("parsing {0}", { member->name });
                            parser.bind_stack->push(member->arg_vars);
                            enode n = parser.parse_statements();
                            return n;
                        };
                        /// resolve type once all modules are loaded
                        if (member->type_string)
                            member->type = EModule::forge_type(module, member->type_string);
                        if (member->member_type == EMember::Type::Method) {
                            assertion(member->value, "method {0} has no value", { member });
                            member->translation = convert_enode(member);
                        }
                    }
                    /// with this type, the silver component can be created in c++
                    /// we were going to actually make this the invocation runtime but what i want is to go direct
                    /// to translation; 
                        // silver 2.0 ... its actually a lot of runtime that i dont really want present, when we can be slimmer than that
                        // basically its for types to serve only silver land, and thats security done some-what right.

                        // cl->runtime = cl->create_type();
                        // methods are members and those members are referenced in enode
                        // #1
                        // enode has method_call, 
                        //      value:      module.implementation['class'].find_member('method')
                        //      operands:   [ arg0, arg1 as objects ]
                        //  
                    
                    /// when its methods are translated, we can construct and run this
                    /// we are creating with UA model;
                    /// any server setting for instance can get
                    /// both in-script and in-code methods with introspectable args, components, their pareants etc
                }
            }
            module->translated = true;
            /// the ENode schema is bound to the classes stored in module
        };
        for (eimport& import: imports) {
            graph(import->module);
        }
        graph(this);
        return true;
    }

    static EModule *parse(path module_path) {
        EModule *m = new EModule();
        string contents = module_path->read<string>();
        Parser parser = Parser(contents, module_path);
        
        int imports = 0;
        int includes = 0;
        bool inlay = false;
        vector<emember> templ_args;
        ///
        for (;;) {
            ident token = parser.next();
            if (!token)
                break;
            bool intern = false;
            if (token == "intern") {
                parser.consume();
                token  = parser.pop();
                intern = true;
            }
            auto push_implementation = [&](ident keyword, ident name, object value) mutable {
                assertion(!m->implementation->fetch(name),  "duplicate identifier for {0}: {1}", { keyword, name });
                m->implementation[name] = ispec(inlay, value);
                if (name == "app")
                    m->app = value;
            };

            if (token != "class" && templ_args) {
                assertion(false, "expected class after template definition");
            }

            if (token == "import") {
                assertion(!intern, "intern keyword not applicable to import");
                eimport import(parser);
                imports++;
                string import_str = "import{0}";
                string import_id = import_str->format({ imports });
                push_implementation(token, import_id, import);
                string  loc = "{1}{0}.si";
                vector<string> attempt = {"", "spec/"};
                bool exists = false;
                for (string pre: attempt) {
                    path si_path = loc->format({ import->name, pre });
                    console.log("si_path = {0}", { si_path });
                    if (!si_path->exists())
                        continue;
                    import->module_path = si_path;
                    import->module = parse(si_path); /// needs to be found relative to the location of this module
                    exists = true;
                    break;
                }
                assertion(exists, "path does not exist for silver module: {0}", { import->name });

            } else if (token == "includes") {
                assertion(!intern, "intern keyword not applicable to includes");
                eincludes includes_obj(parser);
                includes++;
                string includes_str = "includes{0}";
                string includes_id  = includes_str->format({ includes });
                push_implementation(token, includes_id, includes_obj);
            } else if (token == "enum") {
                EnumDef* edef = new EnumDef(parser, intern);
                push_implementation(token, edef->name, edef);
                edef->drop();
            } else if (token == "class") {
                EClass* cl = new EClass(parser, intern, templ_args);
                push_implementation(token, cl->name, cl);
                cl->drop();
            } else if (token == "struct") {
                EStruct* st = new EStruct(parser, intern);
                push_implementation(token, st->name, st);
                st->drop();
            } else if (token == "template") {
                parser.pop();
                /// state var we would expect to be null for any next token except for class
                templ_args = parser.parse_args(null, true); /// a null indicates a template; this would change if we allow for isolated methods in module (i dont quite want this; we would be adding more than 1 entry)
                
            } else if (token != "inlay") {
                EVar* data = new EVar(parser, intern);
                push_implementation(token, data->name, data);
                data->drop();
            } else {
                inlay = true;
                parser.pop();
            }

        }
        return m;
    }
};
UA_impl(emodule, EModule)

/// we of course need this for enum, and struct
/// structs cannot have methods in them, they are just data and 
type_t EClass::create_type() {
    if (runtime) return runtime;
    /// create our own id* 
    runtime = (type_t)calloc(1, sizeof(id));
    runtime->name = (char*)calloc(name->len() + 1, 1);
    memcpy(runtime->name, name->cs(), name->len() + 1);
    runtime->meta = new hashmap(16);
    /// we need to be able to create from any constructor provided, too; 
    /// for that we can construct by allocation, 
    /// set default properties and then call the constructor method on the instance
    for (emember &m: members) {
        if (m->member_type == EMember::Type::Method) {
            /// lookup the silver type used, for a given set of tokens
            m->type = EModule::forge_type(module, m->type_string);
            m->runtime = create_method(m);
            assert(m->runtime);
            object o_prop = prop { m->name, m->runtime, array() }; /// todo: set default args in method
            prop &test_prop = o_prop;
            assert(test_prop.type == m->runtime);
            (*runtime->meta)->set(m->name, o_prop);
        }
        /// we need meta properties
    }
    return runtime;
}

eclass EClass::composed() {
    if (composed_ident && !composed_last) {
        composed_last = module->find_class(composed_ident);
        assert(composed_last);
    }
    return composed_last;
}
}

using namespace ion;

int main(int argc, char **argv) {
    map  def     { field { "source",  path(".") } };
    map  args    { map::args(argc, argv, def, "source") };
    path source  { args["source"]  };

    emodule m = EModule::parse(source);
    m->graph();
    m->run();
    return 0;
}