//#define PRINT_DEBUG

#include "Model.h"
#include "Variable.h"
#include "Matrix.h"
#include "scip/cons_linear.h"
#include "scip/cons_knapsack.h"
#include "scip/cons_setppc.h"
#include "scip/cons_indicator.h"
#include <numeric>

#define geas_add_constr(expr) if (!expr) { status_ = Status::Infeasible; return false; }

namespace Nutmeg
{

bool
Model::add_constr_linear(
    const Vector<BoolVar>& vars,
    const Vector<Int>& coeffs,
    const Sign sign,
    const Int rhs,
    const IntVar rhs_var,
    const Int rhs_coeff
)
{
    // Check.
    release_assert(sign == Sign::EQ || sign == Sign::LE || sign == Sign::GE,
                   "Linear constraint only supports <=, == or >=");
    release_assert(vars.size() == coeffs.size(),
                   "Vectors of variables and coefficients have different lengths in "
                   "creating linear constraint");
    for (const auto var : vars)
    {
        release_assert(var.is_valid(),
                       "Variable is not valid in creating linear constraint");
    }

    // Create set partition constraint if the linear constraint can be upgraded.
    if ((rhs_coeff == 0 || !rhs_var.is_valid() || (lb(rhs_var) == 0 && ub(rhs_var) == 0)) &&
        sign == Sign::EQ &&
        rhs == 1)
    {
        for (const auto coeff : coeffs)
            if (coeff != 1)
                goto CREATE_MIP_CONSTRAINT;

        return add_constr_set_partition(vars);
    }

    // Create constraint in MIP.
    CREATE_MIP_CONSTRAINT:
    if (rhs_coeff == 0 || !rhs_var.is_valid() || (lb(rhs_var) == 0 && ub(rhs_var) == 0) || mip_var(rhs_var))
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
            const auto coeff = coeffs[idx];

            // Bool vars must appear in MIP
            scip_assert(SCIPaddCoefLinear(mip_, cons, mip_var(var), coeff));
        }
        if (rhs_coeff != 0 && rhs_var.is_valid() && !(lb(rhs_var) == 0 && ub(rhs_var) == 0) && mip_var(rhs_var))
        {
            scip_assert(SCIPaddCoefLinear(mip_, cons, mip_var(rhs_var), -rhs_coeff));
        }
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }
    else
    {
        release_assert(method_ != Method::MIP,
                       "Invalid RHS for linear constraint when solving with MIP");
    }

    // Create constraint in CP.
    {
        vec<geas::patom_t> cp_vars;
        vec<int> cp_coeffs;
        for (size_t i = 0; i < vars.size(); ++i)
        {
            cp_vars.push(cp_var(vars[i]));
            cp_coeffs.push(coeffs[i]);
        }

        geas::intvar rhs_cp_var;
        if (rhs_coeff != 0 && rhs_var.is_valid() && !(lb(rhs_var) == 0 && ub(rhs_var) == 0))
        {
            if (rhs_coeff == 1)
            {
                rhs_cp_var = cp_var(rhs_var);
            }
            else if (rhs_coeff == -1)
            {
                rhs_cp_var = -cp_var(rhs_var);
            }
            else
            {
                // new_var = rhs_coeff * rhs_var
                Int rhs_lb, rhs_ub;
                if (rhs_coeff > 0)
                {
                    rhs_lb = rhs_coeff * lb(rhs_var);
                    rhs_ub = rhs_coeff * ub(rhs_var);
                }
                else
                {
                    rhs_lb = rhs_coeff * ub(rhs_var);
                    rhs_ub = rhs_coeff * lb(rhs_var);
                }
                rhs_cp_var = cp_.new_intvar(rhs_lb, rhs_ub);

                vec<geas::intvar> vars;
                vec<int> coeffs;

                vars.push(rhs_cp_var);
                vars.push(cp_var(rhs_var));
                coeffs.push(-1);
                coeffs.push(rhs_coeff);
                geas_add_constr(geas::linear_le(cp_.data, coeffs, vars, 0, geas::at_True));

                coeffs[0] = -coeffs[0];
                coeffs[1] = -coeffs[1];
                geas_add_constr(geas::linear_le(cp_.data, coeffs, vars, 0, geas::at_True));
            }
        }
        else
        {
            rhs_cp_var = cp_var(get_zero());
        }

        if (sign != Sign::GE)
        {
            // coeffs[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] <= rhs + rhs_var
            // coeffs[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] - rhs <= rhs_var
            // rhs_var >= coeffs[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] - rhs
            geas_add_constr(geas::bool_linear_ge(cp_.data,
                                                 geas::at_True,
                                                 rhs_cp_var,
                                                 cp_coeffs,
                                                 cp_vars,
                                                 -rhs));
        }
        if (sign != Sign::LE)
        {
            // coeffs[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] >= rhs + rhs_var
            // coeffs[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] - rhs >= rhs_var
            // rhs_var <= coeffs[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] - rhs
            geas_add_constr(geas::bool_linear_le(cp_.data,
                                                 geas::at_True,
                                                 rhs_cp_var,
                                                 cp_coeffs,
                                                 cp_vars,
                                                 -rhs));
        }
    }

    // Success.
    return true;
}

