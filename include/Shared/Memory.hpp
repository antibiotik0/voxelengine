// =============================================================================
// VOXEL ENGINE - MEMORY UTILITIES
// Aligned allocation helpers and memory management utilities
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace voxel::memory {

// =============================================================================
// ALIGNMENT CONSTANTS
// =============================================================================
inline constexpr std::size_t CACHE_LINE_SIZE = 64;
inline constexpr std::size_t SIMD_ALIGNMENT = 32;  // AVX2
inline constexpr std::size_t AVX512_ALIGNMENT = 64;

// =============================================================================
// ALIGNED ALLOCATOR
// For use with STL containers requiring custom alignment
// =============================================================================
template<typename T, std::size_t Alignment = CACHE_LINE_SIZE>
class AlignedAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    static constexpr std::size_t alignment = Alignment;

    constexpr AlignedAllocator() noexcept = default;

    template<typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_array_new_length();
        }

        void* ptr = ::operator new(n * sizeof(T), std::align_val_t{Alignment});
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, [[maybe_unused]] std::size_t n) noexcept {
        ::operator delete(ptr, std::align_val_t{Alignment});
    }

    template<typename U>
    [[nodiscard]] bool operator==(const AlignedAllocator<U, Alignment>&) const noexcept {
        return true;
    }

    template<typename U>
    [[nodiscard]] bool operator!=(const AlignedAllocator<U, Alignment>&) const noexcept {
        return false;
    }
};

// =============================================================================
// ALIGNED UNIQUE POINTER HELPERS
// =============================================================================
template<typename T>
struct AlignedDeleter {
    static constexpr std::size_t Alignment = alignof(T) > CACHE_LINE_SIZE 
                                            ? alignof(T) : CACHE_LINE_SIZE;

    void operator()(T* ptr) const noexcept {
        if (ptr) {
            ptr->~T();
            ::operator delete(ptr, std::align_val_t{Alignment});
        }
    }
};

template<typename T>
struct AlignedArrayDeleter {
    static constexpr std::size_t Alignment = alignof(T) > CACHE_LINE_SIZE 
                                            ? alignof(T) : CACHE_LINE_SIZE;

    void operator()(T* ptr) const noexcept {
        if (ptr) {
            ::operator delete[](ptr, std::align_val_t{Alignment});
        }
    }
};

template<typename T>
using AlignedUniquePtr = std::unique_ptr<T, AlignedDeleter<T>>;

template<typename T>
using AlignedArrayUniquePtr = std::unique_ptr<T[], AlignedArrayDeleter<T>>;

// Factory functions
template<typename T, typename... Args>
[[nodiscard]] AlignedUniquePtr<T> make_aligned(Args&&... args) {
    constexpr std::size_t Alignment = alignof(T) > CACHE_LINE_SIZE 
                                     ? alignof(T) : CACHE_LINE_SIZE;
    void* mem = ::operator new(sizeof(T), std::align_val_t{Alignment});
    try {
        T* ptr = new(mem) T(std::forward<Args>(args)...);
        return AlignedUniquePtr<T>(ptr);
    } catch (...) {
        ::operator delete(mem, std::align_val_t{Alignment});
        throw;
    }
}

template<typename T>
[[nodiscard]] AlignedArrayUniquePtr<T> make_aligned_array(std::size_t count) {
    constexpr std::size_t Alignment = alignof(T) > CACHE_LINE_SIZE 
                                     ? alignof(T) : CACHE_LINE_SIZE;
    void* mem = ::operator new[](count * sizeof(T), std::align_val_t{Alignment});
    return AlignedArrayUniquePtr<T>(static_cast<T*>(mem));
}

// =============================================================================
// MEMORY UTILITIES
// =============================================================================

// Check if pointer is aligned
template<std::size_t Alignment>
[[nodiscard]] constexpr bool is_aligned(const void* ptr) noexcept {
    return (reinterpret_cast<std::uintptr_t>(ptr) % Alignment) == 0;
}

// Align size up to boundary
[[nodiscard]] constexpr std::size_t align_up(std::size_t size, std::size_t alignment) noexcept {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Align size down to boundary
[[nodiscard]] constexpr std::size_t align_down(std::size_t size, std::size_t alignment) noexcept {
    return size & ~(alignment - 1);
}

} // namespace voxel::memory
