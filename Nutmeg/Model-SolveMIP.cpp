//#define PRINT_DEBUG

#include "Model.h"

namespace Nutmeg
{

void Model::minimize_using_mip(
    const IntVar obj_var,
    const Float time_limit,
    const bool verbose
)
{
    // Add objective function.
    release_assert(obj_var.model == this, "Objective variable belongs to a different model");
    release_assert(0 <= obj_var.idx && obj_var.idx < nb_int_vars(), "Objective variable is invalid");
    release_assert(mip_var(obj_var), "Objective variable is not in the MIP model");
    scip_assert(SCIPchgVarObj(mip_, mip_var(obj_var), 1.0));
    probdata_.obj_var_idx_ = obj_var.idx;

    // Create space to store solution.
    sol_.bool_vars_sol_.resize(nb_bool_vars());
    sol_.int_vars_sol_.resize(nb_int_vars(), std::numeric_limits<Int>::max());

    // Write LP to file.
//    scip_assert(SCIPwriteOrigProblem(mip_, "model.lp", 0, 0));

    // Turn off screen log.
    if (!verbose)
    {
        scip_assert(SCIPsetIntParam(mip_, "display/verblevel", 0));
    }

    // Start timer.
    if (time_limit < Infinity)
    {
        scip_assert(SCIPsetRealParam(mip_, "limits/time", time_limit));
    }
    start_timer(time_limit);

    // Solve.
    scip_assert(SCIPsolve(mip_));
//    scip_assert(SCIPwriteTransProblem(mip_, "model_transform.lp", 0, 0));

    // Stop timer.
    run_time_ = get_cpu_time();

    // Print statistics.
    println("");
    scip_assert(SCIPprintStatistics(mip_, nullptr));

    // Get status.
    const auto status = SCIPgetStatus(mip_);
    release_assert(status == SCIP_STATUS_TIMELIMIT ||
                   status == SCIP_STATUS_OPTIMAL ||
                   status == SCIP_STATUS_INFEASIBLE,
                   "Invalid status {} after solving", status);

    // Get solution.
    auto sol = SCIPgetBestSol(mip_);
    if (sol)
    {
        // Store solution.
        for (Int idx = 0; idx < nb_bool_vars(); ++idx)
        {
            const auto mip_var = probdata_.mip_bool_vars_[idx];
            sol_.bool_vars_sol_[idx] = SCIPround(mip_, SCIPgetSolVal(mip_, sol, mip_var));
        }
        for (Int idx = 0; idx < nb_int_vars(); ++idx)
        {
            const auto mip_var = probdata_.mip_int_vars_[idx];
            if (mip_var)
            {
                sol_.int_vars_sol_[idx] = SCIPround(mip_, SCIPgetSolVal(mip_, sol, mip_var));
            }
        }

        // Store objective value.
        obj_ = SCIPround(mip_, SCIPgetSolOrigObj(mip_, sol));
        release_assert(obj_ == sol_.int_vars_sol_[obj_var.idx],
                       "Objective value mismatch {} {}", obj_, sol_.int_vars_sol_[obj_var.idx]);

        // Check.
        debug_assert(SCIPisIntegral(mip_, SCIPgetSolOrigObj(mip_, sol)));
        debug_assert(SCIPisIntegral(mip_, SCIPgetPrimalbound(mip_)));;
        debug_assert(obj_ == SCIPround(mip_, SCIPgetPrimalbound(mip_)));
    }

    // Get dual bound.
    obj_bound_ = SCIPceil(mip_, SCIPgetDualbound(mip_));

    // Get status.
    if (status == SCIP_STATUS_OPTIMAL)
    {
        debug_assert(sol);
        status_ = Status::Optimal;
    }
    else if (status == SCIP_STATUS_INFEASIBLE)
    {
        debug_assert(!sol);
        status_ = Status::Infeasible;
    }
    else
    {
        debug_assert(status == SCIP_STATUS_TIMELIMIT);
        if (sol)
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
        println("Method: MIP");
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
