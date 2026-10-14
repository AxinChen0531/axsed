#include <mimalloc.h>
#include <process.h>
#include <axsed/core/base.h>

namespace axsed
{
    static ax_malloc_func_t p_axe_malloc = mi_malloc;
    static ax_aligned_alloc_func_t p_axe_aligned_alloc = mi_aligned_alloc;
    static ax_calloc_func_t p_axe_calloc = mi_calloc;
    static ax_realloc_func_t p_axe_realloc = mi_realloc;
    static ax_free_func_t p_axe_free = mi_free;

    void ax_set_memory_function(
        const ax_malloc_func_t m,
        const ax_aligned_alloc_func_t a,
        const ax_calloc_func_t c,
        const ax_realloc_func_t r,
        const ax_free_func_t f
    ) noexcept {
        p_axe_malloc = m;
        p_axe_aligned_alloc = a;
        p_axe_calloc = c;
        p_axe_realloc = r;
        p_axe_free = f;
    }

    void* ax_malloc(const size_t size) {
        return p_axe_malloc(size);
    }

    void* ax_calloc(const size_t elementNum, const size_t elementSize) {
        return p_axe_calloc(elementNum, elementSize);
    }

    void* ax_realloc(void* memory, const size_t size) {
        return p_axe_realloc(memory, size);
    }

    void ax_free(void* memory) {
        return p_axe_free(memory);
    }

    void* ax_aligned_alloc(const size_t alignment, const size_t size) {
        return p_axe_aligned_alloc(alignment, size);
    }

    int ax_get_pid() {
#ifdef _MSC_VER
        return _getpid();
#else
        return getpid();
#endif
    }

    uint64_t ax_get_tid() {
        const std::thread::id tid = std::this_thread::get_id();
        if constexpr (sizeof(std::thread::id) >= 8) {
            return *reinterpret_cast<const uint64_t*>(&tid);
        } else {
            return *reinterpret_cast<const uint32_t*>(&tid);
        }
    }
}
