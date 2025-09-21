#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <algorithm>

// Basic integer types
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

// Floating point types
using float32 = float;
using float64 = double;

// String types
using String = std::string;
using WString = std::wstring;

// Smart pointers
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Containers
template<typename T, size_t N>
using Array = std::array<T, N>;

template<typename T>
using Vector = std::vector<T>;

template<typename Key, typename Value>
using HashMap = std::unordered_map<Key, Value>;

// Function types
template<typename T>
using Function = std::function<T>;

// Handle types for resources
using EntityID = uint32;
using ResourceID = uint32;

struct Handle {
    uint32 index = 0;
    uint32 generation = 0;

    bool IsValid() const { return index != 0; }
    bool operator==(const Handle& other) const {
        return index == other.index && generation == other.generation;
    }
};

// Common macros
#define DECLARE_NON_COPYABLE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete;

#define DECLARE_NON_MOVABLE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete;

// Debug macros
#ifdef _DEBUG
    #define DEBUG_BUILD 1
    #define ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                std::string msg = "Assertion failed: " + std::string(message) + \
                                  "\nFile: " + __FILE__ + \
                                  "\nLine: " + std::to_string(__LINE__); \
                MessageBoxA(nullptr, msg.c_str(), "Assertion Failed", MB_OK | MB_ICONERROR); \
                __debugbreak(); \
            } \
        } while(0)
#else
    #define DEBUG_BUILD 0
    #define ASSERT(condition, message) ((void)0)
#endif

// Utility macros
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BIT(x) (1u << (x))

