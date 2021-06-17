#include "InstanceData.h"
#include "Nutmeg/Nutmeg.h"

using namespace Nutmeg;

int main(int argc, char** argv)
{
    // Read instance.
    release_assert(argc >= 2, "Path to instance must be second argument");
    const InstanceData instance(argv[1]);

    // Get time limit.
    const auto time_limit = argc >= 3 ? std::atof(argv[2]) : Infinity;

    // Get instance data.
    const auto time_horizon = instance.time_horizon;
    const auto num_resources = instance.num_resources;
    const auto& resource_availability = instance.resource_availability;
    const auto num_jobs = instance.num_jobs;
    const auto& job_duration = instance.job_duration;
    const auto& job_consumption = instance.job_consumption;
    const auto& job_duedate = instance.job_duedate;
    const auto& job_earliness_cost = instance.job_earliness_cost;
    const auto& job_tardiness_cost = instance.job_tardiness_cost;
    const auto& job_successors = instance.job_successors;

    // Create empty model.
#if defined(SOLVE_USING_CP)
    Model model(Method::CP);
#elif defined(SOLVE_USING_MIP)
    Model model(Method::MIP);
#elif defined(SOLVE_USING_LBBD)
    Model model(Method::LBBD);
#elif defined(SOLVE_USING_BC)
    Model model(Method::BC);
#else
    err("Unspecified solving method");
#endif

    // Create cost variable.
    Int max_cost = 0;
    for (int i = 0; i < num_jobs; ++i)
    {
        max_cost += std::max(job_earliness_cost[i] * job_duedate[i],
                             job_tardiness_cost[i] * (time_horizon - job_duedate[i]));
    }
    IntVar vars_cost = model.add_int_var(0, max_cost, true, "cost");

    // Create start time variables.
    Vector<IntVar> vars_start;
    vars_start.resize(num_jobs);
    for (int i = 0; i < num_jobs; ++i)
    {
        const auto lb = 0;
        const auto ub = time_horizon - job_duration[i] - 1;
        release_assert(lb <= ub);
        const auto name = fmt::format("start[{}]", i);
        vars_start[i] = model.add_int_var(lb, ub, true, name);
    }

    // Add precedence constraints.
    // start[i] + duration[i] <= start[j]
    for (int i = 0; i < num_jobs; ++i)
        for (const auto j : job_successors[i])
        {
            model.add_constr_subtraction_leq(vars_start[i], vars_start[j], -job_duration[i]);
        }

    // Add scheduling constraints.
    for (int r = 0; r < num_resources; ++r)
    {
        Vector<Int> consumption(job_consumption.begin(r), job_consumption.end(r));
        model.add_constr_cumulative(vars_start, job_duration, consumption, resource_availability[r], true);
    }

    // Create objective function.
    {
        Vector<BoolVar> vars;
        Vector<Int> coeffs;
        for (int i = 0; i < num_jobs; ++i)
        {
            auto vars_start_indicator = model.add_indicator_vars(vars_start[i]);

            for (int t = 0; t <= job_duedate[i] - 1; ++t)
            {
                vars.push_back(vars_start_indicator[t]);
                coeffs.push_back(job_earliness_cost[i] * (job_duedate[i] - t));
            }
            for (int t = job_duedate[i] + 1; t < time_horizon - job_duration[i]; ++t)
            {
                vars.push_back(vars_start_indicator[t]);
                coeffs.push_back(job_tardiness_cost[i] * (t - job_duedate[i]));
            }
        }
        model.add_constr_linear(vars, coeffs, Sign::EQ, 0, vars_cost);
    }

    // Solve.
    model.minimize(vars_cost, time_limit);

    // Print solution.
    if (model.get_status() == Status::Feasible || model.get_status() == Status::Optimal)
    {
        println("LB: {}", model.get_dual_bound());
        println("UB: {}", model.get_primal_bound());

        for (int i = 0; i < num_jobs; ++i)
        {
            println("Job {}: due date {}, start {}", i, job_duedate[i], model.get_sol(vars_start[i]));
        }
    }

    // Done.
    return 0;
}
