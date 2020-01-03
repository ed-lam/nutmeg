//#define PRINT_DEBUG

#include "ConstraintHandler-Geas.h"
#include "ProblemData.h"
#include "scip/clock.h"
#include "scip/cons_linear.h"
#include "scip/cons_logicor.h"
#include "scip/cons_bounddisjunction.h"

#define MAX_FRACTIONAL_CHECK_DURATION                  0.3
#define MAX_FRACTIONAL_CHECK_CONFLICTS                 300
#define MAX_CUT_MINIMIZATION_DURATION                  0.3
#define MAX_CUT_MINIMIZATION_CONFLICTS                 300

#define CONSHDLR_NAME                               "geas"
#define CONSHDLR_DESC                      "CP subproblem"
#ifdef CHECK_AT_LP
#define CONSHDLR_ENFOPRIORITY                            1 // priority of the constraint handler for constraint enforcing
#else
#define CONSHDLR_ENFOPRIORITY                     -4000000 // priority of the constraint handler for constraint enforcing
#endif
#define CONSHDLR_CHECKPRIORITY                    -4000000 // priority of the constraint handler for checking feasibility
#define CONSHDLR_PROPFREQ                                1 // frequency for propagating domains; zero means only preprocessing propagation
#define CONSHDLR_EAGERFREQ                               1 // frequency for using all instead of only the useful constraints in separation,
                                                           // propagation and enforcement, -1 for no eager evaluations, 0 for first only
#define CONSHDLR_DELAYPROP                           FALSE // should propagation method be delayed, if other propagators found reductions?
#define CONSHDLR_NEEDSCONS                            TRUE // should the constraint handler be skipped, if no constraints are available?

#define CONSHDLR_PROP_TIMING      SCIP_PROPTIMING_BEFORELP // when should the constraint be propagated

using namespace Nutmeg;

// Create a constraint
SCIP_RETCODE SCIPcreateConsBasicGeas(
    SCIP* scip,          // SCIP
    SCIP_CONS** cons,    // Pointer to hold the created constraint
    const char* name     // Name of constraint
)
{
    SCIP_CALL(SCIPcreateConsGeas(scip,
                                 cons,
                                 name,
                                 TRUE,
                                 TRUE,
                                 TRUE,
                                 TRUE,
                                 TRUE,
                                 FALSE,
                                 FALSE,
                                 FALSE,
                                 FALSE,
                                 FALSE));
    return SCIP_OKAY;
}

SCIP_RETCODE SCIPcreateConsGeas(
    SCIP* scip,                 // SCIP
    SCIP_CONS** cons,           // Pointer to hold the created constraint
    const char* name,           // Name of constraint
    SCIP_Bool initial,          // Should the LP relaxation of constraint be in the initial LP?
    SCIP_Bool separate,         // Should the constraint be separated during LP processing?
    SCIP_Bool enforce,          // Should the constraint be enforced during node processing?
    SCIP_Bool check,            // Should the constraint be checked for feasibility?
    SCIP_Bool propagate,        // Should the constraint be propagated during node processing?
    SCIP_Bool local,            // Is constraint only valid locally?
    SCIP_Bool modifiable,       // Is constraint modifiable (subject to column generation)?
    SCIP_Bool dynamic,          // Is constraint subject to aging?
    SCIP_Bool removable,        // Should the relaxation be removed from the LP due to aging or cleanup?
    SCIP_Bool stickingatnode    // Should the constraint always be kept at the node where it was added, even
                                // if it may be moved to a more global node?
)
{
    // Find constraint handler.
    auto conshdlr = SCIPfindConshdlr(scip, CONSHDLR_NAME);
    release_assert(conshdlr, "Constraint handler for Geas is not found");

    // Create constraint.
    SCIP_CALL(SCIPcreateCons(scip,
                             cons,
                             name,
                             conshdlr,
                             nullptr,
                             initial,
                             separate,
                             enforce,
                             check,
                             propagate,
                             local,
                             modifiable,
                             dynamic,
                             removable,
                             stickingatnode));

    // Done.
    return SCIP_OKAY;
}

#ifdef PRINT_DEBUG
static
void print_sol(
    SCIP* scip,              // SCIP
    SCIP_SOL* sol,           // Solution
    ProblemData& probdata    // Problem data
)
{
    debugln("   Solution:");
    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        auto mip_var = probdata.mip_bool_vars_[idx];
        const auto val = SCIPgetSolVal(scip, sol, mip_var);
        if (!SCIPisZero(scip, val))
        {
            debugln("      {} = {}", probdata.bool_vars_name_[idx], val);
        }
    }
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        auto mip_var = probdata.mip_int_vars_[idx];
        if (mip_var)
        {
            const auto val = SCIPgetSolVal(scip, sol, mip_var);
            if (!SCIPisZero(scip, val))
            {
                debugln("      {} = {}", probdata.int_vars_name_[idx], val);
            }
        }
    }
}
#endif

bool make_bool_assumptions(
    SCIP* scip,               // SCIP
    SCIP_SOL* sol,            // Solution
    ProblemData& probdata,    // Problem data
    geas::solver& cp          // CP solver
)
{
    // Make assumptions on Boolean variables.
    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        const auto mip_var = probdata.mip_bool_vars_[idx];
        debug_assert(mip_var);

        const auto val = SCIPgetSolVal(scip, sol, mip_var);
        if (SCIPisEQ(scip, val, 1.0))
        {
            debugln("      {} (bool var {})", probdata.bool_vars_name_[idx], idx);
            const auto& cp_var = probdata.cp_bool_vars_[idx];
            const auto success = cp.assume(cp_var);
            if (!success)
                return false;
        }
        else if (SCIPisZero(scip, val))
        {
            debugln("      ~{} (bool var {})", probdata.bool_vars_name_[idx], idx);
            const auto& cp_var = probdata.cp_bool_vars_[idx];
            const auto success = cp.assume(~cp_var);
            if (!success)
                return false;
        }
    }

    // Success.
    return true;
}