bool
Model::add_constr_linear(
    const Vector<IntVar>& vars,
    const Vector<Vector<Int>>& coeffs,
    const Sign sign,
    const Int rhs,
    const IntVar rhs_var,
    const Int rhs_coeff
)
{
    // Check.
    release_assert(sign == Sign::EQ || sign == Sign::LE || sign == Sign::GE,
                   "Linear constraint only supports <=, == or >=");
    release_assert(vars.size() == coeffs.size(),
                   "Vectors of variables and coefficients have different lengths in "
                   "creating linear constraint");
    const Int N = vars.size();
    for (Int idx = 0; idx < N; ++idx)
    {
        auto var = vars[idx];

        release_assert(var.is_valid(),
                       "Variable is not valid in creating linear constraint");

        const auto size = ub(var) - lb(var) + 1;
        release_assert(size == static_cast<Int>(coeffs[idx].size()),
                       "Vector of vector of coefficients has wrong number of values in "
                       "position {} in creating linear constraint", idx);
    }

    // Create constraint in MIP.
    if (rhs_coeff == 0 || !rhs_var.is_valid() || (lb(rhs_var) == 0 && ub(rhs_var) == 0) || mip_var(rhs_var))
    {
        // Create binarization variables if not already created.
        for (const auto var : vars)
        {
            add_indicator_vars(var);
        }

        // Create constraint.
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
        for (Int idx = 0; idx < N; ++idx)
        {
            auto var = vars[idx];

            for (Int k = lb(var); k <= ub(var); ++k)
            {
                auto bin_var = mip_indicator_var(var, k);
                debug_assert(bin_var);
                const auto coeff = coeffs[idx][k - lb(var)];
                scip_assert(SCIPaddCoefLinear(mip_, cons, bin_var, coeff));
            }
        }
        if (rhs_coeff != 0 && rhs_var.is_valid() && !(lb(rhs_var) == 0 && ub(rhs_var) == 0) && mip_var(rhs_var))
        {
            scip_assert(SCIPaddCoefLinear(mip_, cons, mip_var(rhs_var), -rhs_coeff));
        }
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }
    else
    {
        release_assert(method_ != Method::MIP,
                       "Invalid RHS for linear constraint when solving with MIP");
    }

    // Create constraint in CP.
    {
        // Create a list to store the variables of the linear constraint.
        vec<geas::intvar> cp_vars;
        vec<int> cp_coeffs;

        // Create element constraint for each variable.
        for (Int idx = 0; idx < N; ++idx)
        {
            // Get coefficient for each value in the domain. Create new CP variable
            // storing the coefficient.
            vec<int> val_coeffs;
            auto min_coeff = std::numeric_limits<int>::max();
            auto max_coeff = std::numeric_limits<int>::min();
            for (size_t m = 0; m < coeffs[idx].size(); ++m)
            {
                auto c = coeffs[idx][m];

                val_coeffs.push(c);

                if (c < min_coeff) min_coeff = c;
                if (c > max_coeff) max_coeff = c;
            }
            auto coeff_var = cp_.new_intvar(min_coeff, max_coeff);

            // Create element constraint to link the new CP variable. Add one because
            // element constraints are 1-indexed.
            geas_add_constr(geas::int_element(cp_.data,
                                              coeff_var,
                                              cp_var(vars[idx]) + 1,
                                              val_coeffs));

            // Add the variable to the list of variables of the linear constraint.
            cp_vars.push(coeff_var);
            cp_coeffs.push(1);
        }

        // Append RHS variable.
        if (rhs_coeff != 0 && rhs_var.is_valid() && !(lb(rhs_var) == 0 && ub(rhs_var) == 0))
        {
            cp_vars.push(cp_var(rhs_var));
            cp_coeffs.push(-rhs_coeff);
        }

        // Add the linear constraint.
        if (sign != Sign::GE)
        {
            geas_add_constr(geas::linear_le(cp_.data, cp_coeffs, cp_vars, rhs));
        }
        if (sign != Sign::LE)
        {
            for (Int idx = 0; idx < cp_coeffs.size(); ++idx)
            {
                cp_coeffs[idx] *= -1;
            }

            geas_add_constr(geas::linear_le(cp_.data, cp_coeffs, cp_vars, rhs));
        }
    }

    // Success.
    return true;
}

