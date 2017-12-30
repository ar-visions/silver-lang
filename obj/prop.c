#include <obj/obj.h>

implement(Prop)

typedef struct _TypeAssoc {
    const char *type;
    const char *primitive;
} TypeAssoc;

static TypeAssoc assoc[] = {
    { "Boolean",    "bool" },
    { "Int8",       "char" },
    { "UInt8",      "unsigned char" },
    { "Int16",      "short" },
    { "UInt16",     "unsigned short" },
    { "Int32",      "int" },
    { "UInt32",     "unsigned int" },
    { "Long",       "long" },
    { "ULong",      "unsigned long" },
    { "Float",      "float" },
    { "Double",     "double" }
};

const char *var_to_obj_type(char *vtype) {
    for (int i = 0; i < sizeof(assoc) / sizeof(TypeAssoc); i++) {
        TypeAssoc *ta = &assoc[i];
        if (strcmp(ta->primitive, vtype) == 0)
            return ta->type;
    }
    return NULL;
}

Prop Prop_new_with(char *type, char *name, Getter getter, Setter setter, char *meta) {
    const char *type_ = var_to_obj_type(type);
    if (!type_)
        type_ = type;
    Class c = class_find(type_);
    Type t = enum_find(Type, type_);
    Type t_object = enum_find(Type, "Object");
    if (!c && !t)
        return NULL;
    Prop self = new(Prop);
    self->name = new_string(name);
    self->enum_type = t ? (Enum)t : (c ? (Enum)t_object : NULL);
    self->class_type = c;
    self->getter = getter;
    self->setter = setter;
    self->meta = meta ? class_call(Pairs, from_cstring, meta) : NULL;
    return self;
}