bool make_int_assumptions(
    SCIP* scip,               // SCIP
    SCIP_SOL* sol,            // Solution
    ProblemData& probdata,    // Problem data
    geas::solver& cp          // CP solver
)
{
    // Make assumptions on integer variables.
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        const auto mip_var = probdata.mip_int_vars_[idx];
        if (mip_var)
        {
            const auto val = SCIPgetSolVal(scip, sol, mip_var);
            Float val_up, val_down;
            if (SCIPisIntegral(scip, val))
            {
                val_up = SCIPround(scip, val);
                val_down = val_up;
            }
            else
            {
                val_up = std::ceil(val);
                val_down = std::floor(val);
            }
            debug_assert(val_down <= val_up);

            const auto& cp_var = probdata.cp_int_vars_[idx];
            {
                debugln("      [{} >= {}] (int var {})", probdata.int_vars_name_[idx], val_down, idx);
                const auto success = cp.assume(cp_var >= val_down);
                if (!success)
                    return false;
            }
            {
                debugln("      [{} <= {}] (int var {})", probdata.int_vars_name_[idx], val_up, idx);
                const auto success = cp.assume(cp_var <= val_up);
                if (!success)
                    return false;
            }
        }
    }

    // Success.
    return true;
}

bool make_obj_assumptions(
    SCIP* scip,               // SCIP
    SCIP_SOL* sol,            // Solution
    ProblemData& probdata,    // Problem data
    geas::solver& cp          // CP solver
)
{
    // Make assumptions on objective variable.
    const auto idx = probdata.obj_var_idx_;
    {
        const auto mip_var = probdata.mip_int_vars_[idx];
        debug_assert(mip_var);
        {
            const auto val = SCIPgetSolVal(scip, sol, mip_var);
            Float val_up, val_down;
            if (SCIPisIntegral(scip, val))
            {
                val_up = SCIPround(scip, val);
                val_down = val_up;
            }
            else
            {
                val_up = std::ceil(val);
                val_down = std::floor(val);
            }
            debug_assert(val_down <= val_up);

            const auto& cp_var = probdata.cp_int_vars_[idx];
            {
                debugln("      [{} >= {}] (int var {})", probdata.int_vars_name_[idx], val_down, idx);
                const auto success = cp.assume(cp_var >= val_down);
                if (!success)
                    return false;
            }
            {
                debugln("      [{} <= {}] (int var {})", probdata.int_vars_name_[idx], val_up, idx);
                const auto success = cp.assume(cp_var <= val_up);
                if (!success)
                    return false;
            }
        }
    }

    // Success.
    return true;
}

static
bool make_bounds_assumptions(
    SCIP* scip,               // SCIP
    ProblemData& probdata,    // Problem data
    geas::solver& cp          // CP solver
)
{
    // Make assumptions on Boolean variables.
    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        const auto mip_var = probdata.mip_bool_vars_[idx];
        debug_assert(mip_var);

        const auto lb = SCIPround(scip, SCIPvarGetLbLocal(mip_var));
        const auto ub = SCIPround(scip, SCIPvarGetUbLocal(mip_var));
        debug_assert(lb <= ub);

        if (SCIPisEQ(scip, lb, 1.0))
        {
            debugln("      {} (bool var {})", probdata.bool_vars_name_[idx], idx);
            const auto& cp_var = probdata.cp_bool_vars_[idx];
            const auto success = cp.assume(cp_var);
            if (!success)
                return false;
        }
        else if (SCIPisZero(scip, ub))
        {
            debugln("      ~{} (bool var {})", probdata.bool_vars_name_[idx], idx);
            const auto& cp_var = probdata.cp_bool_vars_[idx];
            const auto success = cp.assume(~cp_var);
            if (!success)
                return false;
        }
    }

    // Make assumptions on integer variables.
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        const auto mip_var = probdata.mip_int_vars_[idx];
        if (mip_var)
        {
            const auto lb = SCIPround(scip, SCIPvarGetLbLocal(mip_var));
            const auto ub = SCIPround(scip, SCIPvarGetUbLocal(mip_var));
            debug_assert(lb <= ub);

            const auto& cp_var = probdata.cp_int_vars_[idx];
            {
                debugln("      [{} >= {}] (int var {})",
                        probdata.int_vars_name_[idx], lb, idx);
                const auto success = cp.assume(cp_var >= lb);
                if (!success)
                    return false;
            }
            {
                debugln("      [{} <= {}] (int var {})",
                        probdata.int_vars_name_[idx], ub, idx);
                const auto success = cp.assume(cp_var <= ub);
                if (!success)
                    return false;
            }
        }
    }

    // Success.
    return true;
}

