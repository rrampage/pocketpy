#pragma once

#include "obj.h"
#include "pointer.h"
#include "error.h"

enum Opcode {
    #define OPCODE(name) OP_##name,
    #include "opcodes.h"
    #undef OPCODE
};

static const char* OP_NAMES[] = {
    #define OPCODE(name) #name,
    #include "opcodes.h"
    #undef OPCODE
};

struct Bytecode{
    uint8_t op;
    int arg;
    int line;           // the line number
    uint16_t block;     // the block id
};

_Str pad(const _Str& s, const int n){
    return s + std::string(n - s.size(), ' ');
}

enum CodeBlockType {
    NO_BLOCK,
    FOR_LOOP,
    WHILE_LOOP,
    CONTEXT_MANAGER,
    TRY_EXCEPT,
};

struct CodeBlock {
    CodeBlockType type;
    std::vector<int> id;
    int parent;        // parent index in co_blocks

    int start;          // start index of this block in co_code, inclusive
    int end;            // end index of this block in co_code, exclusive

    std::string toString() const {
        if(parent == -1) return "";
        std::string s = "[";
        for(int i = 0; i < id.size(); i++){
            s += std::to_string(id[i]);
            if(i != id.size()-1) s += "-";
        }
        s += ", type=";
        s += std::to_string(type);
        s += "]";
        return s;
    }

    bool operator==(const std::vector<int>& other) const {
        return id == other;
    }

    bool operator!=(const std::vector<int>& other) const {
        return id != other;
    }

    int depth() const {
        return id.size();
    }
};

struct CodeObject {
    _Source src;
    _Str name;

    CodeObject(_Source src, _Str name) {
        this->src = src;
        this->name = name;
    }

    CompileMode mode() const {
        return src->mode;
    }

    std::vector<Bytecode> co_code;
    PyVarList co_consts;
    std::vector<std::pair<_Str, NameScope>> co_names;
    std::vector<_Str> co_global_names;

    std::vector<CodeBlock> co_blocks = { CodeBlock{NO_BLOCK, {}, -1} };

    // tmp variables
    int _currBlockIndex = 0;
    bool __isCurrBlockLoop() const {
        return co_blocks[_currBlockIndex].type == FOR_LOOP || co_blocks[_currBlockIndex].type == WHILE_LOOP;
    }

    void __enterBlock(CodeBlockType type){
        const CodeBlock& currBlock = co_blocks[_currBlockIndex];
        std::vector<int> copy(currBlock.id);
        copy.push_back(-1);
        int t = 0;
        while(true){
            copy[copy.size()-1] = t;
            auto it = std::find(co_blocks.begin(), co_blocks.end(), copy);
            if(it == co_blocks.end()) break;
            t++;
        }
        co_blocks.push_back(CodeBlock{type, copy, _currBlockIndex, (int)co_code.size()});
        _currBlockIndex = co_blocks.size()-1;
    }

    void __exitBlock(){
        co_blocks[_currBlockIndex].end = co_code.size();
        _currBlockIndex = co_blocks[_currBlockIndex].parent;
        if(_currBlockIndex < 0) UNREACHABLE();
    }

    // for goto use
    // goto/label should be put at toplevel statements
    emhash8::HashMap<_Str, int> co_labels;

    void addLabel(const _Str& label){
        if(co_labels.find(label) != co_labels.end()){
            _Str msg = "label '" + label + "' already exists";
            throw std::runtime_error(msg.c_str());
        }
        co_labels[label] = co_code.size();
    }

    int addName(_Str name, NameScope scope){
        if(scope == NAME_LOCAL && std::find(co_global_names.begin(), co_global_names.end(), name) != co_global_names.end()){
            scope = NAME_GLOBAL;
        }
        auto p = std::make_pair(name, scope);
        for(int i=0; i<co_names.size(); i++){
            if(co_names[i] == p) return i;
        }
        co_names.push_back(p);
        return co_names.size() - 1;
    }

    int addConst(PyVar v){
        co_consts.push_back(v);
        return co_consts.size() - 1;
    }
};

class Frame {
private:
    std::vector<PyObject*> s_data;
    int ip = -1;         // the current instruction pointer
    int next_ip = 0;     // the next instruction pointer
public:
    const _Code code;
    PyVar _module;
    PyVarDict f_locals;

    Frame(const _Code code, PyVar _module, PyVarDict&& locals)
        : code(code), _module(_module), f_locals(std::move(locals)) {
    }

    inline PyVarDict copy_f_locals() const noexcept { return f_locals; }
    inline PyVarDict& f_globals() noexcept { return _module->attribs; }

    inline const Bytecode& next_bytecode() noexcept {
        ip = next_ip;
        next_ip = ip + 1;
        return code->co_code[ip];
    }

    inline bool is_bytecode_ended() const noexcept {
        return ip >= code->co_code.size();
    }

    template<typename T>
    inline void push(T&& obj) noexcept{
        s_data.push_back(std::forward<T>(obj));
    }

    inline PyObject* pop(){
        if(s_data.empty()) throw std::runtime_error("s_data.empty() is true");
        PyObject* obj = s_data.back();
        s_data.pop_back();
        return obj;
    }

    PyVarList pop_n_reversed_unlimited(VM* vm, int n){
        PyVarList v(n);
        for(int i=n-1; i>=0; i--) v[i] = pop();
        return v;
    }

    pkpy::ArgList pop_n_reversed(int n){
        pkpy::ArgList v(n);
        for(int i=n-1; i>=0; i--) v._index(i) = pop();
        return v;
    }

    inline PyObject*& top() {
        if(s_data.empty()) throw std::runtime_error("s_data.empty() is true");
        return s_data.back();
    }

    inline PyObject* top_offset(VM* vm, int n=-1) const{
        size_t i = s_data.size() + n;
        if(i >= s_data.size()) throw std::runtime_error("top_offset out of range");
        return s_data[i];
    }

    inline void jump_abs(int i) noexcept{ next_ip = i; }
    inline void jump_rel(int i) noexcept{ next_ip = ip + i; }

    void jumpAbsoluteSafe(int target){
        const Bytecode& prev = code->co_code[this->ip];
        int i = prev.block;
        this->ip = target;
        if(isCodeEnd()){
            while(i>=0){
                if(code->co_blocks[i].type == FOR_LOOP) __pop();
                i = code->co_blocks[i].parent;
            }
        }else{
            const Bytecode& next = code->co_code[target];
            while(i>=0 && i!=next.block){
                if(code->co_blocks[i].type == FOR_LOOP) __pop();
                i = code->co_blocks[i].parent;
            }
            if(i!=next.block) throw std::runtime_error(
                "invalid jump from " + code->co_blocks[prev.block].toString() + " to " + code->co_blocks[next.block].toString()
            );
        }
    }

    bool jumpToNextExceptionHandler(){
        const Bytecode& curr = code->co_code[this->ip];
        int i = curr.block;
        while(i>=0){
            if(code->co_blocks[i].type == TRY_EXCEPT){
                // jump to its except block
                jumpAbsoluteSafe(code->co_blocks[i].end);
                return true;
            }
            i = code->co_blocks[i].parent;
        }
        return false;
    }


    _Str errorSnapshot(){
        int line = code->co_code[ip].line;
        return code->src->snapshot(line);
    }

    inline int stackSize() const noexcept {
        return s_data.size();
    }

    inline auto __debugStackData() const noexcept{
        return s_data;
    }
};