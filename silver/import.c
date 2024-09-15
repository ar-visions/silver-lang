#include <import>

#define intern(I,M,...) EImport_ ## M(I, ## __VA_ARGS__)

path create_folder(silver module, cstr name, cstr sub) {
    string dir = format(
        "%o/%s%s%s", module->source_path, name, sub ? "/" : "", sub ? sub : "");
    path   res = cast(dir, path);
    call(res, make_dir);
    return res;
}

string exe_ext() {
    #ifdef _WIN32
        return str("exe");
    #endif
    return str("");
}

string shared_ext() {
#ifdef _WIN32
    return str("dll");
#elif defined(__APPLE__)
    return str("dylib");
#else
    return str("so");
#endif
}

string static_ext() {
#ifdef _WIN32
    return str("lib");
#else
    return str("a");
#endif
}

string lib_prefix() {
#ifdef _WIN32
    return str("");
#else
    return str("lib");
#endif
}

array EImport_import_list(EImport a, Tokens tokens) {
    array list = new(array);
    if (call(tokens, next_is, "[")) {
        call(tokens, consume);
        while (true) {
            Token arg = call(tokens, next);
            print("arg = %o", arg);
            if (call(arg, eq, "]")) break;
            assert (call(arg, is_string) == typeid(ELiteralStr), "expected build-arg in string literal");
            A l = call(arg, convert_literal);
            call(list, push, l);
            if (call(tokens, next_is, ",")) {
                call(tokens, consume);
                continue;
            }
            break;
        }
        assert (call(tokens, next_is, "]"), "expected ] after build flags");
        call(tokens, consume);
    } else {
        Token next = call(tokens, next);
        string l = call(next, convert_literal);
        call(list, push, l);
    }
    return list;
}

void EImport_import_fields(EImport a, Tokens tokens) {
    while (true) {
        if (call(tokens, next_is, "]")) {
            call(tokens, consume);
            break;
        }
        Token arg_name = call(tokens, next);
        if (call(arg_name, is_string) == typeid(ELiteralStr))
            a->source = array_of(typeid(string), str(arg_name), null);
        else {
            assert (is_alpha(arg_name), "expected identifier for import arg");
            assert (call(tokens, next_is, ":"), "expected : after import arg (argument assignment)");
            call(tokens, consume);
            if (call(arg_name, eq, "name")) {
                Token token_name = call(tokens, next);
                assert (! call(token_name, is_string), "expected token for import name");
                a->name = str(token_name);
            } else if (call(arg_name, eq, "links"))    a->links      = intern(a, import_list, tokens);
              else if (call(arg_name, eq, "includes")) a->includes   = intern(a, import_list, tokens);
              else if (call(arg_name, eq, "source"))   a->source     = intern(a, import_list, tokens);
              else if (call(arg_name, eq, "build"))    a->build_args = intern(a, import_list, tokens);
              else if (call(arg_name, eq, "shell")) {
                Token token_shell = call(tokens, next);
                assert (call(token_shell, is_string), "expected shell invocation for building");
                a->shell = str(token_shell);
            } else if (call(arg_name, eq, "defines")) {
                // none is a decent name for null.
                assert (false, "not implemented");
            } else
                assert (false, "unknown arg: %o", arg_name);

            if (call(tokens, next_is, ","))
                call(tokens, next);
            else {
                assert (call(tokens, next_is, "]"), "expected comma or ] after arg %o", arg_name);
                break;
            }
        }
    }
}

/// get import keyword working to build into build-root (silver-import)
none EImport_init(EImport a) {
    assert(isa(a->tokens) == typeid(Tokens), "tokens mismatch: class is %s", isa(a->tokens)->name);
    Tokens tokens = a->tokens;
    if (tokens) {
        assert(call(tokens, next_is, "import"), "expected import");
        call(tokens, consume);
        //Token n_token = call(tokens, next);
        bool is_inc = call(tokens, next_is, "<");
        if (is_inc) {
            Tokens_f* type = isa(tokens);
            call(tokens, consume);
            a->includes = new(array, alloc, 8);
            while (1) {
                Token inc = call(tokens, next);
                assert (is_alpha(inc), "expected alpha-identifier for header");
                call(a->includes, push, inc);
                bool is_inc = call(tokens, next_is, ">");
                if (is_inc) break;
            }
        }
        Token t_next = call(tokens, next);
        string module_name = cast(t_next, string);
        a->name = hold(module_name);
        assert(is_alpha(module_name), "expected module name identifier");

        /// <c, includes, here>
        if (is_inc) {
            a->includes   = new(array);
            call(a->includes, push, module_name);
            while (true) {
                Token t = call(tokens, next);
                assert(call(t, eq, ",") || call(t, eq, ">"), "expected > or , in <include> syntax, found %o", t);
                if (call(t, eq, ">")) break;
                string proceeding = call(tokens, next);
                call(a->includes, push, proceeding);
            }
        }
        if (call(tokens, next_is, "as")) {
            call(tokens, consume);
            a->isolate_namespace = call(tokens, next);
        }

        assert(is_alpha(module_name), format("expected variable identifier, found %o", module_name));
        
        if (call(tokens, next_is, "[")) {
            call(tokens, next);
            Token n = call(tokens, peek);
            AType s = call(n, is_string);
            if (s == typeid(ELiteralStr)) {
                a->source = new(array);
                while (true) {
                    Token    inner = call(tokens, next);
                    string s_inner = cast(inner, string);
                    assert(call(inner, is_string) == typeid(ELiteralStr), "expected a string literal");
                    string  source = call(s_inner, mid, 1, len(s_inner) - 2);
                    call(a->source, push, source);
                    string       e = call(tokens, next);
                    if (call(e, eq, ","))
                        continue;
                    assert(call(e, eq, "]"), "expected closing bracket");
                    break;
                }
            } else {
                intern(a, import_fields, a->tokens);
                Token cur = call(a->tokens, peek);
                call(a->tokens, consume);
            }
        }
    }
}

