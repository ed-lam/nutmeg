//#define PRINT_DEBUG

#include "Includes.h"
#include "InstanceData.h"

#include "gurobi_c.h"
#include "geas/solver/solver.h"
#include "geas/constraints/builtins.h"

#define INT_TOL (0.01)

struct Problem
{
    InstanceData instance;

    GRBenv* env;
    GRBmodel* mip;
    GRBvar nb_mip_vars;
    Matrix<GRBvar> mip_vars_x;

    geas::solver cp;
    Matrix<geas::patom_t> cp_vars_x;
    Vector<geas::intvar> cp_vars_start;
    Vector<geas::intvar> cp_vars_capacity;
};

int __stdcall
callback_func(GRBmodel* mip, void* cbdata, int where, void* usrdata)
{
    // Gurobi only calls this callback function from the main thread so no mutex locks
    // are needed.

    // Stop if not in the correct part of the MIP solver.
    if (where != GRB_CB_MIPSOL && where != GRB_CB_MIPNODE)
    {
        return 0;
    }

    // Get problem.
    auto& prob = *reinterpret_cast<Problem*>(usrdata);

    // Get MIP solution.
    const auto nb_mip_vars = prob.nb_mip_vars;
    Vector<double> mip_vars_val(nb_mip_vars);
    if (where == GRB_CB_MIPSOL)
    {
        gurobi_assert(GRBcbget(cbdata, where, GRB_CB_MIPSOL_SOL, mip_vars_val.data()));
    }
    else
    {
        int status;
        gurobi_assert(GRBcbget(cbdata, where, GRB_CB_MIPNODE_STATUS, &status));
        if (status == GRB_OPTIMAL)
        {
            gurobi_assert(GRBcbget(cbdata,
                                   where,
                                   GRB_CB_MIPNODE_REL,
                                   mip_vars_val.data()));
        }
        else
        {
            return 0;
        }
    }

    // Print solution.
//#ifdef PRINT_DEBUG
//    for (GRBvar v = 0; v < nb_mip_vars; ++v)
//        if (std::abs(mip_vars_val[v]) > 0.1)
//        {
//            char* name;
//            gurobi_assert(GRBgetstrattrelement(mip, GRB_STR_ATTR_VARNAME, v, &name));
//            debug("{} = {}", name, mip_vars_val[v]);
//        }
//#endif

    // Allocate space to store the result.
    geas::solver::result result;

    // Get instance data.
    const auto& instance = prob.instance;
    const auto R = instance.R;

    // Get MIP variables.
    const auto& mip_vars_x = prob.mip_vars_x;

    // Get CP variables.
    auto& cp_vars_x = prob.cp_vars_x;

    // Create assumptions.
    auto& cp = prob.cp;
    cp.clear_assumptions();
    {
        for (int i = 0; i <= R; ++i)
            for (int j = 1; j <= R + 1; ++j)
                if (mip_vars_x(i, j) >= 0 &&
                    mip_vars_val[mip_vars_x(i, j)] > 1.0 - INT_TOL)
                {
                    auto result = cp.assume(cp_vars_x(i, j));
//                    debug("Assuming x[{},{}]", i,j);
                    if (!result)
                        goto GET_CONFLICT;
                }
        for (int i = 0; i <= R; ++i)
            for (int j = 1; j <= R + 1; ++j)
                if (mip_vars_x(i, j) >= 0 && mip_vars_val[mip_vars_x(i, j)] < INT_TOL)
                {
                    auto result = cp.assume(~cp_vars_x(i, j));
//                    debug("Assuming ~x[{},{}]", i,j);
                    if (!result)
                        goto GET_CONFLICT;
                }
    }

    // Solve.
    result = cp.solve();

    // Create nogood.
    debug_assert(result == geas::solver::UNSAT || result == geas::solver::SAT);
    if (result == geas::solver::UNSAT)
    {
        // Get conflict (a clause, i.e., disjunction of literals).
        GET_CONFLICT:
        vec<geas::patom_t> conflict;
        cp.get_conflict(conflict);

        // Create nogood.
        Vector<GRBvar> var_idx;
        Vector<double> var_coeff;
        double rhs = 1.0;
        double lhs = 0.0;
#ifdef PRINT_DEBUG
        fmt::print("Nogood:");
#endif
        for (const auto atom : conflict)
        {
            for (int i = 0; i <= R; ++i)
                for (int j = 1; j <= R + 1; ++j)
                    if (mip_vars_x(i, j) >= 0)
                    {
                        if (atom == cp_vars_x(i, j))
                        {
#ifdef PRINT_DEBUG
                            fmt::print(" x({},{})", i, j);
#endif
                            var_idx.push_back(mip_vars_x(i, j));
                            var_coeff.push_back(1.0);
                            lhs += mip_vars_val[mip_vars_x(i, j)];
                            goto NEXT_ATOM;
                        }
                        else if (atom == ~cp_vars_x(i, j))
                        {
#ifdef PRINT_DEBUG
                            fmt::print(" ~x({},{})", i, j);
#endif
                            var_idx.push_back(mip_vars_x(i, j));
                            var_coeff.push_back(-1.0);
                            rhs--;
                            lhs -= mip_vars_val[mip_vars_x(i, j)];
                            goto NEXT_ATOM;
                        }
                    }
            unreachable();
            NEXT_ATOM:;
        }
        debug("");

        // Add constraint.
        release_assert(var_idx.size() >= 2);
        release_assert(lhs < rhs, "Nogood is not violated");
        gurobi_assert(GRBcblazy(cbdata,
                                var_idx.size(),
                                var_idx.data(),
                                var_coeff.data(),
                                GRB_GREATER_EQUAL,
                                rhs));
    }
//#ifdef PRINT_DEBUG
//    else if (result == geas::solver::SAT)
//    {
//        // Print.
//        {
//            debug("Satisfiable");
//
//            for (int i = 0; i <= R; ++i)
//                for (int j = 1; j <= R + 1; ++j)
//                    if (mip_vars_x(i, j) >= 0)
//                    {
//                        const auto val = cp_vars_x(i, j).lb(cp.data->state.p_vals);
//                        if (val)
//                        {
//                            debug("x[{},{}] = 1", i, j);
//                        }
//                    }
//
//            auto& cp_vars_start = prob.cp_vars_start;
//            auto& cp_vars_capacity = prob.cp_vars_capacity;
//            for (int i = 0; i <= R + 1; ++i)
//            {
//                debug("{}, start[{}] = {}, capacity[{}] = {}",
//                      i,
//                      i,
//                      cp_vars_start[i].lb(cp.data),
//                      i,
//                      cp_vars_capacity[i].lb(cp.data));
//            }
//            printf("");
//        }
//    }
//#endif

    // Done.
    return 0;
}

