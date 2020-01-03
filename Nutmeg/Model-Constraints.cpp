//#define PRINT_DEBUG

#include "Model.h"
#include "Variable.h"
#include "scip/cons_linear.h"
#include "scip/cons_setppc.h"
#include "scip/cons_indicator.h"

#define geas_add_constr(expr) if (!expr) { status_ = Status::Infeasible; return false; }

namespace Nutmeg
{

bool Model::add_constr_fix(const BoolVar var)
{
    // Fix variable in MIP.
    {
        const auto var_idx = var.idx;
        const auto neg_idx = probdata_.mip_neg_vars_idx_[var_idx];
        const auto var_is_pos = is_pos_var(var);
        auto mip_var = var_is_pos ? probdata_.mip_bool_vars_[var_idx] : probdata_.mip_bool_vars_[neg_idx];

        SCIP_Bool infeasible = FALSE;
        SCIP_Bool fixed = FALSE;
        scip_assert(SCIPfixVar(mip_, mip_var, var_is_pos, &infeasible, &fixed));
        release_assert(fixed, "Failed to fix variable {}", name(var));
        if (infeasible)
        {
            status_ = Status::Infeasible;
            return false;
        }
    }

    // Fix variable in CP.
    geas_add_constr(cp_.post(cp_var(var)));

    // Success.
    return true;
}

bool Model::add_constr_linear(
    const Vector<IntVar>& vars,
    const Vector<Int>& coeffs,
    Sign sign,
    const Int rhs
)
{
    // Check.
    release_assert(vars.size() == coeffs.size(),
                   "Vectors of variables and coefficients have different lengths in "
                   "creating linear constraint");
    release_assert(sign == Sign::EQ || sign == Sign::LE || sign == Sign::GE,
                   "Linear constraint currently only supports <=, == or >=");

    // Create constraint in MIP.
    {
        const auto lhs_bound = sign != Sign::LE ?
                               static_cast<Float>(rhs) :
                               -SCIPinfinity(mip_);
        const auto rhs_bound = sign != Sign::GE ?
                               static_cast<Float>(rhs) :
                               SCIPinfinity(mip_);
        SCIP_CONS* cons;
        scip_assert(SCIPcreateConsBasicLinear(mip_,
                                              &cons,
                                              "",
                                              0,
                                              nullptr,
                                              nullptr,
                                              lhs_bound,
                                              rhs_bound));
        for (size_t idx = 0; idx < vars.size(); ++idx)
        {
            auto var = vars[idx];
            release_assert(var.is_valid(),
                           "Variable is not valid in creating linear constraint");
            if (mip_var(var))
            {
                scip_assert(SCIPaddCoefLinear(mip_, cons, mip_var(var), coeffs[idx]));
            }
            else
            {
                scip_assert(SCIPreleaseCons(mip_, &cons));
                goto CREATE_CP_CONSTRAINT;
            }
        }
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }

    // Create constraint in CP.
    CREATE_CP_CONSTRAINT:
    {
        if (vars.size() == 2 && ((coeffs[0] == 1 && coeffs[1] == -1) || (coeffs[0] == -1 && coeffs[1] == 1)))
        {
            // x - y <= rhs or x - y == rhs or x - y >= rhs
            auto x = (coeffs[0] == 1 && coeffs[1] == -1) ? cp_var(vars[0]) : cp_var(vars[1]);
            auto y = (coeffs[0] == 1 && coeffs[1] == -1) ? cp_var(vars[1]) : cp_var(vars[0]);

            if (sign == Sign::GE)
            {
                // x - y >= rhs
                // y - x <= -rhs
                // y <= x - rhs
                geas_add_constr(geas::int_le(cp_.data, y, x, -rhs));
                goto EXIT;
            }
            else if (sign == Sign::LE)
            {
                // x - y <= rhs
                // x <= y + rhs
                geas_add_constr(geas::int_le(cp_.data, x, y, rhs));
                goto EXIT;
            }
            else if (sign == Sign::EQ && rhs == 0)
            {
                // x - y == 0
                // x == y
                geas_add_constr(geas::int_eq(cp_.data, x, y));
                goto EXIT;
            }
        }

        {
            vec<geas::intvar> cp_vars;
            vec<int> cp_coeffs;
            for (size_t i = 0; i < vars.size(); ++i)
            {
                cp_vars.push(cp_var(vars[i]));
                cp_coeffs.push(coeffs[i]);
            }
            if (sign != Sign::GE)
            {
                geas_add_constr(geas::linear_le(cp_.data, cp_coeffs, cp_vars, rhs));
            }
            if (sign != Sign::LE)
            {
                for (auto& coeff : cp_coeffs)
                {
                    coeff *= -1;
                }
                geas_add_constr(geas::linear_le(cp_.data, cp_coeffs, cp_vars, -rhs));
            }
        }
    }

    // Success.
    EXIT:
    return true;
}

bool Model::add_constr_linear_neq(
    const Vector<IntVar>& vars,
    const Vector<Int>& coeffs,
    const Int rhs
)
{
    // Check.
    release_assert(vars.size() == coeffs.size(),
                   "Vectors of variables and coefficients have different lengths in "
                   "creating linear constraint");

    // Create constraint in MIP.
    {
        // Ensure all variables appear in the MIP.
        for (auto var : vars)
            if (!mip_var(var))
            {
                goto CREATE_CP_CONSTRAINT;
            }

        // Create variable to indicate less than the rhs.
        SCIP_VAR* ind_var;
        scip_assert(SCIPcreateVarBasic(mip_,
                                       &ind_var,
                                       "",
                                       0.0,
                                       1.0,
                                       0.0,
                                       SCIP_VARTYPE_BINARY));
        release_assert(ind_var, "Failed to create Boolean variable in MIP");
        scip_assert(SCIPaddVar(mip_, ind_var));

        // Get variables.
        const Int size = vars.size();
        Vector<SCIP_VAR*> mip_vars(size);
        Vector<Float> mip_coeffs(size);
        for (Int idx = 0; idx < size; ++idx)
        {
            mip_vars[idx] = mip_var(vars[idx]);
            mip_coeffs[idx] = coeffs[idx];
        }

        // Create constraint < rhs.
        {
            SCIP_CONS* cons;
            scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                     &cons,
                                                     "",
                                                     ind_var,
                                                     size,
                                                     mip_vars.data(),
                                                     mip_coeffs.data(),
                                                     rhs - 1));
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }

