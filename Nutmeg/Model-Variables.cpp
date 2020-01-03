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

    // Check.
    debug_assert(probdata_.mip_int_vars_.size() == probdata_.mip_indicator_vars_idx_.size());
    debug_assert(probdata_.mip_indicator_vars_idx_.size() == probdata_.cp_int_vars_.size());
    debug_assert(probdata_.cp_int_vars_.size() == probdata_.int_vars_lb_.size());
    debug_assert(probdata_.int_vars_lb_.size() == probdata_.int_vars_ub_.size());
    debug_assert(probdata_.int_vars_ub_.size() == probdata_.int_vars_name_.size());

    // Return.
    return int_var;
}

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

        // Create indicator variables.
        indicator_vars_idx.resize(size);
        Vector<SCIP_VAR*> indicator_vars(size);
        Vector<Float> indicator_vals(size);
        for (const auto val : domain_copy)
        {
            // Create variable name.
            const auto idx = val - var_lb;
            auto ind_var_name = fmt::format("[{}={}]", name(var), val);

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
            scip_assert(SCIPcreateConsBasicSetpart(mip_,
                                                   &cons,
                                                   "",
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
            scip_assert(SCIPcreateConsBasicLinear(mip_,
                                                  &cons,
                                                  "",
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

bool Model::get_sol(const BoolVar var)
{
    // Check.
    release_assert(var.model == this, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < nb_bool_vars(), "Variable is invalid");

    // Get value.
    return sol_.bool_vars_sol_[var.idx];
}

Int Model::get_sol(const IntVar var)
{
    // Check.
    release_assert(var.model == this, "Variable belongs to a different model");
    release_assert(0 <= var.idx && var.idx < nb_int_vars(), "Variable is invalid");

    // Get value.
    return sol_.int_vars_sol_[var.idx];
}

}
