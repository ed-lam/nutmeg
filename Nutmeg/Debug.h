#ifndef NUTMEG_DEBUG_H
#define NUTMEG_DEBUG_H

#define FMT_HEADER_ONLY 1

#include "fmt/format.h"
#include "scip/scip.h"

#ifndef NDEBUG
#define println(format, ...) do { \
    fmt::print(format "\n", ##__VA_ARGS__); \
    fflush(stdout); \
} while (false)
#else
#define println(format, ...) do { \
    fmt::print(format "\n", ##__VA_ARGS__); \
} while (false)
#endif

#ifdef PRINT_DEBUG
#define debugln(format, ...) println(format, ##__VA_ARGS__)
#define debug(format, ...) fmt::print(format, ##__VA_ARGS__);
#else
#define debugln(format, ...) {}
#define debug(format, ...) {}
#endif

#ifndef NDEBUG
#define err(format, ...) do { \
    fmt::print(stderr, "Error: " format "\n", ##__VA_ARGS__); \
    fmt::print(stderr, "Function: {}\n", __PRETTY_FUNCTION__); \
    fmt::print(stderr, "File: {}\n", __FILE__); \
    fmt::print(stderr, "Line: {}\n", __LINE__); \
    std::abort(); \
} while (false)
#else
#define err(format, ...) do { \
    fmt::print(stderr, "Error: " format "\n", ##__VA_ARGS__); \
    std::abort(); \
} while (false)
#endif

#define release_assert(condition, ...) do { \
    if (!(condition)) err(__VA_ARGS__); \
} while (false)

#ifndef NDEBUG
#define debug_assert(condition) release_assert(condition, #condition)
#else
#define debug_assert(condition) {}
#endif

#define scip_assert(statement, ...) do { \
    const auto error = statement; \
    release_assert(error == SCIP_OKAY, "SCIP error {}", error); \
} while (false)

#define not_yet_implemented() do { \
    err("Reached unimplemented section in function \"{}\"", __PRETTY_FUNCTION__); \
} while (false)

#define not_yet_checked() do { \
    err("Reached unchecked section in function \"{}\"", __PRETTY_FUNCTION__); \
} while (false)

#define unreachable() do { \
    err("Reached unreachable section in function \"{}\"", __PRETTY_FUNCTION__); \
} while (false)

#endif
