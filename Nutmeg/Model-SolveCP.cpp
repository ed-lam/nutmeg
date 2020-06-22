//#define PRINT_DEBUG

#include "Model.h"
#include "geas/solver/solver.h"

namespace Nutmeg
{

void Model::minimize_using_cp(
    const IntVar obj_var,
    const Float time_limit,
    const bool verbose
)
{
    // Declare.
    geas::solver::result result = geas::solver::result::UNKNOWN;

    // Check for failure at the root level.
    if (status_ == Status::Infeasible)
    {
        goto EXIT;
    }

    // Add objective function.
    release_assert(obj_var.model == this, "Objective variable belongs to a different model");
    release_assert(0 <= obj_var.idx && obj_var.idx < nb_int_vars(), "Objective variable is invalid");
    probdata_.obj_var_idx_ = obj_var.idx;

    // Set dual bound.
    obj_bound_ = lb(obj_var);

    // Create space to store solution.
    sol_.bool_vars_sol_.resize(nb_bool_vars());
    sol_.int_vars_sol_.resize(nb_int_vars(), std::numeric_limits<Int>::max());

    // Start timer.
    start_timer(time_limit);

    // Solve.
    SOLVE:
    result = cp_.solve(limits{.time = get_time_remaining(), .conflicts = 0});

    // Get solution.
    if (result == geas::solver::SAT)
    {
        // Store objective value.
        obj_ = probdata_.cp_int_vars_[obj_var.idx].lb(cp_.data);
        debug_assert(obj_ < sol_.int_vars_sol_[obj_var.idx]);

        // Store solution.
        for (Int idx = 0; idx < nb_bool_vars(); ++idx)
        {
            const auto& cp_var = probdata_.cp_bool_vars_[idx];
            sol_.bool_vars_sol_[idx] = cp_var.lb(cp_.data->state.p_vals);
        }
        for (Int idx = 0; idx < nb_int_vars(); ++idx)
        {
            const auto& cp_var = probdata_.cp_int_vars_[idx];
            sol_.int_vars_sol_[idx] = cp_var.lb(cp_.data);
        }

        // Print solution.
        if (verbose)
        {
            println("Found new solution with objective value {}", obj_);
        }
        if (print_new_solution_function_)
        {
            print_new_solution_function_();
        }

        // Tighten primal bound.
        if (cp_.post(probdata_.cp_int_vars_[obj_var.idx] < obj_))
        {
            // Solve again.
            goto SOLVE;
        }
        else
        {
            // Became infeasible.
            result = geas::solver::UNSAT;
        }
    }

    // Stop timer.
    run_time_ = get_cpu_time();

    // Get status.
    debug_assert(result == geas::solver::UNSAT || result == geas::solver::UNKNOWN);
    EXIT:
    const auto found_sol = obj_ < Infinity;
    if (result == geas::solver::UNSAT)
    {
        if (found_sol)
        {
            status_ = Status::Optimal;
            obj_bound_ = obj_;
        }
        else
        {
            status_ = Status::Infeasible;
        }
    }
    else
    {
        if (found_sol)
        {
            status_ = Status::Feasible;
        }
        else
        {
            status_ = Status::Unknown;
        }
    }

    // Print status.
    if (verbose)
    {
        println("");
        println("--------------------------------------------------");
        println("Method: CP");
        println("CPU time: {:.2f} seconds", run_time_);
        println("Status: {}",
                status_ == Status::Unknown ? "Unknown" :
                status_ == Status::Optimal ? "Optimal" :
                status_ == Status::Feasible ? "Feasible" :
                status_ == Status::Infeasible ? "Infeasible" :
                "Error");
        if (obj_ < Infinity)
        {
            println("Objective value: {:.2f}", obj_);
        }
        if (obj_bound_ > -Infinity && status_ != Status::Infeasible)
        {
            println("Objective bound: {:.2f}", obj_bound_);
        }
        println("--------------------------------------------------");
    }

    // Print solution.
#ifdef PRINT_DEBUG
    if (status_ == Status::Optimal || status_ == Status::Feasible)
    {
        println("");
        println("Solution:");
        for (Int idx = 0; idx < nb_int_vars(); ++idx)
        {
            println("   {} = {}", probdata_.int_vars_name_[idx], sol_.int_vars_sol_[idx]);
        }
        for (Int idx = 0; idx < nb_bool_vars(); ++idx)
        {
            println("   {} = {}", probdata_.bool_vars_name_[idx], sol_.bool_vars_sol_[idx]);
        }
    }
#endif
}

}