NogoodData get_nogood(
    geas::solver& cp,        // CP solver
    ProblemData& probdata    // Problem data
)
{
    // Create output data.
    NogoodData nogood;

    // Get conflict from CP solver.
    vec<geas::patom_t> conflict;
    cp.get_conflict(conflict);

    // Print.
#ifdef DEBUG
    nogood.name = make_nogood_name(probdata, conflict);
    debugln("   Nogood: {}", nogood.name);
#endif

    // Reduce number of terms in the nogood.
#ifdef USE_CUT_MINIMIZATION
    minimize_cut(cp, probdata, conflict);
#if defined(DEBUG)
    nogood.name = make_nogood_name(probdata, conflict);
    debugln("   Reduced nogood: {}", nogood.name);
#endif
#endif

    // Get the literals of the nogood.
    for (const auto atom : conflict)
    {
        // Add literals from binary variables.
        for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
        {
            auto cp_var = probdata.cp_bool_vars_[idx];
            if (atom == cp_var)
            {
                auto mip_var = probdata.mip_bool_vars_[idx];
                nogood.vars.push_back(mip_var);
                nogood.signs.push_back(SCIP_BOUNDTYPE_LOWER);
                nogood.bounds.push_back(1);

                goto NEXT_LITERAL;
            }
            else if (atom == ~cp_var)
            {
                auto mip_var = probdata.mip_bool_vars_[idx];
                nogood.vars.push_back(mip_var);
                nogood.signs.push_back(SCIP_BOUNDTYPE_UPPER);
                nogood.bounds.push_back(0);

                goto NEXT_LITERAL;
            }
        }

        // Add literals from integer variables.
        for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
        {
            auto cp_var = probdata.cp_int_vars_[idx];
            if (atom.pid == cp_var.p)
            {
                // [int_var >= val] literals.
                const auto val = cp_var.lb_of_pval(atom.val);
                if (probdata.mip_int_vars_[idx])
                {
                    auto mip_var = probdata.mip_int_vars_[idx];
                    nogood.vars.push_back(mip_var);
                    nogood.signs.push_back(SCIP_BOUNDTYPE_LOWER);
                    nogood.bounds.push_back(val);

                    nogood.all_binary = false;
                    goto NEXT_LITERAL;
                }
                else if (const auto& ind_vars = probdata.mip_indicator_vars_idx_[idx]; !ind_vars.empty())
                {
                    const auto lb = probdata.int_vars_lb_[idx];
                    const auto size = static_cast<Int>(ind_vars.size());
                    for (Int val_idx = val - lb; val_idx < size; ++val_idx)
                    {
                        auto mip_var = probdata.mip_bool_vars_[ind_vars[val_idx]];
                        if (mip_var)
                        {
                            nogood.vars.push_back(mip_var);
                            nogood.signs.push_back(SCIP_BOUNDTYPE_LOWER);
                            nogood.bounds.push_back(1);
                        }
                    }
                    goto NEXT_LITERAL;
                }
            }
            else if ((~atom).pid == cp_var.p)
            {
                // [int_var <= val] literals.
                const auto val = cp_var.ub_of_pval(atom.val);
                if (probdata.mip_int_vars_[idx])
                {
                    auto mip_var = probdata.mip_int_vars_[idx];
                    nogood.vars.push_back(mip_var);
                    nogood.signs.push_back(SCIP_BOUNDTYPE_UPPER);
                    nogood.bounds.push_back(val);

                    nogood.all_binary = false;
                    goto NEXT_LITERAL;
                }
                else if (const auto& ind_vars = probdata.mip_indicator_vars_idx_[idx]; !ind_vars.empty())
                {
                    const auto lb = probdata.int_vars_lb_[idx];
                    for (Int val_idx = 0; val_idx <= val - lb; ++val_idx)
                    {
                        auto mip_var = probdata.mip_bool_vars_[ind_vars[val_idx]];
                        if (mip_var)
                        {
                            nogood.vars.push_back(mip_var);
                            nogood.signs.push_back(SCIP_BOUNDTYPE_LOWER);
                            nogood.bounds.push_back(1);
                        }
                    }
                    goto NEXT_LITERAL;
                }
            }
        }

        // Literal not found.
        err("Lost literal while retrieving nogood");

        // Next iteration.
        NEXT_LITERAL:;
    }

    // Done.
    return nogood;
}

static inline
Float get_time_remaining(
    SCIP* scip    // SCIP
)
{
    SCIP_Real time_limit;
    scip_assert(SCIPgetRealParam(scip, "limits/time", &time_limit));
    const auto current_time = SCIPgetSolvingTime(scip);
    const auto time_remaining = time_limit - current_time;
    return time_remaining;
}

static
void store_cp_solution(
    ProblemData& probdata,    // Problem data
    geas::solver& cp          // CP solver
)
{
    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        const auto& cp_var = probdata.cp_bool_vars_[idx];
        probdata.sol_.bool_vars_sol_[idx] = cp_var.lb(cp.data->state.p_vals);
    }
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        const auto& cp_var = probdata.cp_int_vars_[idx];
        probdata.sol_.int_vars_sol_[idx] = cp_var.lb(cp.data);
    }
}

#ifdef CHECK_AT_LP
static
void inject_solution(
    SCIP* scip,              // SCIP
    SCIP_SOL* sol,           // Existing solution
    ProblemData& probdata    // Problem data
)
{
    // Create empty solution.
    SCIP_SOL* new_sol;
    scip_assert(SCIPcreateSol(scip,
                              &new_sol,
                              sol ? SCIPsolGetHeur(sol) : nullptr));

    // Store values in the solution.
    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        auto mip_var = probdata.mip_bool_vars_[idx];
        if (mip_var)
        {
            debug_assert(SCIPisIntegral(scip,
                                        probdata.sol_.bool_vars_sol_[idx]));
            debugln("   {} = {} in new primal solution",
                    probdata.bool_vars_name_[idx],
                    probdata.sol_.bool_vars_sol_[idx]);
            scip_assert(SCIPsetSolVal(scip,
                                      new_sol,
                                      mip_var,
                                      probdata.sol_.bool_vars_sol_[idx]));
        }
    }
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        auto mip_var = probdata.mip_int_vars_[idx];
        if (mip_var)
        {
            debug_assert(SCIPisIntegral(scip,
                                        probdata.sol_.int_vars_sol_[idx]));
            debugln("   {} = {} in new primal solution",
                    probdata.int_vars_name_[idx],
                    probdata.sol_.int_vars_sol_[idx]);
            scip_assert(SCIPsetSolVal(scip,
                                      new_sol,
                                      mip_var,
                                      probdata.sol_.int_vars_sol_[idx]));
        }
    }

    // Inject solution.
    SCIP_Bool stored = FALSE;
    scip_assert(SCIPaddSolFree(scip, &new_sol, &stored));
    release_assert(stored);
}
#endif

static
bool sol_is_fractional(
    SCIP* scip,              // SCIP
    SCIP_SOL* sol,           // Solution
    ProblemData& probdata    // Problem data
)
{
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        const auto mip_var = probdata.mip_int_vars_[idx];
        if (mip_var)
        {
            const auto val = SCIPgetSolVal(scip, sol, mip_var);
            if (!SCIPisIntegral(scip, val))
                return true;
        }
    }

    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        const auto mip_var = probdata.mip_bool_vars_[idx];
        debug_assert(mip_var);

        const auto val = SCIPgetSolVal(scip, sol, mip_var);
        if (!SCIPisIntegral(scip, val))
            return true;
    }

    // Solution is integral.
    return false;
}

static
void store_solution(
    SCIP* scip,               // SCIP
    SCIP_SOL* sol,            // Existing solution
    ProblemData& probdata,    // Problem data
    geas::solver& cp          // CP solver
)
{
    // Check.
    auto& prob_sol = probdata.sol_;
    debug_assert(probdata.cp_bool_vars_.size() == prob_sol.bool_vars_sol_.size());
    debug_assert(probdata.cp_int_vars_.size() == prob_sol.int_vars_sol_.size());

    // Store the solution if it is better than the incumbent.
    const auto obj_var_idx = probdata.obj_var_idx_;
    const auto new_obj = probdata.cp_int_vars_[obj_var_idx].lb(cp.data);
    const auto incumbent_obj = prob_sol.int_vars_sol_[obj_var_idx];
    if (new_obj < incumbent_obj)
    {
        // Store CP solution.
        store_cp_solution(probdata, cp);

        // Store new solution as a primal heuristic.
#ifdef CHECK_AT_LP
        if (sol_is_fractional(scip, sol, probdata))
        {
            inject_solution(scip, sol, probdata);
        }
#endif
    }
}

