//#define PRINT_DEBUG

#include "Model.h"
#include "geas/solver/solver.h"
#include "geas/constraints/builtins.h"
#include "geas/vars/pred_var.h"
#include "geas/vars/monitor.h"

namespace Nutmeg
{

void Model::minimize_using_lbbd(
    const IntVar obj_var,
    const Float time_limit,
    const bool verbose
)
{
    not_yet_implemented();

//    TODO
//    // Check for failure at the root level.
//    if (status_ == Status::Infeasible)
//    {
//        goto EXIT;
//    }
//
//    // Add objective function.
//    release_assert(obj_var.model == this,
//                   "Objective variable belongs to a different model");
//    release_assert(obj_var.is_valid(), "Objective variable is not valid");
//    release_assert(has_mip_var(obj_var), "Objective variable must appear in the MIP");
//    scip_assert(SCIPchgVarObj(mip_, mip_var(obj_var), 1.0));
//    probdata_.obj_var_idx_ = obj_var.idx;
//
//    // Create space to store solution.
//    sol_.bool_vars_sol_.resize(nb_bool_vars());
//    sol_.int_vars_sol_.resize(nb_int_vars(), std::numeric_limits<Int>::max());
//
//    // Write LP to file.
////    scip_assert(SCIPwriteOrigProblem(mip_, "model.lp", 0, 0));
//
//    // Disable outputs.
//    SCIPsetMessagehdlrQuiet(mip_, TRUE);
//
//    // Start timer.
//    start_timer(time_limit);
//
//    // Main loop.
//    while (true)
//    {
//        // Set time limit.
//        {
//            const auto iter_time_limit = get_time_remaining();
//            if (iter_time_limit < 1e-4)
//            {
//                break;
//            }
//            else if (iter_time_limit < Infinity)
//            {
//                scip_assert(SCIPsetRealParam(mip_, "limits/time", iter_time_limit));
//            }
//        }
//
//        // Solve.
//        scip_assert(SCIPsolve(mip_));
//
//        // Get status.
//        const auto scip_status = SCIPgetStatus(mip_);
//        release_assert(scip_status == SCIP_STATUS_TIMELIMIT ||
//                       scip_status == SCIP_STATUS_OPTIMAL ||
//                       scip_status == SCIP_STATUS_INFEASIBLE,
//                       "Invalid SCIP status {} after solving", scip_status);
//
//        // Get solution.
//        auto sol = SCIPgetBestSol(mip_);
//
//        // Get dual bound.
//        const auto obj_bound = SCIPceil(mip_, SCIPgetSolOrigObj(mip_, sol));
//        if (obj_bound > obj_bound_)
//        {
//            obj_bound_ = obj_bound;
//            println("Found new dual bound {}", obj_bound_);
//        }
//
//        // Get status.
//        if (scip_status == SCIP_STATUS_INFEASIBLE)
//        {
//            status_ = Status::Infeasible;
//            break;
//        }
//        else if (scip_status == SCIP_STATUS_TIMELIMIT)
//        {
//            status_ = obj_ < Infinity ? Status::Feasible : Status::Unknown;
//            break;
//        }
//
//        // Declare.
//        geas::solver::result cp_result;
//        Float time_remaining;
//
//        // Check CP subproblem only with assumptions on Boolean variables.
//        {
//            // Make assumptions.
//            debugln("   Assumptions:");
//            cp_.clear_assumptions();
//            if (!make_bool_assumptions(mip_, sol, probdata_, cp_))
//            {
//                debugln("   Assumptions infeasible");
//                goto GET_CONFLICT;
//            }
//            debugln("   Assumptions completed");
//
//            // Get time remaining.
//            time_remaining = get_time_remaining();
//            if (time_remaining <= 0)
//            {
//                status_ = obj_ < Infinity ? Status::Feasible : Status::Unknown;
//                break;
//            }
//            else if (time_remaining == Infinity)
//            {
//                time_remaining = 0;
//            }
//
//            // Solve.
//            {
//#ifdef PRINT_DEBUG
//                debugln("   Calling Geas");
//                const auto start_time = clock();
//#endif
//                cp_result = cp_.solve(limits{.time = time_remaining, .conflicts = 0});
//#ifdef PRINT_DEBUG
//                debugln("   Geas run time = {:.3f}",
//                        static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
//#endif
//            }
//        }
//
//        // If satisfied, also make assumptions on objective variable.
//        if (cp_result == geas::solver::SAT)
//        {
//            // Make additional assumptions.
//            debugln("   Assumptions:");
//            cp_.clear_assumptions();
//            if (!make_bool_assumptions(mip_, sol, probdata_, cp_) ||
//                !make_obj_assumptions(mip_, sol, probdata_, cp_))
//            {
//                debugln("   Assumptions infeasible");
//                goto GET_CONFLICT;
//            }
//            debugln("   Assumptions completed");
//
//            // Get time remaining.
//            time_remaining = get_time_remaining();
//            if (time_remaining <= 0)
//            {
//                status_ = obj_ < Infinity ? Status::Feasible : Status::Unknown;
//                break;
//            }
//            else if (time_remaining == Infinity)
//            {
//                time_remaining = 0;
//            }
//
//            // Solve.
//            {
//#ifdef PRINT_DEBUG
//                debugln("   Calling Geas");
//                const auto start_time = clock();
//#endif
//                cp_result = cp_.solve(limits{.time = time_remaining, .conflicts = 0});
//#ifdef PRINT_DEBUG
//                debugln("   Geas run time = {:.3f}",
//                        static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
//#endif
//            }
//        }
//
//        // If satisfied, make assumptions on all MIP variables.
//        if (cp_result == geas::solver::SAT)
//        {
//            // Make additional assumptions.
//            debugln("   Assumptions:");
//            cp_.clear_assumptions();
//            if (!make_bool_assumptions(mip_, sol, probdata_, cp_) ||
//                !make_int_assumptions(mip_, sol, probdata_, cp_))
//            {
//                debugln("   Assumptions infeasible");
//                goto GET_CONFLICT;
//            }
//            debugln("   Assumptions completed");
//
//            // Get time remaining.
//            time_remaining = get_time_remaining();
//            if (time_remaining <= 0)
//            {
//                status_ = obj_ < Infinity ? Status::Feasible : Status::Unknown;
//                break;
//            }
//            else if (time_remaining == Infinity)
//            {
//                time_remaining = 0;
//            }
//
//            // Solve.
//            {
//#ifdef PRINT_DEBUG
//                debugln("   Calling Geas");
//                const auto start_time = clock();
//#endif
//                cp_result = cp_.solve(limits{.time = time_remaining, .conflicts = 0});
//#ifdef PRINT_DEBUG
//                debugln("   Geas run time = {:.3f}",
//                        static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
//#endif
//            }
//        }
//
//        // Get solution.
//        if (cp_result == geas::solver::SAT)
//        {
//            // Store objective value.
//            debug_assert(obj_ > probdata_.cp_int_vars_[obj_var.idx].lb(cp_.data));
//            obj_ = probdata_.cp_int_vars_[obj_var.idx].lb(cp_.data);
//            println("Found new solution with objective value {}", obj_);
//
//            // Check.
//            debug_assert(SCIPisIntegral(mip_, SCIPgetSolOrigObj(mip_, sol)));
//            debug_assert(SCIPisIntegral(mip_, SCIPgetPrimalbound(mip_)));
//            debug_assert(obj_ == SCIPround(mip_, SCIPgetSolOrigObj(mip_, sol)));
//            debug_assert(obj_ == SCIPround(mip_, SCIPgetPrimalbound(mip_)));
//
//            // Store solution.
//            for (Int idx = 0; idx < nb_bool_vars(); ++idx)
//            {
//                const auto& cp_var = probdata_.cp_bool_vars_[idx];
//                sol_.bool_vars_sol_[idx] = cp_var.lb(cp_.data->state.p_vals);
//            }
//            for (Int idx = 0; idx < nb_int_vars(); ++idx)
//            {
//                const auto& cp_var = probdata_.cp_int_vars_[idx];
//                sol_.int_vars_sol_[idx] = cp_var.lb(cp_.data);
//            }
//
//            // Optimal.
//            status_ = Status::Optimal;
//            break;
//        }
//        else if (cp_result == geas::solver::UNSAT)
//        {
//            GET_CONFLICT:
//
//            // Make nogood.
//            auto nogood = get_nogood(cp_, probdata_);
//
//            // Reset SCIP.
//            scip_assert(SCIPfreeTransform(mip_));
//
//            // If there is zero literals, the problem is infeasible.
//            if (nogood.vars.size() == 0)
//            {
//                status_ = Status::Infeasible;
//                break;
//            }
//
//            // If there is one literal, enforce the bound change globally.
//            if (nogood.vars.size() == 1)
//            {
//                // Change bound.
//                auto var = nogood.vars[0];
//                const auto sign = nogood.signs[0];
//                const auto bound = nogood.bounds[0];
//                if (sign == SCIP_BOUNDTYPE_UPPER)
//                {
//                    debug_assert(SCIPisLT(mip_, bound, SCIPvarGetUbGlobal(var)));
//                    scip_assert(SCIPchgVarUbGlobal(mip_, var, bound));
//                }
//                else
//                {
//                    debug_assert(SCIPisGT(mip_, bound, SCIPvarGetLbGlobal(var)));
//                    scip_assert(SCIPchgVarLbGlobal(mip_, var, bound));
//                }
//
//                // Solve again.
//                continue;
//            }
//
//            // Create cut.
//            if (nogood.all_binary)
//            {
//                // Get negated variables.
//                for (size_t idx = 0; idx < nogood.vars.size(); ++idx)
//                {
//                    debug_assert(SCIPvarIsBinary(nogood.vars[idx]));
//                    if (nogood.signs[idx] == SCIP_BOUNDTYPE_UPPER)
//                    {
//                        debug_assert(nogood.bounds[idx] == 0);
//                        scip_assert(SCIPgetNegatedVar(mip_,
//                                                      nogood.vars[idx],
//                                                      &nogood.vars[idx]));
//                    }
//                }
//
//                // Add constraint.
//                SCIP_CONS* cons = nullptr;
//                scip_assert(SCIPcreateConsBasicLogicor(mip_,
//                                                       &cons,
//#ifndef NDEBUG
//                                                       nogood.name.c_str(),
//#else
//                                                       "",
//#endif
//                                                       nogood.vars.size(),
//                                                       nogood.vars.data()));
//                debug_assert(cons);
//                scip_assert(SCIPaddCons(mip_, cons));
//                scip_assert(SCIPreleaseCons(mip_, &cons));
//                debugln("   Adding nogood with only binary variables");
//
//                // Solve again.
//                continue;
//            }
//            else
//            {
//                // Add constraint.
//                SCIP_CONS* cons = nullptr;
//                scip_assert(SCIPcreateConsBasicBounddisjunction(mip_,
//                                                                &cons,
//                                                                "",
//                                                                nogood.vars.size(),
//                                                                nogood.vars.data(),
//                                                                nogood.signs.data(),
//                                                                nogood.bounds.data()));
//                debug_assert(cons);
//                scip_assert(SCIPaddCons(mip_, cons));
//                scip_assert(SCIPreleaseCons(mip_, &cons));
//                debugln("   Adding nogood with integer variables");
//
//                // Solve again.
//                continue;
//            }
//        }
//        else
//        {
//            // Timed out.
//            status_ = obj_ < Infinity ? Status::Feasible : Status::Unknown;
//            break;
//        }
//    }
//
//    // Stop timer.
//    run_time_ = get_cpu_time();
//
//    // Print status.
//    EXIT:
//    println("");
//    println("--------------------------------------------------");
//    println("Method: LBBD");
//    println("CPU time: {:.2f} seconds", run_time_);
//    println("Status: {}",
//            status_ == Status::Unknown ? "Unknown" :
//            status_ == Status::Optimal ? "Optimal" :
//            status_ == Status::Feasible ? "Feasible" :
//            status_ == Status::Infeasible ? "Infeasible" :
//            "Error");
//    if (obj_ < Infinity)
//    {
//        println("Objective value: {:.2f}", obj_);
//    }
//    if (obj_bound_ > -Infinity && status_ != Status::Infeasible)
//    {
//        println("Objective bound: {:.2f}", obj_bound_);
//    }
//    println("--------------------------------------------------");
//
//    // Print solution.
//#ifdef PRINT_DEBUG
//    if (status_ == Status::Optimal || status_ == Status::Feasible)
//    {
//        println("");
//        println("Solution:");
//        for (Int idx = 0; idx < nb_int_vars(); ++idx)
//        {
//            println("   {} = {}",
//                    probdata_.int_vars_name_[idx], sol_.int_vars_sol_[idx]);
//        }
//        for (Int idx = 0; idx < nb_bool_vars(); ++idx)
//        {
//            println("   {} = {}",
//                    probdata_.bool_vars_name_[idx], sol_.bool_vars_sol_[idx]);
//        }
//    }
//#endif
}

}