int main(int argc, char** argv)
{
    // Read instance.
    release_assert(argc >= 2, "Path to instance must be second argument");
    Problem prob{.instance = InstanceData(argv[1])};

    // Get instance data.
    const auto& instance = prob.instance;
    const auto R = instance.R;
    const auto N = R + 2;
    const auto& cost = instance.cost_matrix;
    const auto& time_matrix = instance.time_matrix;

    // Print.
    instance.print();
    cost.print();
    instance.time_matrix.print();

    // Get MIP model.
    auto& mip = prob.mip;
    auto& nb_mip_vars = prob.nb_mip_vars;
    auto& mip_vars_x = prob.mip_vars_x;

    // Get CP model.
    auto& cp = prob.cp;
    auto& cp_vars_x = prob.cp_vars_x;
    auto& cp_vars_start = prob.cp_vars_start;
    auto& cp_vars_capacity = prob.cp_vars_capacity;

    // Create Gurobi model.
    gurobi_assert(GRBloadenv(&prob.env, nullptr));
    gurobi_assert(GRBnewmodel(prob.env,
                              &mip,
                              "Nutmeg",
                              0,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr));

    // Create edge variables.
    mip_vars_x.clear_and_resize(N, N, -1);
    cp_vars_x.clear_and_resize(N, N);
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
            if (!(i == 0 && j == R + 1) &&
                i != j &&
                instance.a[i] + time_matrix(i, j) <= instance.b[j])
            {
                // Create MIP variable.
                const auto name = fmt::format("x[{},{}]", i, j);
                gurobi_assert(GRBaddvar(mip,
                                        0,
                                        nullptr,
                                        nullptr,
                                        cost(i, j),
                                        0,
                                        1,
                                        GRB_BINARY,
                                        name.c_str()));
                mip_vars_x(i, j) = nb_mip_vars++;

                // Create CP variable.
                cp_vars_x(i, j) = cp.new_boolvar();
            }

    // Create start time variables.
    for (int i = 0; i < N; ++i)
    {
        const auto lb = instance.a[i];
        const auto ub = instance.b[i];
        cp_vars_start[i] = cp.new_intvar(lb, ub);
    }

    // Create vehicle capacity variables.
    cp_vars_capacity.resize(N);
    for (int i = 0; i < N; ++i)
    {
        const auto lb = std::abs(instance.q[i]);
        const auto ub = (i == 0 ? 0.0 : instance.Q);
        cp_vars_capacity[i] = cp.new_intvar(lb, ub);
    }

    // Update model.
    gurobi_assert(GRBupdatemodel(mip));

    // Create connectivity constraints in MIP.
    {
        Vector<GRBvar> var_idx;
        Vector<double> var_coeff;
        for (int i = 1; i <= R; ++i)
        {
            var_idx.clear();
            var_coeff.clear();
            for (int j = 1; j <= R + 1; ++j)
                if (mip_vars_x(i, j) >= 0)
                    var_idx.push_back(mip_vars_x(i, j));
            var_coeff.resize(var_idx.size(), 1.0);

            const auto name = fmt::format("out[{}]", i);
            gurobi_assert(GRBaddconstr(mip,
                                       var_idx.size(),
                                       var_idx.data(),
                                       var_coeff.data(),
                                       GRB_EQUAL,
                                       1.0,
                                       name.c_str()));
        }
        for (int i = 1; i <= R; ++i)
        {
            var_idx.clear();
            var_coeff.clear();
            for (int h = 0; h <= R; ++h)
                if (mip_vars_x(h, i) >= 0)
                    var_idx.push_back(mip_vars_x(h, i));
            var_coeff.resize(var_idx.size(), 1.0);

            const auto name = fmt::format("in[{}]", i);
            gurobi_assert(GRBaddconstr(mip,
                                       var_idx.size(),
                                       var_idx.data(),
                                       var_coeff.data(),
                                       GRB_EQUAL,
                                       1.0,
                                       name.c_str()));
        }
    }

    // Create connectivity constraints in CP.
    {
        vec<geas::clause_elt> clause;
        for (int i = 1; i <= R; ++i)
        {
            {
                clause.clear();
                for (int j = 1; j <= R + 1; ++j)
                    if (mip_vars_x(i, j) >= 0)
                        clause.push(cp_vars_x(i, j));
                auto c = geas::add_clause(*cp.data, clause);
                release_assert(c);
            }
            {
                clause.clear();
                for (int h = 0; h <= R; ++h)
                    if (mip_vars_x(h, i) >= 0)
                        clause.push(cp_vars_x(h, i));
                auto c = geas::add_clause(*cp.data, clause);
                release_assert(c);
            }
        }

        for (int h = 1; h <= R; ++h)
            for (int i = 1; i <= R+1; ++i)
                for (int j = 1; j <= R+1; ++j)
                    if (h != i && h != j && i != j &&
                        mip_vars_x(h, i) >= 0 && mip_vars_x(h, j) >= 0)
                    {
                        auto c = geas::add_clause(cp.data,
                                                  ~cp_vars_x(h, i),
                                                  ~cp_vars_x(h, j));
                        release_assert(c);
                    }
        for (int j = 1; j <= R; ++j)
            for (int h = 0; h <= R; ++h)
                for (int i = 0; i <= R; ++i)
                    if (h != i && h != j && i != j &&
                        mip_vars_x(h, j) >= 0 && mip_vars_x(i, j) >= 0)
                    {
                        auto c = geas::add_clause(cp.data,
                                                  ~cp_vars_x(h, j),
                                                  ~cp_vars_x(i, j));
                        release_assert(c);
                    }
    }

    // Create time successor constraints in CP.
    // x[i,j] -> start[i] + s[i] + cost[i,j] <= start[j]
    // x[i,j] -> start[i] - start[j] <= -s[i] - cost[i,j]
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
            if (!(i == 0 && j == R + 1) && mip_vars_x(i, j) >= 0)
            {
                const auto rhs = -time_matrix(i, j);

                auto c = geas::int_le(cp.data,
                                      cp_vars_start[i],
                                      cp_vars_start[j],
                                      rhs,
                                      cp_vars_x(i, j));
                release_assert(c);
            }

    // Create vehicle capacity successor constraints in CP.
    // x[i,j] -> capacity[i] + q[j] <= capacity[j]
    // x[i,j] -> capacity[i] - capacity[j] <= -q[j]
    for (int i = 0; i <= R; ++i)
        for (int j = 1; j <= R + 1; ++j)
            if (!(i == 0 && j == R + 1) && mip_vars_x(i, j) >= 0)
            {
                const auto q = instance.q[j];
                const auto rhs = -q;

                auto c = geas::int_le(cp.data,
                                      cp_vars_capacity[i],
                                      cp_vars_capacity[j],
                                      rhs,
                                      cp_vars_x(i, j));
                release_assert(c);
            }

    // Set callback.
    gurobi_assert(GRBsetcallbackfunc(mip,
                                     callback_func,
                                     static_cast<void*>(&prob)));
    gurobi_assert(GRBsetintparam(GRBgetenv(mip), GRB_INT_PAR_LAZYCONSTRAINTS, 1));

    // Set parameters.
    gurobi_assert(GRBsetintparam(GRBgetenv(mip), GRB_INT_PAR_THREADS, 1));

    // Set time limit.
    if (argc >= 3)
    {
        const auto time_limit = std::atof(argv[2]);
        release_assert(time_limit >= 1, "Time limit {} is invalid", argv[2]);
        gurobi_assert(GRBsetdblparam(GRBgetenv(mip),
                                     GRB_DBL_PAR_TIMELIMIT,
                                     time_limit));
    }

    // Write MIP.
#ifdef DEBUG
    gurobi_assert(GRBwrite(mip, fmt::format("{}.lp", prob.instance.data_name).c_str()));
#endif

    // Solve.
    gurobi_assert(GRBoptimize(mip));
    int status;
    gurobi_assert(GRBgetintattr(mip, GRB_INT_ATTR_STATUS, &status));

    // Write IIS.
    if (status == GRB_INFEASIBLE)
    {
#ifdef DEBUG
        gurobi_assert(GRBcomputeIIS(mip));
        gurobi_assert(GRBwrite(mip,
                               fmt::format("{}.ilp", prob.instance.data_name).c_str()));
#endif
    }
    // Print solution.
    else if (status == GRB_OPTIMAL)
    {
        // Get optimal value.
        double obj;
        gurobi_assert(GRBgetdblattr(mip, GRB_DBL_ATTR_OBJVAL, &obj));

        // Print optimal value.
        println("");
        println("--------------------------------------------------");
        println("Solution {}:", obj / 10.0);

        // Get solution.
        Vector<double> mip_vars_x_val(nb_mip_vars);
        gurobi_assert(GRBgetdblattrarray(mip,
                                         GRB_DBL_ATTR_X,
                                         0,
                                         nb_mip_vars,
                                         mip_vars_x_val.data()));

        // Print solution.
        for (int i = 0; i <= R; ++i)
            for (int j = 1; j <= R + 1; ++j)
                if (mip_vars_x(i, j) >= 0 &&
                    mip_vars_x_val[mip_vars_x(i, j)] > 1.0 - INT_TOL)
                {
                    println("x({},{}) = {}", i, j, mip_vars_x_val[mip_vars_x(i, j)]);
                }
    }
    // Print status.
    else
    {
        err("Unknown status {}", status);
    }

    // Done.
    return 0;
}