#ifdef DEBUG
String make_atom_name(
    ProblemData& probdata,    // Problem data
    geas::patom_t atom        // Atom
)
{
    // Get name of integer variables.
    String name;
    for (Int idx = 0; idx < probdata.nb_int_vars(); ++idx)
    {
        const auto& cp_var = probdata.cp_int_vars_[idx];
        const auto mip_var = probdata.mip_int_vars_[idx];
        if (atom.pid == cp_var.p)
        {
            // [int_var >= val] literals.
            const auto val = cp_var.lb_of_pval(atom.val);
            if (mip_var)
            {
                name = fmt::format("[{} >= {}]", probdata.int_vars_name_[idx], val);
                return name;
            }
            else if (const auto& ind_vars = probdata.mip_indicator_vars_idx_[idx]; !ind_vars.empty())
            {
                const auto lb = probdata.int_vars_lb_[idx];
                const auto size = static_cast<Int>(ind_vars.size());
                for (Int v = val - lb; v < size; ++v)
                {
                    auto mip_var = ind_vars[v];
                    if (name.empty())
                    {
                        name += fmt::format("({}", probdata.bool_vars_name_[mip_var]);
                    }
                    else
                    {
                        name += fmt::format(" \\/ {}", probdata.bool_vars_name_[mip_var]);
                    }
                }
                name += ")";
                return name;
            }
        }
        else if ((~atom).pid == cp_var.p)
        {
            // [int_var <= val] literals.
            const auto val = cp_var.ub_of_pval(atom.val);
            if (mip_var)
            {
                name = fmt::format("[{} <= {}]", probdata.int_vars_name_[idx], val);
                return name;
            }
            else if (const auto& ind_vars = probdata.mip_indicator_vars_idx_[idx]; !ind_vars.empty())
            {
                const auto lb = probdata.int_vars_lb_[idx];
                for (Int v = 0; v <= val - lb; ++v)
                {
                    auto mip_var = ind_vars[v];
                    if (name.empty())
                    {
                        name += fmt::format("({}", probdata.bool_vars_name_[mip_var]);
                    }
                    else
                    {
                        name += fmt::format(" \\/ {}", probdata.bool_vars_name_[mip_var]);
                    }
                }
                name += ")";
                return name;
            }
        }
    }

    // Get name of binary variables.
    for (Int idx = 0; idx < probdata.nb_bool_vars(); ++idx)
    {
        const auto& cp_var = probdata.cp_bool_vars_[idx];
        if (atom == cp_var)
        {
            return fmt::format("{}", probdata.bool_vars_name_[idx]);
        }
        else if (atom == ~cp_var)
        {
            return fmt::format("~{}", probdata.bool_vars_name_[idx]);
        }
    }

    // Failed to find its name.
    err("Lost literal while retrieving nogood");
}

String make_nogood_name(
    ProblemData& probdata,          // Problem data
    vec<geas::patom_t>& conflict    // Nogood
)
{
    if (conflict.size() == 0)
    {
        return String("Empty nogood");
    }
    else
    {
        String name = make_atom_name(probdata, conflict[0]);
        for (Int idx = 1; idx < conflict.size(); ++idx)
        {
            name += " \\/ " + make_atom_name(probdata, conflict[idx]);
        }
        return name;
    }
}
#endif