BuildState EImport_build_project(EImport a, string name, string url) {
    path checkout = create_folder(a->module, "checkouts", name->chars);
    path i        = create_folder(a->module, "install", null);
    path b        = form(path, "%o/%s", checkout, "silver-build");

    /// clone if empty
    if (call(checkout, is_empty)) {
        char   at[2]  = { '@', 0 };
        string f      = form(string, "%s", at);
        num    find   = call(url, index_of, at);
        string branch = null;
        string s_url  = url;
        if (find > -1) {
            s_url     = call(url, mid, 0, find);
            branch    = call(url, mid, find + 1, len(url) - (find + 1));
        }
        string cmd = format("git clone %o %o", s_url, checkout);
        assert (system(cmd->chars) == 0, "git clone failure");
        if (len(branch)) {
            chdir(checkout->chars);
            cmd = form(string, "git checkout %o", branch);
            assert (system(cmd->chars) == 0, "git checkout failure");
        }
        call(b, make_dir);
    }
    /// intialize and build
    if (!call(checkout, is_empty)) { /// above op can add to checkout; its not an else
        chdir(checkout->chars);

        bool build_success = file_exists("%o/silver-token", b);
        if (file_exists("silver-init.sh") && !build_success) {
            string cmd = format(
                "%s/silver-init.sh \"%s\"", path_type.cwd(2048), i);
            assert(system(cmd->chars) == 0, "cmd failed");
        }
    
        bool is_rust = file_exists("Cargo.toml");
        ///
        if (is_rust) {
            cstr rel_or_debug = "release";
            path package = form(path, "%o/%s/%o", i, "rust", name);
            call(package, make_dir);
            ///
            setenv("RUSTFLAGS", "-C save-temps", 1);
            setenv("CARGO_TARGET_DIR", package->chars, 1);
            string cmd = format("cargo build -p %o --%s", name, rel_or_debug);

            assert (system(cmd->chars) == 0, "cmd failed");
            path   lib = form(path,
                "%o/%s/lib%o.so", package, rel_or_debug, name);
            path   exe = form(path,
                "%o/%s/%o_bin",   package, rel_or_debug, name);
            if (!file_exists(exe->chars))
                exe = form(path, "%o/%s/%o", package, rel_or_debug, name);
            if (file_exists(lib->chars)) {
                path sym = form(path, "%o/lib%o.so", i, name);
                a->links = array_of(typeid(string), name, null);
                create_symlink(lib, sym);
            }
            if (file_exists(exe->chars)) {
                path sym = form(path, "%o/%o", i, name);
                create_symlink(exe, sym);
            }
        }   
        else {
            assert (file_exists("CMakeLists.txt"),
                "CMake required for project builds");

            string cmake_flags = str("");
            each(a->build_args, string, arg) {
                if (cast(cmake_flags, bool))
                    call(cmake_flags, append, " ");
                call(cmake_flags, append, arg->chars);
            }
            if (!build_success) {
                string cmake = str(
                    "cmake -S . -DCMAKE_BUILD_TYPE=Release "
                    "-DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON");
                string cmd   = format(
                    "%o -B %o -DCMAKE_INSTALL_PREFIX=%o %o", cmake, b, i, cmake_flags);
                assert (system(cmd->chars) == 0, "cmd failed");
                chdir(b->chars);
                assert (system("make -j16 install") == 0, "install failed");
            }
            
            if (!a->links) a->links = array_of(typeid(string), name, null);
            each(a->links, string, name) {
                string pre = lib_prefix();
                string ext = shared_ext();
                path   lib = form(path, "%o/lib/%o%o.%o", i, pre, name, ext);
                if (!file_exists(lib)) {
                    pre = lib_prefix();
                    ext = static_ext();
                    lib = form(path, "%o/lib/%o%o.%o", i, pre, name, ext);
                }
                assert (file_exists(lib), "lib does not exist");
                path sym = form(path, "%o/%o%o.%o", i, pre, name, ext);
                create_symlink(lib, sym);
            }

            FILE*  silver_token = fopen("silver-token", "w");
            fclose(silver_token);
        }
    }
    return BuildState_built;
}

