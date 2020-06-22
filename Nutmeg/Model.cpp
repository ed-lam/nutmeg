//#define PRINT_DEBUG

#include "Model.h"
#include "ConstraintHandler-Geas.h"
#include "EventHandler-NewSolution.h"
#include "scip/scipdefplugins.h"
#include "geas/vars/monitor.h"

namespace Nutmeg
{

// Create problem data for transformed problem
static
SCIP_RETCODE callback_probtrans(
    SCIP* scip,
    SCIP_PROBDATA* sourcedata,
    SCIP_PROBDATA** targetdata
)
{
    // Create transformed problem data.
    ProblemData* probdata = nullptr;
    scip_assert(SCIPallocBlockMemory(scip, &probdata));
    new(probdata) ProblemData(*reinterpret_cast<ProblemData*>(sourcedata));
    *targetdata = reinterpret_cast<SCIP_ProbData*>(probdata);

    // Transform CP constraint.
    if (probdata->cp_cons_)
    {
        scip_assert(SCIPtransformCons(scip, probdata->cp_cons_, &probdata->cp_cons_));
    }

    // Transform variables.
    for (Int idx = 0; idx < probdata->nb_bool_vars(); ++idx)
    {
        if (probdata->is_pos_var(idx))
        {
            auto& var = probdata->mip_bool_vars_[idx];
            scip_assert(SCIPtransformVar(scip, var, &var));
        }
        else
        {
            const auto pos_idx = probdata->mip_neg_vars_idx_[idx];
            debug_assert(pos_idx >= 0 && pos_idx < idx);
            scip_assert(SCIPgetNegatedVar(scip, probdata->mip_bool_vars_[pos_idx], &probdata->mip_bool_vars_[idx]));
        }
    }
    for (auto& var : probdata->mip_int_vars_)
        if (var)
        {
            if (std::strcmp(SCIPvarGetName(var), "false_neg") == 0)
            {
                debug_assert(probdata->bool_vars_name_[1] == "true");
                var = probdata->mip_bool_vars_[1];
            }
            else
            {
                scip_assert(SCIPtransformVar(scip, var, &var));
            }
        }

    // Done.
    return SCIP_OKAY;
}

// Free problem data of transformed problem
static
SCIP_RETCODE callback_probdeltrans(
    SCIP* scip,
    SCIP_PROBDATA** scip_probdata
)
{
    // Check.
    debug_assert(scip);

    // Get problem data.
    auto probdata = reinterpret_cast<ProblemData*>(*scip_probdata);

    // Release CP constraint handler.
    if (probdata->cp_cons_)
    {
        scip_assert(SCIPreleaseCons(scip, &probdata->cp_cons_));
    }

    // Release variables.
    for (Int idx = 0; idx < probdata->nb_bool_vars(); ++idx)
        if (probdata->is_pos_var(idx))
        {
            auto& var = probdata->mip_bool_vars_[idx];
            scip_assert(SCIPreleaseVar(scip, &var));
        }
    for (auto& var : probdata->mip_int_vars_)
        if (var)
        {
            scip_assert(SCIPreleaseVar(scip, &var));
        }

    // Destroy problem data.
    probdata->~ProblemData();
    SCIPfreeBlockMemory(scip, &probdata);

    // Done.
    return SCIP_OKAY;
}

Model::Model(const Method method) :
    method_(method),
    mip_(nullptr),
    cp_(),
    print_new_solution_function_(),

    probdata_(*this, cp_, sol_),
    status_(Status::Unknown),
    obj_(std::numeric_limits<Float>::quiet_NaN()),
    obj_bound_(std::numeric_limits<Float>::quiet_NaN()),
    sol_(),

