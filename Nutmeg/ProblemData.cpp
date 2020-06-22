#include "ProblemData.h"
#include "Model.h"

namespace Nutmeg
{

ProblemData::ProblemData(Model& model, geas::solver& cp, Solution& sol) noexcept :
    model_(model),

    cp_cons_(nullptr),
    cp_(cp),

    mip_bool_vars_(),
    mip_neg_vars_idx_(),
    cp_bool_vars_(),
    bool_vars_name_(),
    bool_vars_monitor_(*geas::bounds_monitor<geas::atom_var, int>::create(cp.data)),

    mip_int_vars_(),
    mip_indicator_vars_idx_(),
    cp_int_vars_(),
    int_vars_lb_(),
    int_vars_ub_(),
    int_vars_name_(),
    int_vars_monitor_(*geas::bounds_monitor<geas::intvar, int>::create(cp.data)),
    constants_(),

    obj_var_idx_(-1),
    cp_dual_bound_(std::numeric_limits<Int>::min()),

    nb_linear_constraints_(0),
    nb_indicator_constraints_(0),
    nb_indicator_vars_setpart_constraints_(0),
    nb_indicator_vars_linking_constraints_(0),

    sol_(sol)
{
}

Int ProblemData::nb_bool_vars() const
{
    debug_assert(mip_bool_vars_.size() == cp_bool_vars_.size());
    return mip_bool_vars_.size();
}

Int ProblemData::nb_int_vars() const
{
    debug_assert(mip_int_vars_.size() == cp_int_vars_.size());
    return mip_int_vars_.size();
}

//bool ProblemData::has_mip_var(const IntVar var) const
//{
//    release_assert(var.model == this);
//    release_assert(var.is_valid(), "Invalid variable");
//#ifndef NDEBUG
//    return mip_int_vars_.at(var.idx) != nullptr;
//#else
//    return mip_int_vars_[var.idx] != nullptr;
//#endif
//}

SCIP_VAR* ProblemData::mip_var(const BoolVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(mip_bool_vars_.size()), "Variable is invalid");
    return mip_bool_vars_[var.idx];
}

SCIP_VAR* ProblemData::mip_var(const IntVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(mip_int_vars_.size()), "Variable is invalid");
    return mip_int_vars_[var.idx];
}

bool ProblemData::is_pos_var(const BoolVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    return is_pos_var(var.idx);
}

bool ProblemData::is_pos_var(const Int var_idx) const
{
    release_assert(0 <= var_idx && var_idx < static_cast<Int>(mip_bool_vars_.size()), "Variable is invalid");
    const auto neg_idx = mip_neg_vars_idx_[var_idx];
    return neg_idx == -1 || neg_idx > var_idx;
}

bool ProblemData::has_mip_indicator_vars(const IntVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(mip_int_vars_.size()), "Variable is invalid");
    return !mip_indicator_vars_idx_[var.idx].empty();
}

SCIP_VAR* ProblemData::mip_indicator_var(const IntVar var, const Int val) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(mip_int_vars_.size()), "Variable is invalid");
    if (lb(var) <= val && val <= ub(var))
    {
        const auto var_idx = var.idx;
        const auto val_idx = val - lb(var);
        return mip_bool_vars_[mip_indicator_vars_idx_[var_idx][val_idx]];
    }
    else
    {
        return nullptr;
    }
}

geas::patom_t ProblemData::cp_var(const BoolVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(cp_bool_vars_.size()), "Variable is invalid");
    return cp_bool_vars_[var.idx];
}

geas::intvar ProblemData::cp_var(const IntVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(cp_int_vars_.size()), "Variable is invalid");
    return cp_int_vars_[var.idx];
}

Int ProblemData::lb(const IntVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(int_vars_lb_.size()), "Variable is invalid");
    return int_vars_lb_[var.idx];
}

Int ProblemData::ub(const IntVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(int_vars_ub_.size()), "Variable is invalid");
    return int_vars_ub_[var.idx];
}

const String& ProblemData::name(const BoolVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(bool_vars_name_.size()), "Variable is invalid");
    return bool_vars_name_[var.idx];
}

const String& ProblemData::name(const IntVar var) const
{
    release_assert(var.model == &model_, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < static_cast<Int>(int_vars_name_.size()), "Variable is invalid");
    return int_vars_name_[var.idx];
}

}
