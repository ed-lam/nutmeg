#ifndef VRPTW_INCLUDES_H
#define VRPTW_INCLUDES_H

#include "Nutmeg/Debug.h"

#include <vector>
#include <string>

// -------------------------------------------------------------------------------------

template<class T>
using Vector = std::vector<T>;

using String = std::string;

// -------------------------------------------------------------------------------------

using Float               = double;
using Int                 = int_fast32_t;
using UInt                = uint_fast32_t;

using Request             = Int;
using Cost                = Int;
using Position            = Int;
using Time                = Int;
using Load                = Int;

using GRBvar = int;
using GRBconstr = int;

// -------------------------------------------------------------------------------------

#endif