# Issue: LlamaBackend crashes with `std::length_error` when processing a shorter context after a longer one

**Description:**
When using `LlamaBackend::format_prompt()` with a conversation history that is shorter than previous iterations (e.g. after calling `Agent::clear_history()` or when RAG ephemeral context is removed), the backend crashes with a `std::length_error`. The crash stems from line 351 in `src/backend/llama_backend.cpp` attempting to instantiate a string starting from an old length `prev_len_` to a new smaller length `new_len`.

**Steps to Reproduce:**
1. Send a long conversation history to `format_prompt` (e.g., standard chat flow).
2. Clear the conversation history (or reduce the prompt length significantly).
3. Send a new, shorter conversation to `format_prompt`.
4. Observe the `libc++abi: terminating due to uncaught exception of type std::length_error: basic_string` crash.

**Root Cause:**
`format_prompt` calculates an incremental string allocation to update the prompt cache: 
```cpp
    std::string prompt(formatted_.begin() + prev_len_, formatted_.begin() + new_len);
```
However, if `new_len < prev_len_`, this results in a negative allocation size, crashing the process. 

**Proposed Fix:**
The `LlamaBackend` needs to detect when the history has shrunk and automatically invalidate the KV cache and reset the string offset pointers. I have addressed this locally and verified it works.

```cpp
    // src/backend/llama_backend.cpp around line 349
    if (new_len < prev_len_) {
        clear_kv_cache();
    }
```

A patch file `llama_backend_length_error_fix.patch` is attached with this proposed fix.