        // Change coefficients.
        for (Int idx = 0; idx < size; ++idx)
        {
            mip_coeffs[idx] *= -1;
        }

        // Create constraint > rhs.
        {
            SCIP_VAR* neg_ind_var = ind_var;
            scip_assert(SCIPgetNegatedVar(mip_, neg_ind_var, &neg_ind_var));

            SCIP_CONS* cons;
            scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                     &cons,
                                                     "",
                                                     neg_ind_var,
                                                     size,
                                                     mip_vars.data(),
                                                     mip_coeffs.data(),
                                                     -(rhs + 1)));
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }

        // Release indicator variable.
        scip_assert(SCIPreleaseVar(mip_, &ind_var));
    }

    // Create constraint in CP.
    CREATE_CP_CONSTRAINT:
    {
        const Int size = vars.size();
        vec<geas::intvar> cp_vars(size);
        vec<int> cp_coeffs(size);
        for (Int idx = 0; idx < size; ++idx)
        {
            cp_vars[idx] = cp_var(vars[idx]);
            cp_coeffs[idx] = coeffs[idx];
        }
        geas_add_constr(geas::linear_ne(cp_.data, cp_coeffs, cp_vars, rhs));
    }

    // Success.
    return true;
}

bool Model::add_constr_element(const IntVar& idx_var, const Vector<Int>& array, const IntVar& val_var)
{
    // Create constraint in MIP.
    if (mip_var(idx_var) && mip_var(val_var))
    {
        // Create indicator variables.
//        release_assert(lb(idx_var) == 1,
//                       "Domain min of variable {} in element constraint must be 0",
//                       name(idx_var), lb(idx_var));
//        release_assert(ub(idx_var) == static_cast<Int>(array.size()),
//                       "Domain max of variable {} in element constraint is {} must be length of the array",
//                       name(idx_var), ub(idx_var));
        add_indicator_vars(idx_var);

        // The index variable cannot take values outside the array index set.
        const auto size = static_cast<Int>(array.size());
        for (Int val = lb(idx_var); val < 1; ++val)
        {
            auto ind_var = mip_indicator_var(idx_var, val);
            release_assert(ind_var, "Indicator variable missing while creating element constraint");

            SCIP_Bool infeasible = FALSE;
            SCIP_Bool fixed = FALSE;
            scip_assert(SCIPfixVar(mip_, ind_var, 0, &infeasible, &fixed));
            release_assert(fixed, "Failed to fix variable {}", name(idx_var));
            if (infeasible)
            {
                status_ = Status::Infeasible;
                return false;
            }
        }
        for (Int val = size + 1; val <= ub(idx_var); ++val)
        {
            auto ind_var = mip_indicator_var(idx_var, val);
            release_assert(ind_var, "Indicator variable missing while creating element constraint");

            SCIP_Bool infeasible = FALSE;
            SCIP_Bool fixed = FALSE;
            scip_assert(SCIPfixVar(mip_, ind_var, 0, &infeasible, &fixed));
            release_assert(fixed, "Failed to fix variable {}", name(idx_var));
            if (infeasible)
            {
                status_ = Status::Infeasible;
                return false;
            }
        }
        geas_add_constr(cp_.post(cp_var(idx_var) >= 1));
        geas_add_constr(cp_.post(cp_var(idx_var) <= size));

        // Create indicator constraints.
        for (Int idx = std::max(1, lb(idx_var)); idx <= std::min(size, ub(idx_var)); ++idx)
        {
            // Get variables.
            auto ind_var = mip_indicator_var(idx_var, idx);
            release_assert(ind_var, "Indicator variable missing while creating element constraint");
            auto mip_val_var = mip_var(val_var);

            // Create constraint <=.
            {
                SCIP_CONS* cons;
                Float coeff = 1;
                Float rhs = array[idx - 1];
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         ind_var,
                                                         1,
                                                         &mip_val_var,
                                                         &coeff,
                                                         rhs));
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }

            // Create constraint >=.
            {
                SCIP_CONS* cons;
                Float coeff = -1;
                Float rhs = -array[idx - 1];
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         ind_var,
                                                         1,
                                                         &mip_val_var,
                                                         &coeff,
                                                         rhs));
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }
        }
    }

    // Create constraint in CP.
    {
        const Int size = array.size();
        vec<int> cp_array(size);
        for (Int idx = 0; idx < size; ++idx)
        {
            cp_array[idx] = array[idx];
        }
        geas::int_element(cp_.data, cp_var(val_var), cp_var(idx_var), cp_array);
    }

    // Success.
    return true;
}

