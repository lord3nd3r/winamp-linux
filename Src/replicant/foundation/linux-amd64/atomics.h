/*
 Linux x86_64 (amd64) implementation using GCC builtins
*/

#pragma once
#include "../../foundation/types.h"

#ifdef __cplusplus
#define NX_ATOMIC_INLINE inline
#else
#define NX_ATOMIC_INLINE
#endif

NX_ATOMIC_INLINE static size_t nx_atomic_inc(volatile size_t *addr)
{
    return __sync_add_and_fetch(addr, 1);
}

NX_ATOMIC_INLINE static size_t nx_atomic_dec(volatile size_t *addr)
{
    return __sync_sub_and_fetch(addr, 1);
}

NX_ATOMIC_INLINE static size_t nx_atomic_dec_release(volatile size_t *addr)
{
    return __sync_sub_and_fetch(addr, 1);
}

NX_ATOMIC_INLINE static void nx_atomic_write(size_t value, volatile size_t *addr)
{
    __sync_lock_test_and_set(addr, value);
    __sync_synchronize();
}

NX_ATOMIC_INLINE static void nx_atomic_write_pointer(void *value, void* volatile *addr)
{
    __sync_lock_test_and_set(addr, value);
    __sync_synchronize();
}

NX_ATOMIC_INLINE static size_t nx_atomic_add(size_t value, volatile size_t* addr)
{
    return __sync_fetch_and_add(addr, value);
}

NX_ATOMIC_INLINE static size_t nx_atomic_sub(size_t value, volatile size_t* addr)
{
    return __sync_fetch_and_sub(addr, value);
}

NX_ATOMIC_INLINE static size_t nx_atomic_read(volatile size_t *addr)
{
    __sync_synchronize();
    return *addr;
}

NX_ATOMIC_INLINE static size_t nx_atomic_cmpxchg(size_t oldval, size_t newval, volatile size_t *addr)
{
    return __sync_val_compare_and_swap(addr, oldval, newval);
}

NX_ATOMIC_INLINE static void *nx_atomic_cmpxchg_pointer(void *oldval, void *newval, void* volatile *addr)
{
    return __sync_val_compare_and_swap(addr, oldval, newval);
}

NX_ATOMIC_INLINE static size_t nx_atomic_swap(size_t newval, volatile size_t *addr)
{
    return __sync_lock_test_and_set(addr, newval);
}

NX_ATOMIC_INLINE static void *nx_atomic_swap_pointer(void *newval, void* volatile *addr)
{
    return __sync_lock_test_and_set(addr, newval);
}

// Memory barriers
NX_ATOMIC_INLINE static void nx_atomic_sync(void)
{
    __sync_synchronize();
}