bool
Model::add_constr_set_partition(
    const Vector<BoolVar>& vars
)
{
    // Create constraint in MIP.
    {
        SCIP_CONS* cons;
        scip_assert(SCIPcreateConsBasicSetpart(mip_, &cons, "", 0, nullptr));
        for (const auto var : vars)
        {
            release_assert(var.is_valid(),
                           "Variable is not valid in creating set_partition constraint");

            // Bool vars must appear in MIP
            scip_assert(SCIPaddCoefSetppc(mip_, cons, mip_var(var)));
        }
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }

    // Create clauses in CP.
    if (vars.empty())
    {
        geas_add_constr(cp_.post(geas::at_False));
    }
    else
    {
        {
            vec<geas::clause_elt> clause;
            for (const auto& var : vars)
                clause.push(cp_var(var));

            geas_add_constr(geas::add_clause(*cp_.data, clause));
        }
        {
            for (size_t i = 0; i < vars.size() - 1; ++i)
                for (size_t j = i + 1; j < vars.size(); ++j)
                {
                    geas_add_constr(geas::add_clause(cp_.data,
                                                     ~cp_var(vars[i]),
                                                     ~cp_var(vars[j])));
                }
        }
    }

    // Success.
    return true;
}

bool
Model::add_constr_subtraction_leq(
    const IntVar x,
    const IntVar y,
    const Int rhs
)
{
    // Check.
    release_assert(x.is_valid() && y.is_valid(),
                   "Variable is not valid in creating subtraction_leq constraint");

    // Create constraint in MIP.
    if (mip_var(x) && mip_var(y))
    {
        SCIP_VAR* vars[2]{mip_var(x), mip_var(y)};
        SCIP_Real coeffs[2]{1, -1};
        SCIP_CONS* cons;
        scip_assert(SCIPcreateConsBasicLinear(mip_,
                                              &cons,
                                              "",
                                              2,
                                              vars,
                                              coeffs,
                                              -SCIPinfinity(mip_),
                                              rhs));
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }

    // Create constraint in CP.
    geas_add_constr(geas::int_le(cp_.data, cp_var(x), cp_var(y), rhs));

    // Success.
    return true;
}

bool
Model::add_constr_reify_subtraction_leq(
    const BoolVar r,
    const IntVar x,
    const IntVar y,
    const Int rhs
)
{
    // Check.
    release_assert(r.is_valid() && x.is_valid() && y.is_valid(),
                   "Variable is not valid in creating reify_subtraction_leq constraint");

    // Create constraint in MIP.
    if (mip_var(x) && mip_var(y))
    {
        // r -> x - y <= rhs
        SCIP_VAR* vars[2]{mip_var(x), mip_var(y)};
        SCIP_Real coeffs[2]{1, -1};
        SCIP_CONS* cons;
        scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                 &cons,
                                                 "",
                                                 mip_var(r),
                                                 2,
                                                 vars,
                                                 coeffs,
                                                 rhs));
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }

    // Create constraint in CP.
    geas_add_constr(geas::int_le(cp_.data, cp_var(x), cp_var(y), rhs, cp_var(r)));

    // Success.
    return true;
}