bool Model::add_constr_element(const IntVar& idx_var, const Vector<IntVar>& array, const IntVar& val_var)
{
    // Create constraint in MIP.
    if (mip_var(idx_var) && mip_var(val_var))
    {
        // Check that all variables appear in the MIP.
        for (const auto var : array)
            if (!mip_var(var))
            {
                goto CREATE_CP_CONSTRAINT;
            }

        // Create indicator variables.
//        release_assert(lb(idx_var) == 1,
//                       "Domain min of variable {} in element constraint must be 0",
//                       name(idx_var), lb(idx_var));
//        release_assert(ub(idx_var) == static_cast<Int>(array.size()),
//                       "Domain max of variable {} in element constraint is {} must be length of the array",
//                       name(idx_var), ub(idx_var));
        add_indicator_vars(idx_var);

        // The index variable cannot take values outside the array index set.
        const auto size = static_cast<Int>(array.size());
        for (Int val = lb(idx_var); val < 1; ++val)
        {
            auto ind_var = mip_indicator_var(idx_var, val);
            release_assert(ind_var, "Indicator variable missing while creating element constraint");

            SCIP_Bool infeasible = FALSE;
            SCIP_Bool fixed = FALSE;
            scip_assert(SCIPfixVar(mip_, ind_var, 0, &infeasible, &fixed));
            release_assert(fixed, "Failed to fix variable {}", name(idx_var));
            if (infeasible)
            {
                status_ = Status::Infeasible;
                return false;
            }
        }
        for (Int val = size + 1; val <= ub(idx_var); ++val)
        {
            auto ind_var = mip_indicator_var(idx_var, val);
            release_assert(ind_var, "Indicator variable missing while creating element constraint");

            SCIP_Bool infeasible = FALSE;
            SCIP_Bool fixed = FALSE;
            scip_assert(SCIPfixVar(mip_, ind_var, 0, &infeasible, &fixed));
            release_assert(fixed, "Failed to fix variable {}", name(idx_var));
            if (infeasible)
            {
                status_ = Status::Infeasible;
                return false;
            }
        }
        geas_add_constr(cp_.post(cp_var(idx_var) >= 1));
        geas_add_constr(cp_.post(cp_var(idx_var) <= size));

        // Create indicator constraints.
        for (Int idx = std::max(1, lb(idx_var)); idx <= std::min(size, ub(idx_var)); ++idx)
        {
            // Get variables.
            auto ind_var = mip_indicator_var(idx_var, idx);
            release_assert(ind_var, "Indicator variable missing while creating element constraint");
            Vector<SCIP_VAR*> mip_vars{mip_var(val_var), mip_var(array[idx - 1])};

            // Create constraint <=.
            {
                Vector<Float> coeffs{1, -1};
                SCIP_CONS* cons;
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         ind_var,
                                                         2,
                                                         mip_vars.data(),
                                                         coeffs.data(),
                                                         0));
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }

            // Create constraint >=.
            {
                Vector<Float> coeffs{-1, 1};
                SCIP_CONS* cons;
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         ind_var,
                                                         2,
                                                         mip_vars.data(),
                                                         coeffs.data(),
                                                         0));
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }
        }
    }

    // Create constraint in CP.
    CREATE_CP_CONSTRAINT:
    {
        const Int size = array.size();
        vec<geas::intvar> cp_array(size);
        for (Int idx = 0; idx < size; ++idx)
        {
            cp_array[idx] = cp_var(array[idx]);
        }
        geas::var_int_element(cp_.data, cp_var(val_var), cp_var(idx_var), cp_array);
    }

    // Success.
    return true;
}

