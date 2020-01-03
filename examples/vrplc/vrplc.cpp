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
    const auto L = instance.L;
    const auto R = instance.R;
    const auto N = R + 2;
    const auto& cost = instance.cost_matrix;

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
    Matrix<BoolVar> vars_x;
    Vector<IntVar> vars_start;
    Vector<IntVar> vars_capacity;

    // Calculate which edges are valid.
    Matrix<bool> is_valid(N, N, false);
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
        {
            is_valid(i, j) =
                !(i == 0 && j == R + 1) &&
                i != j &&
                instance.a[i] + instance.s[i] + cost(i, j) <= instance.b[j];
        }

    // Create cost variable.
    Int max_cost = 0;
    for (int i = 0; i <= R; ++i)
    {
        Int max_t_cost = 0;
        for (int j = 1; j <= R + 1; ++j)
            if (is_valid(i, j) && cost(i, j) > max_t_cost)
                max_t_cost = cost(i, j);
        max_cost += max_t_cost;
    }
    vars_cost = model.add_int_var(0, max_cost, true, "cost");

    // Create edge variables.
    vars_x.clear_and_resize(N, N);
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
            if (is_valid(i, j))
            {
                const auto name = fmt::format("x[{},{}]", i, j);
                vars_x(i, j) = model.add_bool_var(name);
            }

    // Create start time variables.
    vars_start.resize(N);
    for (int i = 0; i < N; ++i)
    {
        // Create MIP variable.
        const auto lb = instance.a[i];
        const auto ub = instance.b[i];
        release_assert(lb <= ub);
        const auto name = fmt::format("start[{}]", i);
        vars_start[i] = model.add_int_var(lb, ub, false, name);
    }

    // Create vehicle capacity variables.
    vars_capacity.resize(N);
    for (int i = 0; i < N; ++i)
    {
        // Create MIP variable.
        const auto lb = std::abs(instance.q[i]);
        const auto ub = (i == 0 ? 0 : instance.Q);
        release_assert(lb <= ub);
        const auto name = fmt::format("capacity[{}]", i);
        vars_capacity[i] =  model.add_int_var(lb, ub, false, name);
    }

    // Create objective function.
    {
        Vector<BoolVar> vars;
        Vector<Int> coeffs;
        for (int i = 0; i <= R; ++i)
            for (int j = 1; j <= R + 1; ++j)
                if (is_valid(i, j))
                {
                    vars.push_back(vars_x(i, j));
                    coeffs.push_back(cost(i, j));
                }
        model.add_constr_linear(vars, coeffs, Sign::EQ, 0, vars_cost);
    }

    // Create connectivity constraints.
    for (int i = 1; i <= R; ++i)
    {
        Vector<BoolVar> vars;
        for (int j = 1; j <= R + 1; ++j)
            if (is_valid(i, j))
                vars.push_back(vars_x(i, j));
        if (!vars.empty())
        {
            model.add_constr_set_partition(vars);
        }
    }
    for (int i = 1; i <= R; ++i)
    {
        Vector<BoolVar> vars;
        for (int h = 0; h <= R; ++h)
            if (is_valid(h, i))
                vars.push_back(vars_x(h, i));
        if (!vars.empty())
        {
            model.add_constr_set_partition(vars);
        }
    }

    // Create time successor constraints.
    // x[i,j] -> start[i] + s[i] + cost[i,j] <= start[j]
    // x[i,j] -> start[i] - start[j] <= -s[i] - cost[i,j])
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
            if (is_valid(i, j))
            {
                model.add_constr_reify_subtraction_leq(vars_x(i, j),
                                                       vars_start[i],
                                                       vars_start[j],
                                                       -instance.s[i] - cost(i, j));
            }

    // Create vehicle capacity successor constraints.
    // x[i,j] -> capacity[i] + q[j] <= capacity[j]
    // x[i,j] -> capacity[i] - capacity[j] <= -q[j]
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
            if (is_valid(i, j))
            {
                const auto q = std::abs(instance.q[j]);
                model.add_constr_reify_subtraction_leq(vars_x(i, j),
                                                       vars_capacity[i],
                                                       vars_capacity[j],
                                                       -q);
            }

    // Create scheduling constraints in CP.
    for (int l = 1; l <= L; ++l)
    {
        Vector<IntVar> loc_start;
        Vector<Int> loc_duration;
        Vector<Int> loc_resource;
        for (int i = 1; i <= R; ++i)
            if (instance.l[i] == l)
            {
                loc_start.push_back(vars_start[i]);
                loc_duration.push_back(instance.s[i]);
                loc_resource.push_back(1);
            }

        if (!loc_start.empty())
        {
            model.add_constr_cumulative(loc_start,
                                        loc_duration,
                                        loc_resource,
                                        instance.C);
        }
    }

    // Solve.
    model.minimize(vars_cost, time_limit);

    // Print solution.
    if (model.get_status() == Status::Feasible || model.get_status() == Status::Optimal)
    {
        println("   LB {}, UB {}", model.get_lb(), model.get_ub());

        for (int i = 0; i <= R; ++i)
            for (int j = 1; j <= R + 1; ++j)
                if (is_valid(i, j) && model.get_sol(vars_x(i, j)))
                {
                    println("      x[{},{}]", i, j);
                }

        for (int i = 1; i <= R; ++i)
        {
            const auto start = model.get_sol(vars_start[i]);
            const auto end = start + instance.s[i];

            println("      Start[{}] {}, End[{}] {}", i, start, i, end);
        }
    }

    // Done.
    return 0;
}