bool
Model::add_constr_imply(
    const BoolVar r,
    const bool r_val,
    const IntVar x,
    const Sign sign,
    const Int x_val
)
{
    // Check.
    release_assert(sign == Sign::EQ || sign == Sign::LE || sign == Sign::GE,
                   "Imply constraint only supports <=, == or >=");

    // Add constraint in MIP.
    if (mip_var(x))
    {
        // Get LHS variable.
        auto r_mip_var = mip_var(r);
        if (!r_val)
        {
            scip_assert(SCIPgetNegatedVar(mip_, r_mip_var, &r_mip_var));
        }

        // Get RHS variable.
        auto x_mip_var = mip_var(x);

        // r -> x <= x_val
        if (sign != Sign::GE)
        {
            Float coeff = 1;
            SCIP_CONS* cons;
            scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                     &cons,
                                                     "",
                                                     r_mip_var,
                                                     1,
                                                     &x_mip_var,
                                                     &coeff,
                                                     x_val));
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }

        // r -> x >= x_val
        // r -> -x <= -x_val
        if (sign != Sign::LE)
        {
            Float coeff = -1;
            SCIP_CONS* cons;
            scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                     &cons,
                                                     "",
                                                     r_mip_var,
                                                     1,
                                                     &x_mip_var,
                                                     &coeff,
                                                     -x_val));
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }
    }

    // Create constraint in CP.
    // r_lit -> x_lit
    // ~r_lit \/ x_lit
    auto r_lit = r_val ? cp_var(r) : ~cp_var(r);
    auto x_lit = sign == Sign::EQ ? (cp_var(x) == x_val) :
                 sign == Sign::LE ? (cp_var(x) <= x_val) :
                                    (cp_var(x) >= x_val);
    geas_add_constr(geas::add_clause(cp_.data, ~r_lit, x_lit));

    // Success.
    return true;
}

bool
Model::add_constr_cumulative(
    const Vector<IntVar>& start,
    const Vector<Int>& duration,
    const Vector<Int>& resource,
    const Int capacity,
    const bool create_mip_linearization
)
{
    // Check.
    release_assert(start.size() == duration.size() &&
                   duration.size() == resource.size(),
                   "Vectors of tasks have different lengths in creating cumulative "
                   "constraint");
    const Int N = start.size();

    // Create constraint in MIP if solving using MIP.
    if (method_ == Method::MIP || create_mip_linearization)
    {
        // Create binarization variables.
        for (Int j = 0; j < N; ++j)
        {
            add_indicator_vars(start[j]);
        }

        // Calculate earliest start time and latest start time.
        Int e = std::numeric_limits<Int>::max();
        Int l = std::numeric_limits<Int>::min();
        for (Int j = 0; j < N; ++j)
        {
            if (lb(start[j]) < e)
                e = lb(start[j]);
            if (ub(start[j]) > l)
                l = ub(start[j]);
        }

        // Create resource constraints.
        for (Int t = e; t <= l; ++t)
        {
            // Create constraint.
            SCIP_CONS* cons = nullptr;
            scip_assert(SCIPcreateConsBasicKnapsack(mip_,
                                                    &cons,
                                                    "",
                                                    0,
                                                    nullptr,
                                                    nullptr,
                                                    capacity));
            debug_assert(cons);

            // Add variables to constraint.
            for (Int j = 0; j < N; ++j)
                if (resource[j] > 0 && duration[j] > 0)
                {
                    for (Int u = std::max(t - duration[j] + 1, lb(start[j]));
                         u <= std::min(t, ub(start[j]));
                         ++u)
                    {
                        auto var = mip_indicator_var(start[j], u);
                        debug_assert(var);
                        SCIPaddCoefKnapsack(mip_, cons, var, resource[j]);
                    }
                }

            // Add constraint.
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }
    }

    // Create constraint in CP.
    {
        vec<geas::intvar> start2;
        vec<int> duration2;
        vec<int> resource2;
        for (Int idx = 0; idx < N; ++idx)
            if (resource[idx] > 0 && duration[idx] > 0)
            {
                release_assert(start[idx].is_valid(),
                               "Variable is not valid in creating cumulative constraint");
                start2.push(cp_var(start[idx]));
                duration2.push(duration[idx]);
                resource2.push(resource[idx]);
            }

        geas_add_constr(geas::cumulative(cp_.data,
                                         start2,
                                         duration2,
                                         resource2,
                                         capacity));
    }

    // Success.
    return true;
}