bool Model::add_constr_alldifferent(const Vector<IntVar>& vars)
{
    // Check.
    for (const auto var : vars)
    {
        release_assert(var.is_valid(),
                       "Variable is not valid in creating alldifferent constraint");
    }

    // Create constraint/relaxation in MIP.
    {
        // Create binarization variables if not already created. Also, get the minimum and
        // maximum value of the variables.
        auto min_val = std::numeric_limits<Int>::max();
        auto max_val = std::numeric_limits<Int>::min();
        for (const auto var : vars)
            if (mip_var(var))
            {
                add_indicator_vars(var);

                if (lb(var) < min_val)
                    min_val = lb(var);
                if (ub(var) > max_val)
                    max_val = ub(var);
            }

        // Every value can appear at most once.
        for (Int k = min_val; k <= max_val; ++k)
        {
            SCIP_CONS* cons = nullptr;
            scip_assert(SCIPcreateConsBasicSetpack(mip_, &cons, "", 0, nullptr));
            debug_assert(cons);
            for (const auto var : vars)
                if (mip_var(var) && has_mip_indicator_vars(var) && lb(var) <= k && k <= ub(var))
                {
                    auto ind_var = mip_indicator_var(var, k);
                    release_assert(ind_var, "Missing indicator variable while creating alldifferent constraint");
                    scip_assert(SCIPaddCoefSetppc(mip_, cons, ind_var));
                }
            if (SCIPgetNVarsSetppc(mip_, cons) > 1)
            {
                scip_assert(SCIPaddCons(mip_, cons));
            }
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }
    }

    // Create constraint in CP.
    {
        const Int N = vars.size();
        vec<geas::intvar> cp_vars(N);
        for (Int idx = 0; idx < N; ++idx)
            cp_vars[idx] = cp_var(vars[idx]);
        geas_add_constr(geas::all_different_int(cp_.data, cp_vars));
    }

    // Success
    return true;
}

}
