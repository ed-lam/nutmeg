#ifndef NUTMEG_VARIABLE_H
#define NUTMEG_VARIABLE_H

#include "Includes.h"

namespace Nutmeg
{

class Model;

class BoolVar
{
    friend class Model;
    friend struct ProblemData;

    Model* model{nullptr};
    Int idx{-1};

  private:
    // Constructors
    BoolVar(Model* model, const Int idx) : model(model), idx(idx) { debug_assert(is_valid()); }

  public:
    // Constructors
    BoolVar() noexcept = default;
    BoolVar(const BoolVar& rhs) = default;
    BoolVar(BoolVar&& rhs) = default;
    BoolVar& operator=(const BoolVar& rhs) = default;
    BoolVar& operator=(BoolVar&& rhs) = default;
    ~BoolVar() noexcept = default;

    // Check validity of the variable
    inline bool is_valid() const { return model && idx >= 0; }
    inline bool is_same(const BoolVar other) const { return model == other.model && idx == other.idx; }

    // Get negated literal
    BoolVar operator~();
};
static_assert(std::is_trivially_copyable<BoolVar>::value);

class IntVar
{
    friend class Model;
    friend struct ProblemData;

    Model* model{nullptr};
    Int idx{-1};

  private:
    // Constructors
    IntVar(Model* model, const Int idx) : model(model), idx(idx) { debug_assert(is_valid()); }

  public:
    // Constructors
    IntVar() noexcept = default;
    IntVar(const IntVar& rhs) = default;
    IntVar(IntVar&& rhs) = default;
    IntVar& operator=(const IntVar& rhs) = default;
    IntVar& operator=(IntVar&& rhs) = default;
    ~IntVar() noexcept = default;

    // Check validity of the variable
    inline bool is_valid() const { return model && idx >= 0; }
    inline bool is_same(const IntVar other) const { return model == other.model && idx == other.idx; }

    // Get literals
//    BoolVar operator==(const Int val);
//    BoolVar operator<=(const Int val);
//    BoolVar operator>=(const Int val);
//    BoolVar operator<(const Int val);
//    BoolVar operator>(const Int val);
};
static_assert(std::is_trivially_copyable<IntVar>::value);

}

#endif
