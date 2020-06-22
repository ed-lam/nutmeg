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
    const auto P = instance.P;
    const auto C = instance.C;
    const auto T = C;
    const auto& plant_capacity = instance.plant_capacity;
    const auto& plant_cost = instance.plant_cost;
    const auto& client_demand = instance.client_demand;
    const auto& allocation_cost = instance.allocation_cost;
    const auto& vehicle_cost = instance.vehicle_cost;
    const auto& max_distance = instance.max_distance;
    const auto& distance = instance.distance;

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
    for (int p = 0; p < P; ++p)
    {
        max_cost += plant_cost[p];
    }
    for (int c = 0; c < C; ++c)
    {
        Int max_c_cost = 0;
        for (int p = 0; p < P; ++p)
            if (allocation_cost(c, p) > max_c_cost)
                max_c_cost = allocation_cost(c, p);
        max_cost += max_c_cost;
    }
    max_cost += C * vehicle_cost;
    IntVar vars_cost = model.add_int_var(0, max_cost, true, "cost");

    // Create variables to indicate whether a plant is open.
    Vector<BoolVar> vars_plant_open(P);
    for (int p = 0; p < P; ++p)
    {
        const auto name = fmt::format("plant_open[{}]", p);
        vars_plant_open[p] = model.add_bool_var(name);
    }

    // Create variables for number of trucks at a plant.
    Vector<IntVar> vars_trucks_used_at_plant(P);
    Vector<Vector<BoolVar>> vars_plant_trucks_used_indicator(P);
    for (int p = 0; p < P; ++p)
    {
        const auto name = fmt::format("nb_trucks[{}]", p);
        vars_trucks_used_at_plant[p] = model.add_int_var(0, T, true, name);
        vars_plant_trucks_used_indicator[p] = model.add_indicator_vars(vars_trucks_used_at_plant[p]);
    }

    // Create variables for the plant assigned to a client.
    Vector<Vector<BoolVar>> vars_client_plant_indicator(C);
    for (int c = 0; c < C; ++c)
    {
        const auto name = fmt::format("client_plant_indicator[{}]", c);
        auto var = model.add_int_var(0, P - 1, false, name);
        vars_client_plant_indicator[c] = model.add_indicator_vars(var);
    }

    // Create truck number variables.
    Vector<IntVar> vars_truck_number_of_client(C);
    for (int c = 0; c < C; ++c)
    {
        const auto name = fmt::format("truck_number_of_client[{}]", c);
        vars_truck_number_of_client[c] = model.add_int_var(0, T - 1, true, name);
    }

    // Create objective function.
    {
        Vector<BoolVar> vars;
        Vector<Int> coeffs;

        for (int p = 0; p < P; ++p)
        {
            vars.push_back(vars_plant_open[p]);
            coeffs.push_back(plant_cost[p]);
        }

        for (int c = 0; c < C; ++c)
            for (int p = 0; p < P; ++p)
            {
                vars.push_back(vars_client_plant_indicator[c][p]);
                coeffs.push_back(allocation_cost(c, p));
            }

        for (int p = 0; p < P; ++p)
            for (int t = 1; t <= T; ++t)
            {
                vars.push_back(vars_plant_trucks_used_indicator[p][t]);
                coeffs.push_back(vehicle_cost * t);
            }

        model.add_constr_linear(vars, coeffs, Sign::EQ, 0, vars_cost);
    }

    // Create plant capacity constraint.
    // sum(c in C) (client_demand[c] * [plant_of_client[c] == p]) <= plant_capacity[p] * plant_open[p]
    for (int p = 0; p < P; ++p)
    {
        Vector<BoolVar> vars(C);
        Vector<Int> coeffs(C);
        for (int c = 0; c < C; ++c)
        {
            vars[c] = vars_client_plant_indicator[c][p];
            coeffs[c] = client_demand[c];
        }
        vars.push_back(vars_plant_open[p]);
        coeffs.push_back(-plant_capacity[p]);

        model.add_constr_linear(vars, coeffs, Sign::LE, 0);
    }

    // Create truck distance constraints.
    // cumulative(truck_number_of_client,
    //            1,
    //            [distance[c,p] * [plant_of_client[c] == p] for all c],
    //            max_distance)
    for (int p = 0; p < P; ++p)
    {
        Vector<BoolVar> active(C);
        Vector<IntVar> start(C);
        Vector<Int> duration(C, 1);
        Vector<Int> resource(C);
        for (int c = 0; c < C; ++c)
        {
            active[c] = vars_client_plant_indicator[c][p];
            start[c] = vars_truck_number_of_client[c];
            resource[c] = distance(c, p);
            release_assert(resource[c] <= max_distance,
                           "{} {}", resource[c], max_distance);
        }
        model.add_constr_cumulative_optional(active,
                                             start,
                                             duration,
                                             resource,
                                             max_distance);
    }

    // Create truck distance relaxation.
    // sum(c in C) (distance[c,p] * [plant_of_client[c] == p]) <= max_distance * trucks_used_at_plant[p]
    for (int p = 0; p < P; ++p)
    {
        Vector<BoolVar> vars(C);
        Vector<Int> coeffs(C);
        for (int c = 0; c < C; ++c)
        {
            vars[c] = vars_client_plant_indicator[c][p];
            coeffs[c] = distance(c, p);
        }

        model.add_constr_linear(vars,
                                coeffs,
                                Sign::LE,
                                0,
                                vars_trucks_used_at_plant[p],
                                max_distance);
    }

    // Calculate number of trucks used.
    // [plant_of_client[c] == p] -> trucks_used_at_plant[p] >= truck_number_of_client[c] + 1
    // [plant_of_client[c] == p] -> -1 >= truck_number_of_client[c] - trucks_used_at_plant[p]
    // [plant_of_client[c] == p] -> truck_number_of_client[c] - trucks_used_at_plant[p] <= -1
    for (int c = 0; c < C; ++c)
        for (int p = 0; p < P; ++p)
        {
            model.add_constr_reify_subtraction_leq(vars_client_plant_indicator[c][p],
                                                   vars_truck_number_of_client[c],
                                                   vars_trucks_used_at_plant[p],
                                                   -1);
        }

    // Add redundant constraint.
    // trucks_used_at_plant[p] <= sum(c in C) ([plant_of_client[c] == p])
    // sum(c in C) ([plant_of_client[c] == p]) >= trucks_used_at_plant[p]
    for (int p = 0; p < P; ++p)
    {
        Vector<BoolVar> vars(C);
        Vector<Int> coeffs(C, 1);
        for (int c = 0; c < C; ++c)
        {
            vars[c] = vars_client_plant_indicator[c][p];
        }
        model.add_constr_linear(vars, coeffs, Sign::GE, 0, vars_trucks_used_at_plant[p]);
    }

    // Solve.
    model.minimize(vars_cost, time_limit);

    // Print solution.
    if (model.get_status() == Status::Feasible || model.get_status() == Status::Optimal)
    {
        println("LB: {}", model.get_dual_bound());
        println("UB: {}", model.get_primal_bound());

        for (int c = 0; c < C; ++c)
            for (int p = 0; p < P; ++p)
                if (model.get_sol(vars_client_plant_indicator[c][p]))
                {
                    println("Customer {}: Plant {}, Truck {}", c, p, model.get_sol(vars_truck_number_of_client[c]));
                }

        for (int p = 0; p < P; ++p)
        {
            println("Plant {}: Number of trucks {}", p, model.get_sol(vars_trucks_used_at_plant[p]));
        }
    }

    // Done.
    return 0;
}
