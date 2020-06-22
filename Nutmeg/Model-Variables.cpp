//#define PRINT_DEBUG

#include "Model.h"
#include "scip/cons_setppc.h"
#include "scip/cons_linear.h"
#include <numeric>

namespace Nutmeg
{

BoolVar Model::add_bool_var(
    const String& name    // Variable name
)
{
    // Create variable object.
    BoolVar bool_var(this, nb_bool_vars());

    // Create variable in MIP.
    SCIP_VAR*& mip_var = probdata_.mip_bool_vars_.emplace_back();
    scip_assert(SCIPcreateVarBasic(mip_,
                                   &mip_var,
                                   name.c_str(),
                                   0.0,
                                   1.0,
                                   0.0,
                                   SCIP_VARTYPE_BINARY));
    release_assert(mip_var, "Failed to create Boolean variable in MIP");
    scip_assert(SCIPaddVar(mip_, mip_var));
    probdata_.mip_neg_vars_idx_.emplace_back(-1);

    // Create variable in CP.
    probdata_.cp_bool_vars_.push_back(cp_.new_boolvar());

    // Store variable name.
    probdata_.bool_vars_name_.emplace_back(name);

    // Check.
    debug_assert(probdata_.mip_bool_vars_.size() == probdata_.mip_neg_vars_idx_.size());
    debug_assert(probdata_.mip_neg_vars_idx_.size() == probdata_.cp_bool_vars_.size());
    debug_assert(probdata_.cp_bool_vars_.size() == probdata_.bool_vars_name_.size());

    // Return.
    return bool_var;
}

IntVar Model::add_int_var(
    const Int lb,                 // Lower bound of the values
    const Int ub,                 // Upper bound of the values
    const bool include_in_mip,    // Whether the variable is added to MIP
    const String& name            // Variable name
)
{
    // Check.
    release_assert(lb <= ub,
                   "Failed to create integer variable with bounds {} and {}", lb, ub);

    // Retrieve existing variable if the new variable is a constant and already added.
    if (lb == ub)
    {
        auto it = probdata_.constants_.find(lb);
        if (it != probdata_.constants_.end())
        {
            return it->second;
        }
    }

    // Create variable object.
    IntVar int_var(this, nb_int_vars());

    // Create variable in MIP.
    SCIP_VAR*& mip_var = probdata_.mip_int_vars_.emplace_back();
    if (include_in_mip || method_ == Method::MIP)
    {
        scip_assert(SCIPcreateVarBasic(mip_,
                                       &mip_var,
                                       name.c_str(),
                                       lb,
                                       ub,
                                       0.0,
                                       SCIP_VARTYPE_INTEGER));
        release_assert(mip_var, "Failed to create integer variable in MIP");
        scip_assert(SCIPaddVar(mip_, mip_var));
    }
    probdata_.mip_indicator_vars_idx_.emplace_back();

    // Create variable in CP.
    probdata_.cp_int_vars_.push_back(cp_.new_intvar(lb, ub));

    // Store variable bounds.
    probdata_.int_vars_lb_.push_back(lb);
    probdata_.int_vars_ub_.push_back(ub);

    // Store variable name.
    probdata_.int_vars_name_.emplace_back(name);

    // Add to set of constants.
    if (lb == ub)
    {
        probdata_.constants_.insert({lb, int_var});
    }

    // Check.
    debug_assert(probdata_.mip_int_vars_.size() == probdata_.mip_indicator_vars_idx_.size());
    debug_assert(probdata_.mip_indicator_vars_idx_.size() == probdata_.cp_int_vars_.size());
    debug_assert(probdata_.cp_int_vars_.size() == probdata_.int_vars_lb_.size());
    debug_assert(probdata_.int_vars_lb_.size() == probdata_.int_vars_ub_.size());
    debug_assert(probdata_.int_vars_ub_.size() == probdata_.int_vars_name_.size());

    // Return.
    return int_var;
}

IntVar Model::add_mip_var(IntVar var)
{
    // Check.
    release_assert(var.model == this, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < nb_int_vars(), "Variable is invalid");

    // Add MIP variable.
    SCIP_VAR*& mip_var = probdata_.mip_int_vars_[var.idx];
    if (!mip_var)
    {
        // Add linking constraint if indicator variables already exist.
        scip_assert(SCIPcreateVarBasic(mip_,
                                       &mip_var,
                                       name(var).c_str(),
                                       lb(var),
                                       ub(var),
                                       0.0,
                                       SCIP_VARTYPE_INTEGER));
        release_assert(mip_var, "Failed to create integer variable in MIP");
        scip_assert(SCIPaddVar(mip_, mip_var));

        // Add linking constraint.
        auto& indicator_vars_idx = probdata_.mip_indicator_vars_idx_[var.idx];
        if (!indicator_vars_idx.empty())
        {
            // Get the bounds.
            const auto var_lb = lb(var);
            const auto var_ub = ub(var);
            const auto size = var_ub - var_lb + 1;

            // Create constraint.
            SCIP_CONS* cons = nullptr;
            const auto constr_name = fmt::format("indicator_vars_linking_{}",
                                                 probdata_.nb_indicator_vars_linking_constraints_++);
            scip_assert(SCIPcreateConsBasicLinear(mip_,
                                                  &cons,
                                                  constr_name.c_str(),
                                                  0,
                                                  nullptr,
                                                  nullptr,
                                                  0,
                                                  0));
            debug_assert(cons);

            // Add coefficients.
            for (Int idx = 0; idx < size; ++idx)
            {
                const auto var_idx = indicator_vars_idx[idx];
                debug_assert(get_false().idx == 0);
                if (var_idx > 0)
                {
                    auto var = probdata_.mip_bool_vars_[var_idx];
                    const auto val = var_lb + idx;
                    scip_assert(SCIPaddCoefLinear(mip_, cons, var, val));
                }
            }
            scip_assert(SCIPaddCoefLinear(mip_, cons, mip_var, -1));

            // Add constraint.
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }
    }

    // Return.
    return var;
}

//BoolVar Model::add_mip_var(BoolVar var)
//{
//    // Check.
//    release_assert(var.model == this, "Variable belongs to a different model");
//    release_assert(0 <= var.idx && var.idx < nb_bool_vars(), "Variable is invalid");
//
//    // Add MIP variable.
//    SCIP_VAR*& mip_var = probdata_.mip_bool_vars_[var.idx];
//    if (!mip_var)
//    {
//        scip_assert(SCIPcreateVarBasic(mip_,
//                                       &mip_var,
//                                       name(var).c_str(),
//                                       0.0,
//                                       1.0,
//                                       0.0,
//                                       SCIP_VARTYPE_BINARY));
//        release_assert(mip_var, "Failed to create binary variable in MIP");
//        scip_assert(SCIPaddVar(mip_, mip_var));
//    }
//
//    // Return.
//    return var;
//}

Vector<BoolVar> Model::add_indicator_vars(
    const IntVar var,
    const Vector<Int>& domain
)
{
    // Check.
    release_assert(var.model == this, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < nb_int_vars(), "Variable is invalid");

    // Get the bounds.
    const auto var_lb = lb(var);
    const auto var_ub = ub(var);
    const auto size = var_ub - var_lb + 1;

    // Create indicator variables if not yet created.
    debug_assert(var.idx < static_cast<Int>(probdata_.mip_indicator_vars_idx_.size()));
    auto& indicator_vars_idx = probdata_.mip_indicator_vars_idx_[var.idx];
    if (indicator_vars_idx.empty())
    {
        // Create complete domain if the input set is empty.
        auto domain_copy = domain;
        if (domain_copy.empty())
        {
            domain_copy.resize(size);
            std::iota(domain_copy.begin(), domain_copy.end(), var_lb);
        }
        else
        {
            std::sort(domain_copy.begin(), domain_copy.end());
            for (size_t idx = 0; idx < domain_copy.size() - 1; ++idx)
            {
                release_assert(domain_copy[idx] != domain_copy[idx + 1],
                               "Cannot create MIP indicator variables because of duplicate indicator values");
            }
        }

        // Create indicator variables.
        debug_assert(get_false().idx == 0);
        indicator_vars_idx.resize(size, 0);
        Vector<SCIP_VAR*> indicator_vars(size);
        Vector<Float> indicator_vals(size);
        for (const auto val : domain_copy)
        {
            // Create variable name.
            const auto idx = val - var_lb;
            auto ind_var_name = fmt::format("{{{}={}}}", name(var), val);

            // Create variable in MIP.
            SCIP_VAR*& ind_var = indicator_vars[idx];
            scip_assert(SCIPcreateVarBasic(mip_,
                                           &ind_var,
                                           ind_var_name.c_str(),
                                           0.0,
                                           1.0,
                                           0.0,
                                           SCIP_VARTYPE_BINARY));
            release_assert(ind_var, "Failed to create Boolean variable in MIP");
            scip_assert(SCIPaddVar(mip_, ind_var));
            indicator_vars_idx[idx] = nb_bool_vars();
            indicator_vals[idx] = val;
            probdata_.mip_bool_vars_.push_back(ind_var);
            probdata_.mip_neg_vars_idx_.emplace_back(-1);

            // Get literal in CP.
            probdata_.cp_bool_vars_.push_back(cp_var(var) == val);

            // Store variable name.
            probdata_.bool_vars_name_.push_back(move(ind_var_name));

            // Check.
            debug_assert(probdata_.mip_bool_vars_.size() == probdata_.mip_neg_vars_idx_.size());
            debug_assert(probdata_.mip_neg_vars_idx_.size() == probdata_.cp_bool_vars_.size());
            debug_assert(probdata_.cp_bool_vars_.size() == probdata_.bool_vars_name_.size());
        }

        // Fix values not in the domain.
        for (Int idx = 0; idx < size; ++idx)
            if (!indicator_vars[idx])
            {
                const auto val = var_lb + idx;
                release_assert(cp_.post(cp_var(var) != val), "Internal error while creating indicator variables");
            }

        // Create set partition constraint.
        {
            // Create constraint.
            SCIP_CONS* cons = nullptr;
            const auto constr_name = fmt::format("indicator_vars_setpart_{}",
                                                 probdata_.nb_indicator_vars_setpart_constraints_++);
            scip_assert(SCIPcreateConsBasicSetpart(mip_,
                                                   &cons,
                                                   constr_name.c_str(),
                                                   0,
                                                   nullptr));
            debug_assert(cons);

            // Add coefficients.
            for (auto var : indicator_vars)
                if (var)
                {
                    scip_assert(SCIPaddCoefSetppc(mip_, cons, var));
                }

            // Add constraint.
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }

        // Create linking constraint.
        if (mip_var(var))
        {
            // Create constraint.
            SCIP_CONS* cons = nullptr;
            const auto constr_name = fmt::format("indicator_vars_linking_{}",
                                                 probdata_.nb_indicator_vars_linking_constraints_++);
            scip_assert(SCIPcreateConsBasicLinear(mip_,
                                                  &cons,
                                                  constr_name.c_str(),
                                                  0,
                                                  nullptr,
                                                  nullptr,
                                                  0,
                                                  0));
            debug_assert(cons);

            // Add coefficients.
            for (Int idx = 0; idx < size; ++idx)
                if (auto var = indicator_vars[idx]; var)
                {
                    const auto val = var_lb + idx;
                    scip_assert(SCIPaddCoefLinear(mip_, cons, var, val));
                }
            scip_assert(SCIPaddCoefLinear(mip_, cons, mip_var(var), -1));

            // Add constraint.
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }
    }

    // Create output.
    Vector<BoolVar> output(size);
    for (Int idx = 0; idx < size; ++idx)
    {
        const auto ind_var_idx = indicator_vars_idx[idx];
        output[idx] = BoolVar(this, ind_var_idx);
    }
    return output;
}

void Model::add_mip_int_var_as_bool_var_alias(
    BoolVar bool_var,
    IntVar int_var
)
{
    // Check.
    release_assert(int_var.model == this, "Integer variable belongs to a different model");
    release_assert(bool_var.model == this, "Boolean variable belongs to a different model");
    release_assert(0 <= int_var.idx && int_var.idx < nb_int_vars(), "Integer variable is invalid");
    release_assert(0 <= bool_var.idx && bool_var.idx < nb_bool_vars(), "Boolean variable is invalid");
    release_assert(!mip_var(int_var), "Integer variable is already created inside MIP solver");
    release_assert(mip_var(bool_var), "Boolean variable is not yet created inside MIP solver");
//    release_assert(SCIPvarGetStatus(mip_var(bool_var)) == SCIP_VARSTATUS_ORIGINAL,
//                   "Boolean variable is not a variable in the original problem (status {})",
//                   SCIPvarGetStatus(mip_var(bool_var)));

    // Set integer variable to existing Boolean variable in MIP solver.
    probdata_.mip_int_vars_[int_var.idx] = probdata_.mip_bool_vars_[bool_var.idx];
    scip_assert(SCIPcaptureVar(mip_, probdata_.mip_int_vars_[int_var.idx]));
}

BoolVar Model::get_neg(const BoolVar var)
{
    // Check.
    release_assert(var.model == this, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < nb_bool_vars(), "Variable is invalid");

    // Get negated variable if already exists.
    const auto var_idx = var.idx;
    if (const auto neg_idx = probdata_.mip_neg_vars_idx_[var_idx]; neg_idx >= 0)
    {
        return BoolVar(this, neg_idx);
    }

    // Create negated variable.
    const auto neg_idx = nb_bool_vars();
    BoolVar neg_var(this, neg_idx);
    probdata_.mip_neg_vars_idx_[var_idx] = neg_idx;
    probdata_.mip_neg_vars_idx_.emplace_back(var_idx);
    probdata_.mip_bool_vars_.emplace_back();
    scip_assert(SCIPgetNegatedVar(mip_,
                                  probdata_.mip_bool_vars_[var_idx],
                                  &probdata_.mip_bool_vars_.back()));
    probdata_.cp_bool_vars_.emplace_back(~probdata_.cp_bool_vars_[var_idx]);
    probdata_.bool_vars_name_.emplace_back("~" + probdata_.bool_vars_name_[var_idx]);

    // Check.
    debug_assert(probdata_.mip_bool_vars_.size() == probdata_.mip_neg_vars_idx_.size());
    debug_assert(probdata_.mip_neg_vars_idx_.size() == probdata_.cp_bool_vars_.size());
    debug_assert(probdata_.cp_bool_vars_.size() == probdata_.bool_vars_name_.size());

    // Done.
    return neg_var;
}

}