bool
Model::add_constr_cumulative_optional(
    const Vector<BoolVar>& active,
    const Vector<IntVar>& start,
    const Vector<Int>& duration,
    const Vector<Int>& resource,
    const Int capacity,
    const IntVar makespan
)
{
    // Check.
    release_assert(active.size() == start.size() &&
                   start.size() == duration.size() &&
                   duration.size() == resource.size(),
                   "Vectors of tasks have different lengths in creating "
                   "cumulative_optional constraint");
    const Int N = start.size();

    // Create constraint in MIP if solving using MIP.
    if (method_ == Method::MIP)
    {
        // Calculate earliest start time and latest start time.
        Int e = std::numeric_limits<Int>::max();
        Int l = std::numeric_limits<Int>::min();
        for (Int j = 0; j < N; ++j)
        {
            if (lb(start[j]) < e)
                e = lb(start[j]);
            if (ub(start[j]) > l)
                l = ub(start[j]);
        }
        const auto size = l - e + 1;

        // Create private binarization variables.
        Matrix<SCIP_VAR*> bin_vars(N, size);
        for (Int j = 0; j < N; ++j)
            for (Int t = lb(start[j]); t <= ub(start[j]); ++t)
            {
                // Create name.
                auto bin_name = fmt::format("{{{} = {}}}", name(start[j]), t);

                // Create variable in MIP.
                SCIP_VAR*& var = bin_vars(j, t - e);
                scip_assert(SCIPcreateVarBasic(mip_,
                                               &var,
                                               bin_name.c_str(),
                                               0.0,
                                               1.0,
                                               0.0,
                                               SCIP_VARTYPE_BINARY));
                release_assert(var, "Failed to create Boolean variable in MIP");
                scip_assert(SCIPaddVar(mip_, var));
            }

        // Create resource constraints.
        for (Int t = e; t <= l; ++t)
        {
            // Create constraint.
            SCIP_CONS* cons = nullptr;
            scip_assert(SCIPcreateConsBasicKnapsack(mip_,
                                                    &cons,
                                                    "",
                                                    0,
                                                    nullptr,
                                                    nullptr,
                                                    capacity));
            debug_assert(cons);

            // Add variables to constraint.
            for (Int j = 0; j < N; ++j)
            {
                for (Int u = std::max(t - duration[j] + 1, lb(start[j]));
                     u <= std::min(t, ub(start[j]));
                     ++u)
                {
                    auto var = bin_vars(j, u - e);
                    if (var)
                    {
                        SCIPaddCoefKnapsack(mip_, cons, var, resource[j]);
                    }
                }
            }

            // Add constraint.
            scip_assert(SCIPaddCons(mip_, cons));
            scip_assert(SCIPreleaseCons(mip_, &cons));
        }

        // Create allocation constraints.
        for (Int j = 0; j < N; ++j)
        {
            Vector<SCIP_VAR*> vars;
            vars.reserve(ub(start[j]) - lb(start[j]) + 1);
            for (Int t = lb(start[j]); t <= ub(start[j]); ++t)
            {
                vars.push_back(bin_vars(j, t - e));
            }
            Vector<Float> coeffs(vars.size(), 1);

            // active[j] ->
            // sum(t = lb(start[j]) to ub(start[j])) ([start[j] == t]) <= 1
            {
                SCIP_CONS* cons = nullptr;
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         mip_var(active[j]),
                                                         vars.size(),
                                                         vars.data(),
                                                         coeffs.data(),
                                                         1));
                debug_assert(cons);
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }

            // active[j] ->
            // sum(t = lb(start[j]) to ub(start[j])) ([start[j] == t]) >= 1
            {
                std::fill(coeffs.begin(), coeffs.end(), -1);

                SCIP_CONS* cons = nullptr;
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         mip_var(active[j]),
                                                         vars.size(),
                                                         vars.data(),
                                                         coeffs.data(),
                                                         -1));
                debug_assert(cons);
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }

            // active[j] ->
            // sum(t = lb(start[j]) to ub(start[j])) (t * [start[j] == t]) <= start[j]
            {
                std::iota(coeffs.begin(), coeffs.end(), lb(start[j]));

                vars.push_back(mip_var(start[j]));
                coeffs.push_back(-1);

                SCIP_CONS* cons = nullptr;
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         mip_var(active[j]),
                                                         vars.size(),
                                                         vars.data(),
                                                         coeffs.data(),
                                                         0));
                debug_assert(cons);
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }

            // active[j] ->
            // sum(t = lb(start[j]) to ub(start[j])) (t * [start[j] == t]) >= start[j]
            {
                for (auto& coeff : coeffs)
                    coeff *= -1;

                SCIP_CONS* cons = nullptr;
                scip_assert(SCIPcreateConsBasicIndicator(mip_,
                                                         &cons,
                                                         "",
                                                         mip_var(active[j]),
                                                         vars.size(),
                                                         vars.data(),
                                                         coeffs.data(),
                                                         0));
                debug_assert(cons);
                scip_assert(SCIPaddCons(mip_, cons));
                scip_assert(SCIPreleaseCons(mip_, &cons));
            }
        }

        // Release private indicator variables.
        for (Int i = 0; i < static_cast<Int>(bin_vars.rows()); ++i)
            for (Int j = 0; j < static_cast<Int>(bin_vars.cols()); ++j)
                if (bin_vars(i,j))
                {
                    scip_assert(SCIPreleaseVar(mip_, &bin_vars(i,j)));
                }
    }

    // Create constraint in CP.
    {
        vec<geas::patom_t> active2(N);
        vec<geas::intvar> start2(N);
        vec<geas::intvar> duration2(N);
        vec<int> resource2(N);
        for (Int idx = 0; idx < N; ++idx)
        {
            release_assert(active[idx].is_valid() && start[idx].is_valid(),
                           "Variable is not valid in creating cumulative_optional constraint");
            active2[idx] = cp_var(active[idx]);
            start2[idx] = cp_var(start[idx]);
            duration2[idx] = cp_.new_intvar(duration[idx], duration[idx]);
            resource2[idx] = resource[idx];
        }
        geas_add_constr(geas::cumulative_sel(cp_.data,
                                             start2,
                                             duration2,
                                             resource2,
                                             active2,
                                             capacity));
    }

    // Create relaxation in MIP.
    // sum(t in tasks) (resource[t] * duration[t] * optional_indicator[t]) <=
    // capacity * makespan
    if (!makespan.is_valid() || !mip_var(makespan))
    {
        // Compute RHS.
        Int rhs = 0;
        if (makespan.is_valid())
        {
            rhs = ub(makespan);
        }
        else
        {
            for (Int idx = 0; idx < N; ++idx)
                if (ub(start[idx]) + duration[idx] > rhs)
                    rhs = ub(start[idx]) + duration[idx];
        }
        rhs *= capacity;

        // Create constraint.
        SCIP_CONS* cons;
        scip_assert(SCIPcreateConsBasicKnapsack(mip_,
                                                &cons,
                                                "",
                                                0,
                                                nullptr,
                                                nullptr,
                                                rhs));
        for (Int idx = 0; idx < N; ++idx)
        {
            scip_assert(SCIPaddCoefKnapsack(mip_,
                                            cons,
                                            mip_var(active[idx]),
                                            resource[idx] * duration[idx]));
        }
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }
    else
    {
        // Create constraint.
        SCIP_CONS* cons;
        scip_assert(SCIPcreateConsBasicLinear(mip_,
                                              &cons,
                                              "",
                                              0,
                                              nullptr,
                                              nullptr,
                                              -SCIPinfinity(mip_),
                                              0));
        for (Int idx = 0; idx < N; ++idx)
        {
            scip_assert(SCIPaddCoefLinear(mip_,
                                          cons,
                                          mip_var(active[idx]),
                                          resource[idx] * duration[idx]));
        }
        scip_assert(SCIPaddCoefLinear(mip_,
                                      cons,
                                      mip_var(makespan),
                                      -capacity));
        scip_assert(SCIPaddCons(mip_, cons));
        scip_assert(SCIPreleaseCons(mip_, &cons));
    }

    // Success.
    return true;
}

}
