rm -rf web/lib/
mkdir -p web/lib/
em++ src/main.cpp -fno-rtti -fexceptions -O3 -sEXPORTED_FUNCTIONS=_pkpy_delete,_pkpy_new_repl,_pkpy_repl_input,_pkpy_new_tvm,_pkpy_tvm_exec_async,_pkpy_tvm_get_state,_pkpy_tvm_read_jsonrpc_request,_pkpy_tvm_reset_state,_pkpy_tvm_terminate,_pkpy_tvm_write_jsonrpc_response,_pkpy_new_vm,_pkpy_vm_add_module,_pkpy_vm_eval,_pkpy_vm_exec,_pkpy_vm_get_global,_pkpy_vm_read_output -sASYNCIFY -sEXPORTED_RUNTIME_METHODS=ccall -sASYNCIFY_IMPORTS=pkpy_tvm_exec_async -o web/lib/pocketpy.js