bool contains_main(path obj_file) {
    string cmd = format("nm %o", obj_file);
    FILE *fp = popen(cmd->chars, "r");
    assert(fp, "failure to open %o", obj_file);
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, " T main") != NULL) {
            pclose(fp);
            return true;
        }
    }
    pclose(fp);
    return false;
}

BuildState EImport_build_source(EImport a) {
    bool is_debug = a->module->debug;
    string build_root = a->module->source_path;
    each (a->cfiles, string, cfile) {
        path cwd = invoke(path, cwd, 1024);
        string compile;
        if (call(cfile, has_suffix, ".rs")) {
            // rustc integrated for static libs only in this use-case
            compile = format("rustc --crate-type=staticlib -C opt-level=%s %o/%o --out-dir %o",
                is_debug ? "0" : "3", cwd, cfile, build_root);
        } else {
            cstr opt = is_debug ? "-g2" : "-O2";
            compile = format(
                "gcc -I%o/include %s -Wfatal-errors -Wno-write-strings -Wno-incompatible-pointer-types -fPIC -std=c99 -c %o/%o -o %o/%o.o",
                build_root, opt, cwd, cfile, build_root, cfile);
        }
        
        path   obj_path   = form(path,   "%o.o", cfile);
        string log_header = form(string, "import: %o source: %o", a->name, cfile);
        print("%s > %s", cwd, compile);
        assert (system(compile) == 0,  "%o: compilation failed",    log_header);
        assert (file_exists(obj_path), "%o: object file not found", log_header);

        if (contains_main(obj_path)) {
            a->main_symbol = format("%o_main", call(obj_path, stem));
            string cmd = format("objcopy --redefine-sym main=%o %o",
                a->main_symbol, obj_path);
            assert (system(cmd->chars) == 0,
                "%o: could not replace main symbol", log_header);
        }
    }
    return BuildState_built;
}

void EImport_process_includes(EImport a, array includes) {
    /// silver = [ expressions ] and { statements } ?
    /// having a singlar expression instead of a statement would be nice for 1 line things in silver
    /// [cast] is then possible, i believe (if we dont want cast keyword)
    /// '{using} in strings, too, so we were using the character'
    /// 
    each(includes, string, e) {
        print("e = %o");
    }
}

void EImport_process(EImport a) {
    if (len(a->name) && !len(a->source) && len(a->includes)) {
        array attempt = array_of(typeid(string), str(""), str("spec/"), NULL);
        bool  exists  = false;
        each(attempt, string, pre) {
            path module_path = form(path, "%o%o.si", pre, a->name);
            if (!call(module_path, exists)) continue;
            a->module_path = module_path;
            print("module-path %o", module_path);
            exists = true;
            break;
        }
        assert(exists, "path does not exist for silver module: %o", a->name);
    } else if (len(a->name) && len(a->source)) {
        bool has_c  = false, has_h = false, has_rs = false,
             has_so = false, has_a = false;
        each(a->source, string, i0) {
            if (call(i0, has_suffix, str(".c")))   has_c  = true;
            if (call(i0, has_suffix, str(".h")))   has_h  = true;
            if (call(i0, has_suffix, str(".rs")))  has_rs = true;
            if (call(i0, has_suffix, str(".so")))  has_so = true;
            if (call(i0, has_suffix, str(".a")))   has_a  = true;
        }
        if (has_h)
            a->import_type = ImportType_source;
        else if (has_c || has_rs) {
            a->import_type = ImportType_source;
            intern(a, build_source);
        } else if (has_so) {
            a->import_type = ImportType_library;
            if (!a->library_exports)
                 a->library_exports = array_of(typeid(string), str(""), NULL);
            each(a->source, string, lib) {
                string rem = call(lib, mid, 0, len(lib) - 3);
                call(a->library_exports, push, rem);
            }
        } else if (has_a) {
            a->import_type = ImportType_library;
            if (!a->library_exports)
                 a->library_exports = array_of(typeid(string), str(""), NULL);
            each(a->source, string, lib) {
                string rem = call(lib, mid, 0, len(lib) - 2);
                call(a->library_exports, push, rem);
            }
        } else {
            assert(len(a->source) == 1, "source size mismatch");
            a->import_type = ImportType_project;
            intern(a, build_project, a->name, idx(a->source, 0));
            if (!a->library_exports)
                 a->library_exports = array_of(typeid(string), a->name, NULL);
        }
    }
    intern(a, process_includes, a->includes);
}


define_enum(ImportType)
define_enum(BuildState)

define_class(EImport)
