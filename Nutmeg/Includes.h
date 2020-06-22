#ifndef NUTMEG_INCLUDES_H
#define NUTMEG_INCLUDES_H

#include "Debug.h"

#include <vector>
#include <unordered_map>
#include <string>
#include <utility>
#include <limits>

#include "scip/scip.h"

namespace Nutmeg
{

using Int = int32_t;
using Float = SCIP_Real;

template<class T>
using Vector = std::vector<T>;

template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using HashTable = std::unordered_map<Key, T, Hash, KeyEqual>;

using String = std::string;

template<class T1, class T2>
using Pair = std::pair<T1, T2>;

//template<class T, class Deleter = std::default_delete<T>>
//using UniquePtr = std::unique_ptr<T, Deleter>;

constexpr auto Infinity = std::numeric_limits<Float>::infinity();
static_assert(std::numeric_limits<Float>::has_infinity);

}

#endif