    time_limit_(),
    start_time_(),
    run_time_(0)
{
    // Print.
#ifndef NDEBUG
    println("Nutmeg is compiled in debug mode");
#endif

    // Create SCIP.
    scip_assert(SCIPcreate(&mip_));

    // Include default SCIP plugins.
    scip_assert(SCIPincludeDefaultPlugins(mip_));

    // Disable parallel solve.
    scip_assert(SCIPsetIntParam(mip_, "parallel/maxnthreads", 1));
    scip_assert(SCIPsetIntParam(mip_, "lp/threads", 1));

    // Disable multiaggregate variables.
    scip_assert(SCIPsetBoolParam(mip_, "presolving/donotmultaggr", TRUE));

    // Disable restarts.
    scip_assert(SCIPsetIntParam(mip_, "presolving/maxrestarts", 0));

    // Create problem.
    scip_assert(SCIPcreateProbBasic(mip_, "Nutmeg"));

    // Set optimization direction.
    scip_assert(SCIPsetObjsense(mip_, SCIP_OBJSENSE_MINIMIZE));

    // Tell SCIP that the objective value will always be integral.
    scip_assert(SCIPsetObjIntegral(mip_));

    // Create problem data.
    scip_assert(SCIPsetProbData(mip_, reinterpret_cast<SCIP_ProbData*>(&probdata_)));

    // Create variable representing false.
    {
        // Create variable in MIP.
        SCIP_VAR*& mip_var = probdata_.mip_bool_vars_.emplace_back();
        scip_assert(SCIPcreateVarBasic(mip_,
                                       &mip_var,
                                       "false",
                                       0,
                                       0,
                                       0,
                                       SCIP_VARTYPE_BINARY));
        release_assert(mip_var, "Failed to create Boolean variable in MIP");
        scip_assert(SCIPaddVar(mip_, mip_var));
        probdata_.mip_neg_vars_idx_.emplace_back(1);

        // Create variable in CP.
        probdata_.cp_bool_vars_.push_back(geas::at_False);

        // Store variable name.
        probdata_.bool_vars_name_.emplace_back("false");
    }

    // Create variable representing true.
    {
        // Create variable in MIP.
        probdata_.mip_bool_vars_.emplace_back();
        scip_assert(SCIPgetNegatedVar(mip_,
                                      probdata_.mip_bool_vars_[0],
                                      &probdata_.mip_bool_vars_.back()));
        probdata_.mip_neg_vars_idx_.emplace_back(0);

        // Create variable in CP.
        probdata_.cp_bool_vars_.push_back(geas::at_True);

        // Store variable name.
        probdata_.bool_vars_name_.emplace_back("true");
    }

    // Create variable representing 0.
    {
        // Copy FALSE Boolean variable in MIP.
        SCIP_VAR*& mip_var = probdata_.mip_int_vars_.emplace_back(probdata_.mip_bool_vars_[0]);
        scip_assert(SCIPcaptureVar(mip_, mip_var));
        probdata_.mip_indicator_vars_idx_.emplace_back();

        // Create variable in CP.
        probdata_.cp_int_vars_.push_back(cp_.new_intvar(0, 0));

        // Store variable data.
        probdata_.int_vars_lb_.push_back(0);
        probdata_.int_vars_ub_.push_back(0);
        probdata_.int_vars_name_.emplace_back("0");

        // Set as objective variable.
        probdata_.obj_var_idx_ = 0;

        // Add to set of constants.
        probdata_.constants_.insert({0, IntVar(this, 0)});
    }

    // Create constraint handler for Geas.
    if (method_ == Method::BC)
    {
        scip_assert(SCIPincludeConshdlrGeas(mip_));
        scip_assert(SCIPcreateConsBasicGeas(mip_, &probdata_.cp_cons_, "Geas"));
        scip_assert(SCIPaddCons(mip_, probdata_.cp_cons_));
    }

    // Linearize linking constraints for binarized variables.
//    scip_assert(SCIPsetBoolParam(mip_, "constraints/linking/linearize", TRUE));

    // Add callbacks.
    scip_assert(SCIPsetProbTrans(mip_, callback_probtrans));
    scip_assert(SCIPsetProbDeltrans(mip_, callback_probdeltrans));
}

Model::~Model()
{
    // Free problem data.
    {
        // Release CP constraint handler.
        if (probdata_.cp_cons_)
        {
            scip_assert(SCIPreleaseCons(mip_, &probdata_.cp_cons_));
        }

        // Release variables.
        for (Int idx = 0; idx < probdata_.nb_bool_vars(); ++idx)
            if (probdata_.is_pos_var(idx))
            {
                auto& var = probdata_.mip_bool_vars_[idx];
                scip_assert(SCIPreleaseVar(mip_, &var));
            }
        for (auto& var : probdata_.mip_int_vars_)
            if (var)
            {
                scip_assert(SCIPreleaseVar(mip_, &var));
            }
    }

    // Destroy SCIP.
    scip_assert(SCIPfree(&mip_));

    // Check if memory is leaked.
    BMScheckEmptyMemory();
}

void Model::add_print_new_solution_function(std::function<void()> print_new_solution_function)
{
    if ((method_ == Method::BC || method_ == Method::MIP) && !print_new_solution_function_)
    {
        print_new_solution_function_ = print_new_solution_function;
        scip_assert(includeEventHdlrBestsol(mip_, this));
    }
    else
    {
        print_new_solution_function_ = print_new_solution_function;
    }
}

void Model::minimize(const IntVar obj_var, const Float time_limit, const bool verbose)
{
    if (method_ == Method::BC)
    {
        minimize_using_bc(obj_var, time_limit, verbose);
    }
    else if (method_ == Method::LBBD)
    {
        minimize_using_lbbd(obj_var, time_limit, verbose);
    }
    else if (method_ == Method::MIP)
    {
        minimize_using_mip(obj_var, time_limit, verbose);
    }
    else if (method_ == Method::CP)
    {
        minimize_using_cp(obj_var, time_limit, verbose);
    }
    else
    {
        err("Invalid method {}", static_cast<Int>(method_));
    }
}

Status Model::get_status() const
{
    return status_;
}

Float Model::get_runtime() const
{
    release_assert(!std::isnan(run_time_), "Cannot get runtime because the model is not solved");
    return run_time_;
}

Int Model::get_dual_bound() const
{
    release_assert(!std::isnan(obj_bound_), "Cannot get dual bound because the model is not solved");
    return obj_bound_;
}

Int Model::get_primal_bound() const
{
    release_assert(!std::isnan(obj_), "Cannot get primal bound because the model is not solved");
    release_assert(status_ == Status::Optimal || status_ == Status::Feasible,
                   "Cannot get primal bound because no feasible solution is available");
    return obj_;
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

void Model::start_timer(const Float time_limit)
{
    release_assert(time_limit > 0, "Time limit {} is invalid", time_limit);
    time_limit_ = time_limit;
    start_time_ = clock();
}

Float Model::get_cpu_time() const
{
    const auto current_time = clock();
    return static_cast<Float>(current_time - start_time_) / CLOCKS_PER_SEC;
}

Float Model::get_time_remaining() const
{
    return time_limit_ - get_cpu_time();
}

void Model::write_lp()
{
    scip_assert(SCIPwriteOrigProblem(mip_, "model.lp", 0, 0));
}

}
