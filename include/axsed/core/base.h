#ifndef AXSED_BASE_H
#define AXSED_BASE_H

#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <thread>

#ifndef AXSEDCORE_CACHE_LINE_SIZE
#define AXSEDCORE_CACHE_LINE_SIZE 64
#endif
#ifndef AXSEDCORE_RINGBUFFER_SPIN_COUNT
#define AXSEDCORE_RINGBUFFER_SPIN_COUNT 16
#endif

#ifdef AXSEDCORE_NO_EXPORT
#   define AXSEDCORE_API
#else
#   ifndef _WIN32
#       define AXSEDCORE_API __attribute__((visibility("default")))
#   else
#       ifdef AXSEDCORE_EXPORT
#           define AXSEDCORE_API __declspec(dllexport)
#       else
#           define AXSEDCORE_API __declspec(dllimport)
#       endif
#   endif
#endif

#if (defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)) && (defined(__SSE2__) || defined(_MSC_VER))
#include <emmintrin.h>
#endif
#if defined(_MSC_VER)
#include <intrin.h>   // __yield (ARM/ARM64), _mm_pause (x86)
#endif
#include <atomic>

namespace axsed
{
    /* for CAS spin using */
    inline void ax_cpu_pause_hint() noexcept {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)
        // x86 / x86-64: PAUSE — lowers power and relieves store-buffer / memory-
        // order-buffer pressure that builds up under repeated failed CAS.
        // Use the portable _mm_pause() intrinsic for every compiler; fall back to
        // inline asm only for a 32-bit x86 build made without SSE2. No "memory"
        // clobber — PAUSE is a hint, not a barrier; the loop re-reads an atomic.
#  if defined(_MSC_VER) || defined(__SSE2__)
        _mm_pause();
#  else
        __asm__ __volatile__("pause");
#  endif
#elif defined(__aarch64__) || defined(__arm__) || defined(_M_ARM) || defined(_M_ARM64)
        // ARM / AArch64: YIELD hint (HINT #1).
#  if defined(_MSC_VER)
        __yield();
#  elif defined(__clang__) && defined(__has_builtin) && __has_builtin(__builtin_arm_yield)
        __builtin_arm_yield();
#  else
        // GCC does not expose __builtin_arm_yield (bug 105416); both AArch64 and
        // ARMv6K+ support the `yield` (HINT #1) instruction, so emit it directly.
        __asm__ __volatile__("yield");
#  endif
#elif defined(__riscv)
        // RISC-V: PAUSE (Zihintpause). It is encoded as a HINT, so on hardware
        // lacking the extension it is a harmless no-op — always safe to emit.
#  if defined(__has_builtin) && __has_builtin(__builtin_riscv_pause)
        __builtin_riscv_pause(); // GCC 14+ / Clang (Clang also needs -march + zihintpause)
#  else
        __asm__ __volatile__("pause");
#  endif
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
        // POWER has no portable yield builtin. "or 27,27,27" is the classic HMT
        // low/medium-priority hint (a benign no-op on non-SMT cores); this is the
        // same idiom the Linux kernel uses.
        __asm__ __volatile__("or 27,27,27");
#else
        // Unknown architecture: compiler barrier only — keeps the spin loop honest
        // without emitting any CPU instruction.
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    typedef void*(*ax_malloc_func_t)(size_t size);
    typedef void*(*ax_aligned_alloc_func_t)(size_t alignment, size_t size);
    typedef void*(*ax_calloc_func_t)(size_t elementNum, size_t elementSize);
    typedef void*(*ax_realloc_func_t)(void* ptr, size_t size);
    typedef void (*ax_free_func_t)(void* ptr);

    /**
     * set the axe framework's memory function;
     * if never called, the default memory function is from microsoft's mimalloc library;
     * user should promise those memory functions' correction & multi-thread safety
     * @param m malloc function
     * @param a aligned alloc function
     * @param c calloc function
     * @param r realloc function
     * @param f free function
     * @attention must call before any axe memory function called, user must promise the memory function valid
     * @note multi-thread unsafe
     */
    AXSEDCORE_API void ax_set_memory_function(
        ax_malloc_func_t m,
        ax_aligned_alloc_func_t a,
        ax_calloc_func_t c,
        ax_realloc_func_t r,
        ax_free_func_t f
    ) noexcept;

    /**
     * allocate memory on heap
     * @param size the required memory size
     * @return nullptr for failure, otherwise the allocated memory head
     * @note multi-thread safe
     */
    [[nodiscard]] AXSEDCORE_API void* ax_malloc(size_t size);

    /**
     * allocate memory on heap and initialize all data to zero
     * @param elementNum the required element count
     * @param elementSize the required element size
     * @return nullptr for failure, otherwise the allocated memory head
     * @note multi-thread safe
     */
    [[nodiscard]] AXSEDCORE_API void* ax_calloc(size_t elementNum, size_t elementSize);

    /**
     * allocate memory on heap, copy as many as possible content, then free the old allocated memory if needed
     * @param memory the old allocated memory head
     * @param size the new required memory size
     * @return nullptr for failure, otherwise the allocated memory head
     * @note multi-thread safe
     */
    [[nodiscard]] AXSEDCORE_API void* ax_realloc(void* memory, size_t size);
    /**
     * free the allocated memory if needed
     * @param memory the allocated memory head, nullptr for doing nothing
     * @note multi-thread safe
     */
    AXSEDCORE_API void ax_free(void* memory);

    /**
     * allocate memory on heap with aligned
     * @param alignment the required memory alignment, must be n power of 2, failed otherwise
     * @param size the required memory size
     * @return nullptr for failure, otherwise the allocated memory head
     * @note multi-thread safe
     */
    [[nodiscard]] AXSEDCORE_API void* ax_aligned_alloc(size_t alignment, size_t size);