#ifdef USE_CUT_MINIMIZATION
void minimize_cut(
    geas::solver& cp,               // CP solver
    ProblemData& probdata,          // Problem data
    vec<geas::patom_t>& conflict    // Nogood
)
{
    // Make space to store result.
    geas::solver::result cp_result;
#ifdef PRINT_DEBUG
    clock_t start_time;
#endif

    // Determine which atoms are Boolean variables.
    Vector<bool> atom_is_bool_var(conflict.size());
    for (Int idx = 0; idx < conflict.size(); ++idx)
    {
        const auto atom = conflict[idx];
        for (Int v = 0; v < probdata.nb_bool_vars(); ++v)
        {
            const auto& cp_var = probdata.cp_bool_vars_[v];
            if (atom == cp_var || atom == ~cp_var)
            {
                atom_is_bool_var[idx] = true;
                break;
            }
        }
    }

    // Try removing atoms of integer variables.
    for (Int idx = 0; idx < conflict.size() && conflict.size() >= 2; ++idx)
        if (!atom_is_bool_var[idx])
        {
            // Clear old assumptions.
            cp.clear_assumptions();

            // Make assumptions.
            for (Int j = 0; j < conflict.size(); ++j)
                if (j != idx)
                {
                    const auto success = cp.assume(~conflict[j]);
                    if (!success)
                        goto FAILED_INT;
                }

            // Solve.
#ifdef PRINT_DEBUG
            start_time = clock();
#endif
            cp_result = cp.solve(limits{.time = MAX_CUT_MINIMIZATION_DURATION,
                                        .conflicts = MAX_CUT_MINIMIZATION_CONFLICTS});
#ifdef PRINT_DEBUG
            debugln("      Cut minimization run time = {:.3f}",
                    static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
#endif

            // Remove the literal.
            if (cp_result == geas::solver::UNSAT)
            {
                // Remove the literal.
                FAILED_INT:
                conflict[idx] = conflict.last();
                conflict.pop();
                atom_is_bool_var[idx] = atom_is_bool_var.back();
//                atom_is_bool_var.pop_back(); // No need
                --idx;

                // Print.
#ifdef PRINT_DEBUG
                const auto name = make_nogood_name(probdata, conflict);
                debugln("      Minimized cut: {}", name);
#endif
            }
        }

    // Try removing atoms of Boolean variables.
    for (Int idx = 0; idx < conflict.size() && conflict.size() >= 2; ++idx)
        if (atom_is_bool_var[idx])
        {
            // Clear old assumptions.
            cp.clear_assumptions();

            // Make assumptions.
            for (Int j = 0; j < conflict.size(); ++j)
                if (j != idx)
                {
                    const auto success = cp.assume(~conflict[j]);
                    if (!success)
                        goto FAILED_BOOL;
                }

            // Solve.
#ifdef PRINT_DEBUG
            start_time = clock();
#endif
            cp_result = cp.solve(limits{.time = MAX_CUT_MINIMIZATION_DURATION,
                                        .conflicts = MAX_CUT_MINIMIZATION_CONFLICTS});
#ifdef PRINT_DEBUG
            debugln("      Cut minimization run time = {:.3f}",
                    static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
#endif

            // Remove the literal.
            if (cp_result == geas::solver::UNSAT)
            {
                // Remove the literal.
                FAILED_BOOL:
                conflict[idx] = conflict.last();
                conflict.pop();
                atom_is_bool_var[idx] = atom_is_bool_var.back();
//                atom_is_bool_var.pop_back(); // No need
                --idx;

                // Print.
#ifdef PRINT_DEBUG
                const auto name = make_nogood_name(probdata, conflict);
                debugln("      Minimized cut: {}", name);
#endif
            }
        }
}
#endif

// Check feasibility of a solution
static
SCIP_RETCODE geas_check(
    SCIP* scip,            // SCIP
    SCIP_SOL* sol,         // Solution
    SCIP_RESULT* result    // Pointer to store the result
)
{
    using namespace Nutmeg;

    // Print.
    debugln("Starting Geas checker on solution with obj {:.6f}:",
            SCIPgetSolOrigObj(scip, sol));

    // Get problem data.
    auto& probdata = *reinterpret_cast<ProblemData*>(SCIPgetProbData(scip));
    auto& cp = probdata.cp_;

    // Print solution.
#ifdef PRINT_DEBUG
    print_sol(scip, sol, probdata);
#endif

    // Allocate space to store the result.
    geas::solver::result cp_result;
    Float time_remaining;

    // Check objective value.
    {
        auto mip_var = probdata.mip_int_vars_[probdata.obj_var_idx_];
        const auto val = SCIPgetSolVal(scip, sol, mip_var);
        if (val < probdata.cp_dual_bound_)
        {
            *result = SCIP_INFEASIBLE;
            return SCIP_OKAY;
        }
    }

    // Make assumptions.
    debugln("   Assumptions:");
    cp.clear_assumptions();
    if (!make_bool_assumptions(scip, sol, probdata, cp) ||
        !make_int_assumptions(scip, sol, probdata, cp))
    {
        debugln("   Assumptions infeasible");
        *result = SCIP_INFEASIBLE;
        return SCIP_OKAY;
    }
    debugln("   Assumptions completed");

    // Get time remaining.
    time_remaining = get_time_remaining(scip);
    if (time_remaining <= 0)
    {
        debugln("   Timed out");
        *result = SCIP_INFEASIBLE;
        return SCIP_OKAY;
    }

    // Solve.
    {
#ifdef PRINT_DEBUG
        debugln("   Calling Geas");
        const auto start_time = clock();
#endif
        cp_result = cp.solve(limits{.time = time_remaining, .conflicts = 0});
#ifdef PRINT_DEBUG
        debugln("   Geas run time = {:.3f}",
                static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
#endif
    }

    // Store solution or report infeasible (or timed out).
    if (cp_result == geas::solver::SAT)
    {
        // Store solution.
        store_solution(scip, sol, probdata, cp);

        // Feasible.
        debugln("   Feasible");
        *result = SCIP_FEASIBLE;
    }
    else
    {
        debugln("   Infeasible or timed out");
        *result = SCIP_INFEASIBLE;
    }

    // Done.
    return SCIP_OKAY;
}

static
SCIP_RETCODE geas_separate(
    SCIP* scip,                       // SCIP
    Nutmeg::ProblemData& probdata,    // Problem data
    SCIP_RESULT* result               // Pointer to store the result
)
{
    using namespace Nutmeg;

    constexpr SCIP_SOL* sol = nullptr;

    // Print.
    debugln("Starting Geas MIP separator on solution with obj {:.6f} (LP {:.6f}) at node "
            "{}, depth {}:",
            SCIPgetSolOrigObj(scip, sol),
            SCIPgetLPObjval(scip),
            SCIPnodeGetNumber(SCIPgetCurrentNode(scip)),
            SCIPgetDepth(scip));

    // Get problem.
    auto& cp = probdata.cp_;
    const auto is_fractional = sol_is_fractional(scip, nullptr, probdata);

    // Print solution.
#ifdef PRINT_DEBUG
    print_sol(scip, sol, probdata);
#endif

    // Allocate space to store the result.
    bool lp_early_stop = false;
    geas::solver::result cp_result;
    Float time_remaining;

    // Check CP subproblem only with assumptions on Boolean variables.
    {
        // Make assumptions.
        debugln("   Assumptions:");
        cp.clear_assumptions();
        if (!make_bool_assumptions(scip, sol, probdata, cp))
        {
            debugln("   Assumptions infeasible");
            goto GET_CONFLICT;
        }
        debugln("   Assumptions completed");

        // Get time remaining.
        time_remaining = get_time_remaining(scip);
        if (time_remaining <= 0)
        {
            debugln("   Timed out");
            *result = SCIP_INFEASIBLE;
            return SCIP_OKAY;
        }

        // If at a fractional solution, limit the maximum time the CP subproblem can run.
        if (is_fractional && time_remaining > MAX_FRACTIONAL_CHECK_DURATION)
        {
            time_remaining = MAX_FRACTIONAL_CHECK_DURATION;
            lp_early_stop = true;
        }

        // Solve.
        {
#ifdef PRINT_DEBUG
            debugln("   Calling Geas");
            const auto start_time = clock();
#endif
            cp_result = cp.solve(
                limits{.time = time_remaining,
                       .conflicts = is_fractional ? MAX_FRACTIONAL_CHECK_CONFLICTS : 0});
#ifdef PRINT_DEBUG
            debugln("   Geas run time = {:.3f}",
                    static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
#endif
        }
    }

    // If satisfied, also make assumptions on objective variable.
    if (cp_result == geas::solver::SAT)
    {
        // Make additional assumptions.
        debugln("   Assumptions:");
        cp.clear_assumptions();
        if (!make_bool_assumptions(scip, sol, probdata, cp) ||
            !make_obj_assumptions(scip, sol, probdata, cp))
        {
            debugln("   Assumptions infeasible");
            goto GET_CONFLICT;
        }
        debugln("   Assumptions completed");

        // Get time remaining.
        time_remaining = get_time_remaining(scip);
        if (time_remaining <= 0)
        {
            debugln("   Timed out");
            *result = SCIP_INFEASIBLE;
            return SCIP_OKAY;
        }

        // If at a fractional solution, limit the maximum time the CP subproblem can run.
        if (is_fractional && time_remaining > MAX_FRACTIONAL_CHECK_DURATION)
        {
            time_remaining = MAX_FRACTIONAL_CHECK_DURATION;
            lp_early_stop = true;
        }

        // Solve.
        {
#ifdef PRINT_DEBUG
            debugln("   Calling Geas");
            const auto start_time = clock();
#endif
            cp_result = cp.solve(
                limits{.time = time_remaining,
                       .conflicts = is_fractional ? MAX_FRACTIONAL_CHECK_CONFLICTS : 0});
#ifdef PRINT_DEBUG
            debugln("   Geas run time = {:.3f}",
                    static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
#endif
        }
    }

    // If satisfied, make assumptions on all MIP variables.
    if (cp_result == geas::solver::SAT)
    {
        // Make additional assumptions.
        debugln("   Assumptions:");
        cp.clear_assumptions();
        if (!make_bool_assumptions(scip, sol, probdata, cp) ||
            !make_int_assumptions(scip, sol, probdata, cp))
        {
            debugln("   Assumptions infeasible");
            goto GET_CONFLICT;
        }
        debugln("   Assumptions completed");

        // Get time remaining.
        time_remaining = get_time_remaining(scip);
        if (time_remaining <= 0)
        {
            debugln("   Timed out");
            *result = SCIP_INFEASIBLE;
            return SCIP_OKAY;
        }

        // If at a fractional solution, limit the maximum time the CP subproblem can run.
        if (is_fractional && time_remaining > MAX_FRACTIONAL_CHECK_DURATION)
        {
            time_remaining = MAX_FRACTIONAL_CHECK_DURATION;
            lp_early_stop = true;
        }

        // Solve.
        {
#ifdef PRINT_DEBUG
            debugln("   Calling Geas");
            const auto start_time = clock();
#endif
            cp_result = cp.solve(
                limits{.time = time_remaining,
                       .conflicts = is_fractional ? MAX_FRACTIONAL_CHECK_CONFLICTS : 0});
#ifdef PRINT_DEBUG
            debugln("   Geas run time = {:.3f}",
                    static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC);
#endif
        }
    }

    // Create nogood.
    if (cp_result == geas::solver::UNSAT)
    {
        GET_CONFLICT:

        // Make nogood.
        auto nogood = get_nogood(cp, probdata);

        // If there is zero literals, the problem is infeasible.
        if (nogood.vars.size() == 0)
        {
            scip_assert(SCIPinterruptSolve(scip));
            *result = SCIP_CUTOFF;
            return SCIP_OKAY;
        }

        // If there is one literal, enforce the bound change globally.
        if (nogood.vars.size() == 1)
        {
            // Change bound.
            auto var = nogood.vars[0];
            const auto sign = nogood.signs[0];
            const auto bound = nogood.bounds[0];
            if (sign == SCIP_BOUNDTYPE_UPPER)
            {
                debug_assert(SCIPisLT(scip, bound, SCIPvarGetUbLocal(var)));
                scip_assert(SCIPchgVarUbGlobal(scip, var, bound));
            }
            else
            {
                debug_assert(SCIPisGT(scip, bound, SCIPvarGetLbLocal(var)));
                if (var == probdata.mip_int_vars_[probdata.obj_var_idx_])
                {
                    if (SCIPisGT(scip, bound, SCIPvarGetUbLocal(var)))
                    {
                        probdata.cp_dual_bound_ = bound;
                        *result = SCIP_CUTOFF;
                        return SCIP_OKAY;
                    }
                    else
                    {
                        scip_assert(SCIPchgVarLbGlobal(scip, var, bound));
                    }
                }
                else
                {
                    scip_assert(SCIPchgVarLbGlobal(scip, var, bound));
                }
            }

            // Reduced domain.
            *result = SCIP_REDUCEDDOM;
            return SCIP_OKAY;
        }

        // Create cut.
        if (nogood.all_binary)
        {
            // Get negated variables.
            for (size_t idx = 0; idx < nogood.vars.size(); ++idx)
            {
                debug_assert(SCIPvarIsBinary(nogood.vars[idx]));
                if (nogood.signs[idx] == SCIP_BOUNDTYPE_UPPER)
                {
                    debug_assert(nogood.bounds[idx] == 0);
                    scip_assert(SCIPgetNegatedVar(scip,
                                                  nogood.vars[idx],
                                                  &nogood.vars[idx]));
                }
            }

            // Add constraint.
            SCIP_CONS* cons = nullptr;
            scip_assert(SCIPcreateConsBasicLogicor(scip,
                                                   &cons,
#ifdef DEBUG
                                                   nogood.name.c_str(),
#else
                                                   "",
#endif
                                                   nogood.vars.size(),
                                                   nogood.vars.data()));
            debug_assert(cons);
            scip_assert(SCIPaddCons(scip, cons));
            scip_assert(SCIPreleaseCons(scip, &cons));
            debugln("   Adding nogood with only binary variables");

            // Created constraint.
            *result = SCIP_CONSADDED;
            return SCIP_OKAY;
        }
        else
        {
            // Add constraint.
            SCIP_CONS* cons = nullptr;
            scip_assert(SCIPcreateConsBasicBounddisjunction(scip,
                                                            &cons,
#ifdef DEBUG
                                                            nogood.name.c_str(),
#else
                                                            "",
#endif
                                                            nogood.vars.size(),
                                                            nogood.vars.data(),
                                                            nogood.signs.data(),
                                                            nogood.bounds.data()));
            debug_assert(cons);
            scip_assert(SCIPaddCons(scip, cons));
            scip_assert(SCIPreleaseCons(scip, &cons));
            debugln("   Adding nogood with integer variables");

            // Created constraint.
            *result = SCIP_INFEASIBLE; // Stuck in infinite loop if returning CONSADDED
            return SCIP_OKAY;
        }
    }
    else if (cp_result == geas::solver::SAT)
    {
        // Store solution.
        store_solution(scip, sol, probdata, cp);

        // Feasible.
        debugln("   Feasible");
        *result = SCIP_FEASIBLE;
    }
    else
    {
        debug_assert(cp_result == geas::solver::UNKNOWN);
        if (lp_early_stop)
        {
            // Skipped checking LP solution by time out.
            debugln("   Skipped checking LP solution");
            *result = SCIP_FEASIBLE;
        }
        else
        {
            // Timed out.
            debugln("   Timed out");
            *result = SCIP_INFEASIBLE;
        }
    }

    // Done.
    return SCIP_OKAY;
}

// Propagation of a solution
static
SCIP_RETCODE geas_propagate(
    SCIP* scip,            // SCIP
    SCIP_RESULT* result    // Pointer to store the result
)
{
    using namespace Nutmeg;

    // Print.
    debugln("Starting Geas propagator on solution");

    // Get problem data.
    auto& probdata = *reinterpret_cast<ProblemData*>(SCIPgetProbData(scip));
    auto& cp = probdata.cp_;
    auto& bool_vars_monitor = probdata.bool_vars_monitor_;
    auto& int_vars_monitor = probdata.int_vars_monitor_;

    // Make assumptions.
    bool_vars_monitor.reset();
    int_vars_monitor.reset();
    debugln("   Assumptions:");
    cp.clear_assumptions();
    if (!make_bounds_assumptions(scip, probdata, cp))
    {
        debugln("   Assumptions infeasible");
        *result = SCIP_CUTOFF;
        return SCIP_OKAY;
    }
    debugln("   Assumptions completed");

    // Propagate.
    if (!cp.is_consistent())
    {
        debugln("   Propagation infeasible");
        *result = SCIP_CUTOFF;
        return SCIP_OKAY;
    }

    // Propagate CP domain changes in the MIP.
    debugln("   Propagating");
    for (auto idx : bool_vars_monitor.updated_lbs())
    {
        // Get variable corresponding to the bounds change.
        auto& cp_var = probdata.cp_bool_vars_[idx];
        auto mip_var = probdata.mip_bool_vars_[idx];

        // Make the bounds change in the MIP subproblem.
        const auto bound = cp_var.lb(cp.data->state.p_vals);
        if (SCIPisGT(scip, bound, SCIPvarGetLbLocal(mip_var)))
        {
            debugln("   {} >= {}", probdata.bool_vars_name_[idx], bound);
            scip_assert(SCIPchgVarLb(scip, mip_var, bound));
            *result = SCIP_REDUCEDDOM;
        }
    }
    for (auto idx : bool_vars_monitor.updated_ubs())
    {
        // Get variable corresponding to the bounds change.
        auto& cp_var = probdata.cp_bool_vars_[idx];
        auto mip_var = probdata.mip_bool_vars_[idx];

        // Make the bounds change in the MIP subproblem.
        const auto bound = cp_var.ub(cp.data->state.p_vals);
        if (SCIPisLT(scip, bound, SCIPvarGetUbLocal(mip_var)))
        {
            debugln("   {} <= {}", probdata.bool_vars_name_[idx], bound);
            scip_assert(SCIPchgVarUb(scip, mip_var, bound));
            *result = SCIP_REDUCEDDOM;
        }
    }
    for (auto idx : int_vars_monitor.updated_lbs())
    {
        // Get variable corresponding to the bounds change.
        auto& cp_var = probdata.cp_int_vars_[idx];
        auto mip_var = probdata.mip_int_vars_[idx];

        // Make the bounds change in the MIP subproblem.
        const auto bound = cp_var.lb(cp.data);
        if (SCIPisGT(scip, bound, SCIPvarGetLbLocal(mip_var)))
        {
            debugln("   {} >= {}", probdata.int_vars_name_[idx], bound);
            scip_assert(SCIPchgVarLb(scip, mip_var, bound));
            *result = SCIP_REDUCEDDOM;
        }
    }
    for (auto idx : int_vars_monitor.updated_ubs())
    {
        // Get variable corresponding to the bounds change.
        auto& cp_var = probdata.cp_int_vars_[idx];
        auto mip_var = probdata.mip_int_vars_[idx];

        // Make the bounds change in the MIP subproblem.
        const auto bound = cp_var.ub(cp.data);
        if (SCIPisLT(scip, bound, SCIPvarGetUbLocal(mip_var)))
        {
            debugln("   {} <= {}", probdata.int_vars_name_[idx], bound);
            scip_assert(SCIPchgVarUb(scip, mip_var, bound));
            *result = SCIP_REDUCEDDOM;
        }
    }

    // Done.
    return SCIP_OKAY;
}

// Copy method for constraint handler
static
SCIP_DECL_CONSHDLRCOPY(conshdlrCopyGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(valid);

    // Include constraint handler.
    SCIP_CALL(SCIPincludeConshdlrGeas(scip));

    // Done.
    *valid = TRUE;
    return SCIP_OKAY;
}

// Transform constraint data into data belonging to the transformed problem
static
SCIP_DECL_CONSTRANS(consTransGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(sourcecons);
    debug_assert(targetcons);

    // Create constraint.
    char name[SCIP_MAXSTRLEN];
    SCIPsnprintf(name, SCIP_MAXSTRLEN, "t_%s", SCIPconsGetName(sourcecons));
    SCIP_CALL(SCIPcreateCons(scip,
                             targetcons,
                             name,
                             conshdlr,
                             nullptr,
                             SCIPconsIsInitial(sourcecons),
                             SCIPconsIsSeparated(sourcecons),
                             SCIPconsIsEnforced(sourcecons),
                             SCIPconsIsChecked(sourcecons),
                             SCIPconsIsPropagated(sourcecons),
                             SCIPconsIsLocal(sourcecons),
                             SCIPconsIsModifiable(sourcecons),
                             SCIPconsIsDynamic(sourcecons),
                             SCIPconsIsRemovable(sourcecons),
                             SCIPconsIsStickingAtNode(sourcecons)));

    // Done.
    return SCIP_OKAY;
}

// Feasibility check method of constraint handler for integral solutions
static
SCIP_DECL_CONSCHECK(consCheckGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(nconss == 0 || conss);
    debug_assert(result);

    // Start.
    *result = SCIP_FEASIBLE;

    // Start checker.
    debug_assert(sol);
    SCIP_CALL(geas_check(scip, sol, result));

    // Done.
    return SCIP_OKAY;
}

// Constraint enforcing method of constraint handler for LP solutions
static
SCIP_DECL_CONSENFOLP(consEnfolpGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(conss);
    debug_assert(result);

    // Start.
    *result = SCIP_FEASIBLE;

    // Start separator.
    auto& probdata = *reinterpret_cast<ProblemData*>(SCIPgetProbData(scip));
    SCIP_CALL(geas_separate(scip, probdata, result));

    // Done.
    return SCIP_OKAY;
}

// Constraint enforcing method of constraint handler for pseudo solutions
static
SCIP_DECL_CONSENFOPS(consEnfopsGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(nconss == 0 || conss);
    debug_assert(result);

    // Start.
    *result = SCIP_FEASIBLE;

    // Start checker.
    SCIP_CALL(geas_check(scip, nullptr, result));

    // Done.
    return SCIP_OKAY;
}

// Domain propagation method of constraint handler
static
SCIP_DECL_CONSPROP(consPropGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(conss);
    debug_assert(result);

    // Start.
    *result = SCIP_DIDNOTFIND;

    // Start propagator.
    SCIP_CALL(geas_propagate(scip, result));

    // Done.
    return SCIP_OKAY;
}

// Variable rounding lock method of constraint handler
static
SCIP_DECL_CONSLOCK(consLockGeas)
{
    using namespace Nutmeg;

    // Check.
    debug_assert(scip);
    debug_assert(conshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    debug_assert(cons);

    // Get problem data.
    auto& probdata = *reinterpret_cast<ProblemData*>(SCIPgetProbData(scip));

    // Lock rounding of variables.
    for (auto var : probdata.mip_bool_vars_)
    {
        debug_assert(var);
        SCIP_CALL(SCIPaddVarLocks(scip,
                                  var,
                                  nlockspos + nlocksneg,
                                  nlockspos + nlocksneg));
    }
    for (auto var : probdata.mip_int_vars_)
        if (var)
        {
            SCIP_CALL(SCIPaddVarLocks(scip,
                                      var,
                                      nlockspos + nlocksneg,
                                      nlockspos + nlocksneg));
        }

    // Done.
    return SCIP_OKAY;
}

// Separation method of constraint handler for LP solutions
//static
//SCIP_DECL_CONSSEPALP(consSepalpGeas)
//{
//    // Check.
//    debug_assert(scip);
//    debug_assert(conshdlr);
//    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
//    debug_assert(conss);
//    debug_assert(result);
//
//    // Start.
//    *result = SCIP_DIDNOTFIND;
//
//    // Loop through all constraints.
//    for (Int c = 0; c < nconss; ++c)
//    {
//        // Get constraint.
//        auto cons = conss[c];
//        debug_assert(cons);
//
//        // Start separator.
//        SCIP_CALL(geas_separate(scip, cons, result));
//    }
//
//    // Done.
//    return SCIP_OKAY;
//}

// Separation method of constraint handler for arbitrary primal solutions
//static
//SCIP_DECL_CONSSEPASOL(consSepasolGeas)
//{
//    // Check.
//    debug_assert(scip);
//    debug_assert(conshdlr);
//    debug_assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
//    debug_assert(conss);
//    debug_assert(result);
//
//    // Start.
//    *result = SCIP_DIDNOTFIND;
//
//    // Loop through all constraints.
//    for (Int c = 0; c < nconss; ++c)
//    {
//        // Get constraint.
//        auto cons = conss[c];
//        debug_assert(cons);
//
//        // Start separator.
//        SCIP_CALL(geas_check(scip, cons, sol, result));
//    }
//
//    // Done.
//    return SCIP_OKAY;
//}

// Copying constraint of constraint handler
static
SCIP_DECL_CONSCOPY(consCopyGeas)
{
    // Check.
    debug_assert(scip);
    debug_assert(sourceconshdlr);
    debug_assert(strcmp(SCIPconshdlrGetName(sourceconshdlr), CONSHDLR_NAME) == 0);
    debug_assert(cons);
    debug_assert(sourcescip);
    debug_assert(sourcecons);
    debug_assert(varmap);

    // Stop if invalid.
    if (*valid)
    {
        // Create copied constraint.
        if (!name)
        {
            name = SCIPconsGetName(sourcecons);
        }
        SCIP_CALL(SCIPcreateConsGeas(scip,
                                     cons,
                                     name,
                                     initial,
                                     separate,
                                     enforce,
                                     check,
                                     propagate,
                                     local,
                                     modifiable,
                                     dynamic,
                                     removable,
                                     stickingatnode));
    }

    // Done.
    return SCIP_OKAY;
}

// Create the constraint handler
SCIP_RETCODE SCIPincludeConshdlrGeas(
    SCIP* scip    // SCIP
)
{
    // Include constraint handler.
    SCIP_CONSHDLR* conshdlr = nullptr;
    SCIP_CALL(SCIPincludeConshdlrBasic(scip,
                                       &conshdlr,
                                       CONSHDLR_NAME,
                                       CONSHDLR_DESC,
                                       CONSHDLR_ENFOPRIORITY,
                                       CONSHDLR_CHECKPRIORITY,
                                       CONSHDLR_EAGERFREQ,
                                       CONSHDLR_NEEDSCONS,
                                       consEnfolpGeas,
                                       consEnfopsGeas,
                                       consCheckGeas,
                                       consLockGeas,
                                       nullptr));
    debug_assert(conshdlr);

    // Set callbacks.
//    SCIP_CALL(SCIPsetConshdlrDelete(scip,
//                                    conshdlr,
//                                    consDeleteGeas));
//    SCIP_CALL(SCIPsetConshdlrExitsol(scip,
//                                     conshdlr,
//                                     consExitsolGeas));
    SCIP_CALL(SCIPsetConshdlrCopy(scip,
                                  conshdlr,
                                  conshdlrCopyGeas,
                                  consCopyGeas));
    SCIP_CALL(SCIPsetConshdlrTrans(scip,
                                   conshdlr,
                                   consTransGeas));
    SCIP_CALL(SCIPsetConshdlrProp(scip,
                                  conshdlr,
                                  consPropGeas,
                                  CONSHDLR_PROPFREQ,
                                  CONSHDLR_DELAYPROP,
                                  CONSHDLR_PROP_TIMING));

    // Done.
    return SCIP_OKAY;
}
