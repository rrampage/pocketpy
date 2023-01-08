#pragma once

#include "obj.h"

class Frame;

struct BaseRef {
    virtual PyObject* get(VM*, Frame*) const = 0;
    virtual void set(VM*, Frame*, PyObject*) const = 0;
    virtual void del(VM*, Frame*) const = 0;
    virtual ~BaseRef() = default;
};

enum NameScope {
    NAME_LOCAL = 0,
    NAME_GLOBAL = 1,
    NAME_ATTR = 2,
};

struct NameRef : BaseRef {
    const std::pair<_Str, NameScope>* pair;
    NameRef(const std::pair<_Str, NameScope>& pair) : pair(&pair) {}

    PyObject* get(VM* vm, Frame* frame) const;
    void set(VM* vm, Frame* frame, PyObject* val) const;
    void del(VM* vm, Frame* frame) const;
};

struct AttrRef : BaseRef {
    mutable PyAutoRef obj;
    const NameRef attr;
    AttrRef(PyObject* obj, const NameRef attr) : obj(obj->share()), attr(attr) {}

    PyObject* get(VM* vm, Frame* frame) const;
    void set(VM* vm, Frame* frame, PyObject* val) const;
    void del(VM* vm, Frame* frame) const;
};

struct IndexRef : BaseRef {
    mutable PyAutoRef obj;
    PyAutoRef index;
    IndexRef(PyObject* obj, PyObject* index) : obj(obj->share()), index(index->share()) {}

    PyObject* get(VM* vm, Frame* frame) const;
    void set(VM* vm, Frame* frame, PyObject* val) const;
    void del(VM* vm, Frame* frame) const;
};

struct TupleRef : BaseRef {
    PyVarList varRefs;
    TupleRef(const PyVarList& varRefs) : varRefs(varRefs) {}
    TupleRef(PyVarList&& varRefs) : varRefs(std::move(varRefs)) {}

    PyObject* get(VM* vm, Frame* frame) const;
    void set(VM* vm, Frame* frame, PyObject* val) const;
    void del(VM* vm, Frame* frame) const;
};