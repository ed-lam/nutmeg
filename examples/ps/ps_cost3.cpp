#include "InstanceData.h"
#include "Nutmeg/Nutmeg.h"

int main(int argc, char** argv)
{
    // Read instance.
    release_assert(argc >= 2, "Path to instance must be second argument");
    const InstanceData instance(argv[1]);

    // Get time limit.
    const auto time_limit = argc >= 3 ? std::atof(argv[2]) : Infinity;

    // Get instance data.
    const auto T = instance.T;
    const auto M = instance.M;
    const auto& cost = instance.cost;
    const auto& duration = instance.duration;
    const auto& resource = instance.resource;
    const auto& release = instance.release;
    const auto& deadline = instance.deadline;
    const auto& capacity = instance.capacity;

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
    IntVar vars_cost;
    Vector<IntVar> vars_machine_of_job;
    Vector<Vector<BoolVar>> vars_job_machine_assignment;
    Matrix<IntVar> vars_start;

    // Calculate which assignments don't exceed the job duration.
    Matrix<bool> is_valid(T, M, false);
    for (int t = 0; t < T; ++t)
        for (int m = 0; m < M; ++m)
        {
            const auto lb = release[t];
            const auto ub = deadline[t] - duration(t, m);
            is_valid(t, m) = lb <= ub;
        }

    // Create cost variable.
    Int max_cost = 0;
    for (int t = 0; t < T; ++t)
    {
        Int max_t_cost = 0;
        for (int m = 0; m < M; ++m)
            if (is_valid(t, m) && cost(t, m) > max_t_cost)
                max_t_cost = cost(t, m);
        max_cost += max_t_cost;
    }
    vars_cost = model.add_int_var(0, max_cost, true, "cost");

    // Create assignment variables.
    vars_machine_of_job.resize(T);
    vars_job_machine_assignment.resize(T);
    for (int t = 0; t < T; ++t)
    {
        const auto name = fmt::format("assign[{}]", t);
        vars_machine_of_job[t] = model.add_int_var(0, M - 1, false, name);
        vars_job_machine_assignment[t] = model.add_indicator_vars(vars_machine_of_job[t]);

        for (int m = 0; m < M; ++m)
            if (!is_valid(t, m))
                model.add_constr_fix(~vars_job_machine_assignment[t][m]);
    }

    // Create start time variables.
    vars_start.clear_and_resize(T, M);
    for (int t = 0; t < T; ++t)
        for (int m = 0; m < M; ++m)
            if (is_valid(t, m))
            {
                const auto lb = release[t];
                const auto ub = deadline[t] - duration(t, m);
                const auto name = fmt::format("start[{},{}]", t, m);
                vars_start(t, m) = model.add_int_var(lb, ub, false, name);
            }

    // Create objective function.
    {
        Vector<Vector<Int>> val_coeffs(T);
        for (int t = 0; t < T; ++t)
        {
            val_coeffs[t].resize(M);

            for (int m = 0; m < M; ++m)
            {
                val_coeffs[t][m] = is_valid(t, m) ? cost(t, m) : 0;
            }
        }
        model.add_constr_linear(vars_machine_of_job,
                                val_coeffs,
                                Sign::EQ,
                                0,
                                vars_cost);
    }

    // Create scheduling constraints.
    for (int m = 0; m < M; ++m)
    {
        Vector<BoolVar> loc_active;
        Vector<IntVar> loc_start;
        Vector<Int> loc_duration;
        Vector<Int> loc_resource;
        for (int t = 0; t < T; ++t)
            if (is_valid(t, m))
            {
                loc_active.push_back(vars_job_machine_assignment[t][m]);
                loc_start.push_back(vars_start(t, m));
                loc_duration.push_back(duration(t, m));
                loc_resource.push_back(resource(t, m));
            }

        model.add_constr_cumulative_optional(loc_active,
                                             loc_start,
                                             loc_duration,
                                             loc_resource,
                                             capacity[m]);
    }

    // Solve.
    model.minimize(vars_cost, time_limit);

    // Print solution.
    if (model.get_status() == Status::Feasible || model.get_status() == Status::Optimal)
    {
        println("LB: {}", model.get_dual_bound());
        println("UB: {}", model.get_primal_bound());
        for (int t = 0; t < T; ++t)
            for (int m = 0; m < M; ++m)
                if (is_valid(t, m) && model.get_sol(vars_job_machine_assignment[t][m]))
                {
                    const auto start = model.get_sol(vars_start(t, m));
                    const auto end = start + duration(t, m);
                    println("Task {}: Machine {}, Start {}, Complete {}, Cost {}", t, m, start, end, cost(t, m));
                }
    }

    // Done.
    return 0;
}