    /**
     * typed version of aligned_alloc
     * @attention alloc only, without construction
     * @note multi-thread safe
     */
    template<typename T>
    [[nodiscard]] T* ax_aligned_alloc(const size_t count) {
        return static_cast<T*>(ax_aligned_alloc(alignof(T), count * sizeof(T)));
    }

    /**
     * new instance of T
     * @note multi-thread safe
     */
    template<typename T, typename... Args> requires std::is_constructible_v<T, Args...>
    [[nodiscard]] T* ax_new(Args&&... args) {
        if constexpr (alignof(T) > alignof(std::max_align_t)) {
            return std::construct_at(ax_aligned_alloc<T>(1), std::forward<Args>(args)...);
        } else {
            return std::construct_at(static_cast<T*>(ax_malloc(sizeof(T))), std::forward<Args>(args)...);
        }
    }

    /**
     * the delete function for instance created by ax_new
     * @note multi-thread safe
     */
    template<typename T> requires std::is_destructible_v<T>
    void ax_delete(T* ptr) {
        std::destroy_at(ptr);
        ax_free(ptr);
    }

    /**
     * get the process id of this runtime
     * @attention multi-thread safe
     */
    [[nodiscard]] AXSEDCORE_API int ax_get_pid();

    /**
     * get the caller's thread id
     * @attention multi-thread safe
     */
    [[nodiscard]] AXSEDCORE_API uint64_t ax_get_tid();

    /**
     * Allocator for STL container using;
     * Applying of custom memory function (default mimalloc)
     */
    template<class T>
    class AxAllocator
    {
    public:
        using value_type = T;

        template<class U>
        struct rebind
        {
            typedef AxAllocator<U> other;
        };

        AxAllocator() noexcept = default;

        template<class U>
        explicit AxAllocator(const AxAllocator<U>&) noexcept {}

        AxAllocator select_on_container_copy_construction() const { return *this; }

        T* allocate(const std::size_t n) {
            if constexpr (alignof(T) > alignof(std::max_align_t)) {
                return ax_aligned_alloc<T>(n);
            } else {
                return static_cast<T*>(ax_malloc(n * sizeof(T)));
            }
        }

        void deallocate(T* p, std::size_t) {
            ax_free(p);
        }

        bool operator==(const AxAllocator&) const noexcept { return true; }
        bool operator!=(const AxAllocator&) const noexcept { return false; }
    };

    template<typename K, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
    using UnorderedSet = std::unordered_set<K, Hash, Pred, AxAllocator<K>>;
    template<typename K, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
    using UnorderedMultiSet = std::unordered_multiset<K, Hash, Pred, AxAllocator<K>>;
    template<typename K, typename V, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
    using UnorderedMap = std::unordered_map<K, V, Hash, Pred, AxAllocator<std::pair<const K, V>>>;
    template<typename K, typename V, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
    using UnorderedMultiMap = std::unordered_multimap<K, V, Hash, Pred, AxAllocator<std::pair<const K, V>>>;
    template<typename K, typename Compare = std::less<K>>
    using Set = std::set<K, Compare, AxAllocator<K>>;
    template<typename K, typename Compare = std::less<K>>
    using MultiSet = std::multiset<K, Compare, AxAllocator<K>>;
    template<typename K, typename V, typename Compare = std::less<K>>
    using Map = std::map<K, V, Compare, AxAllocator<std::pair<const K, V>>>;
    template<typename K, typename V, typename Compare = std::less<K>>
    using MultiMap = std::multimap<K, V, Compare, AxAllocator<std::pair<const K, V>>>;
    template<typename T> requires (!std::is_same_v<T, bool>)
    using Vector = std::vector<T, AxAllocator<T>>;
    template<typename T>
    using List = std::list<T, AxAllocator<T>>;
    template<typename T>
    using ForwardList = std::forward_list<T, AxAllocator<T>>;
    template<typename T>
    using Deque = std::deque<T, AxAllocator<T>>;
    template<typename T, typename Sequence = Deque<T>>
    using Stack = std::stack<T, Sequence>;
    template<typename T, typename Sequence = Deque<T>>
    using Queue = std::queue<T, Sequence>;
    template<typename T, typename Compare = std::less<T>>
    using PriorityQueue = std::priority_queue<T, Vector<T>, Compare>;

    using String = std::basic_string<char, std::char_traits<char>, AxAllocator<char>>;
    using WString = std::basic_string<wchar_t, std::char_traits<wchar_t>, AxAllocator<wchar_t>>;
    using U8String = std::basic_string<char8_t, std::char_traits<char8_t>, AxAllocator<char8_t>>;
    using U16String = std::basic_string<char16_t, std::char_traits<char16_t>, AxAllocator<char16_t>>;
    using U32String = std::basic_string<char32_t, std::char_traits<char32_t>, AxAllocator<char32_t>>;

    struct AxDeleter
    {
        template<class T>
        void operator()(T* p) const noexcept {
            ax_delete(p);
        }
    };

    template<typename T>
    using Scope = std::unique_ptr<T, AxDeleter>;
    template<typename T>
    using Ref = std::shared_ptr<T>;
    template<typename T>
    using WeakRef = std::weak_ptr<T>;

    template<class T, class... Args>
    Scope<T> ax_make_scope(Args&&... args) {
        return Scope<T>(ax_new<T>(std::forward<Args>(args)...));
    }

    template<class T, class... Args>
    Ref<T> ax_make_ref(Args&&... args) {
        return std::allocate_shared<T>(AxAllocator<T>{}, std::forward<Args>(args)...);
    }
}

#endif //AXSED_BASE_H
