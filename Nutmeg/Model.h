#ifndef NUTMEG_MODEL_H
#define NUTMEG_MODEL_H

#include "Includes.h"
#include "Variable.h"
#include "ProblemData.h"
#include "Solution.h"

namespace Nutmeg
{

enum class Sign
{
    EQ,
    LE,
    GE,
};

enum class Method
{
    BC,
    LBBD,
    CP,
    MIP
};

enum class Status
{
    Unknown,
    Optimal,
    Feasible,
    Infeasible,
    Error
};

class Model
{
    // Solvers
    Method method_;
    SCIP* mip_;
    geas::solver cp_;
    std::function<void()> print_new_solution_function_;

    // Problem
    ProblemData probdata_;
    Status status_;
    Float obj_;
    Float obj_bound_;
    Solution sol_;

    // Timer
    Float time_limit_;
    clock_t start_time_;
    Float run_time_;

  public:
    // Constructors
    // ------------
    Model() = delete;
    Model(const Method method = Method::BC);
    Model(const Model& model) = delete;
    Model(Model&& model) = delete;
    Model& operator=(const Model& model) = delete;
    Model& operator=(Model&& model) = delete;
    ~Model();

    // Get solver internal data
    // ------------------------
    inline geas::solver_data*& cp_data() { return cp_.data; }
    inline geas::solver& cp() { return cp_; }
    inline SCIP* mip() { return mip_; }
    inline void mark_as_infeasible() { status_ = Status::Infeasible; }

    // Create variables
    // ----------------
    BoolVar add_bool_var(const String& name = "");
    IntVar add_int_var(const Int lb,
                       const Int ub,
                       const bool include_in_mip = false,
                       const String& name = "");
    Vector<BoolVar> add_indicator_vars(const IntVar var, const Vector<Int>& domain = {});
    IntVar add_mip_var(const IntVar var);
    inline BoolVar add_mip_var(const BoolVar var) { return var; }
    void add_mip_int_var_as_bool_var_alias(const BoolVar bool_var, const IntVar int_var);

    // Functions to get variables
    // --------------------------
    inline Int nb_bool_vars() const { return probdata_.nb_bool_vars(); };
    inline Int nb_int_vars() const { return probdata_.nb_int_vars(); };
    inline Int lb(const IntVar var) const { return probdata_.lb(var); }
    inline Int ub(const IntVar var) const { return probdata_.ub(var); }
    inline const String& name(const BoolVar var) const { return probdata_.name(var); }
    inline const String& name(const IntVar var) const { return probdata_.name(var); }
    BoolVar get_neg(const BoolVar var);
    inline BoolVar get_false() { return BoolVar(this, 0); }
    inline BoolVar get_true() { return BoolVar(this, 1); }
    inline IntVar get_zero() { return IntVar(this, 0); }
    
    // Functions to get internal variables data
    // ----------------------------------------
    inline SCIP_VAR* mip_var(const BoolVar var) const { return probdata_.mip_var(var); }
    inline SCIP_VAR* mip_var(const IntVar var) const { return probdata_.mip_var(var); }
    inline bool is_pos_var(const BoolVar var) const { return probdata_.is_pos_var(var); }
    inline bool has_mip_indicator_vars(const IntVar var) const { return probdata_.has_mip_indicator_vars(var); }
    inline SCIP_VAR* mip_indicator_var(const IntVar var, const Int k) const { return probdata_.mip_indicator_var(var, k); }
    inline geas::patom_t cp_var(const BoolVar var) const { return probdata_.cp_var(var); }
    inline geas::intvar cp_var(const IntVar var) const { return probdata_.cp_var(var); }

    // Functions to get internal constraints data
    // ------------------------------------------
    inline Int& nb_linear_constraints() { return probdata_.nb_linear_constraints_; }
    inline Int& nb_indicator_constraints() { return probdata_.nb_indicator_constraints_; }

    // Create constraints
    // ------------------
    // Fix variable to true
    bool add_constr_fix(const BoolVar var);

    // coeff[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] <= / == / >= rhs
    bool add_constr_linear(const Vector<IntVar>& vars,
                           const Vector<Int>& coeffs,
                           const Sign sign,
                           const Int rhs);

    // coeff[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] != rhs
    bool add_constr_linear_neq(const Vector<IntVar>& vars,
                               const Vector<Int>& coeffs,
                               const Int rhs);

    // var == array[idx]
    bool add_constr_element(const IntVar& idx_var, const Vector<Int>& array, const IntVar& val_var);
    bool add_constr_element(const IntVar& idx_var, const Vector<IntVar>& array, const IntVar& val_var);

    // alldifferent(vars)
    bool add_constr_alldifferent(const Vector<IntVar>& vars);

    // Create constraints (unchecked)
    // ------------------------------
    // coeff[0] * vars[0] + ... + coeffs[n-1] * vars[n-1] <= / == / >= rhs + rhs_coeff * rhs_var
    bool add_constr_linear(const Vector<BoolVar>& vars,
                           const Vector<Int>& coeffs,
                           const Sign sign,
                           const Int rhs,
                           const IntVar rhs_var = {},
                           const Int rhs_coeff = 1);

    // coeff[0][0] * [vars[0] == lb(vars[0])] + coeff[0][1] * [vars[0] == lb(vars[0]) + 1] + ...
    // <= / == / >= rhs + rhs_coeff * rhs_var
    bool add_constr_linear(const Vector<IntVar>& vars,
                           const Vector<Vector<Int>>& coeffs,
                           const Sign sign,
                           const Int rhs,
                           const IntVar rhs_var = {},
                           const Int rhs_coeff = 1);

    // x[0] + ... + x[n-1] == 1
    bool add_constr_set_partition(const Vector<BoolVar>& vars);

    // x - y <= rhs
    bool add_constr_subtraction_leq(const IntVar x, const IntVar y, const Int rhs);

    // r -> x - y <= rhs
    bool add_constr_reify_subtraction_leq(const BoolVar r,
                                          const IntVar x,
                                          const IntVar y,
                                          const Int rhs);

    // (r == r_val) -> (x <= / == / >= x_val)
    bool add_constr_imply(const BoolVar r,
                          const bool r_val,
                          const IntVar x,
                          const Sign sign,
                          const Int x_val);

    // cumulative(start, duration, resource, capacity)
    bool add_constr_cumulative(const Vector<IntVar>& start,
                               const Vector<Int>& duration,
                               const Vector<Int>& resource,
                               const Int capacity,
                               const bool create_mip_linearization = false);

    // cumulative(start, duration, [resource[i] * active[i] for all i], capacity)
    // start[i] + duration[i] <= makespan for all i
    bool add_constr_cumulative_optional(const Vector<BoolVar>& active,
                                        const Vector<IntVar>& start,
                                        const Vector<Int>& duration,
                                        const Vector<Int>& resource,
                                        const Int capacity,
                                        const IntVar makespan = {});

    // Solve
    // -----
    void add_print_new_solution_function(std::function<void()> print_new_solution_function);
    void satisfy(const Float time_limit = Infinity, const bool verbose = true) { minimize(get_zero(), time_limit, verbose); }
    void minimize(const IntVar obj_var, const Float time_limit = Infinity, const bool verbose = true);
    inline void print_new_solution() { print_new_solution_function_(); }

    // Solution
    // --------
    Status get_status() const;
    Float get_runtime() const;
    Int get_dual_bound() const;
    Int get_primal_bound() const;
    bool get_sol(const BoolVar var);
    Int get_sol(const IntVar var);

    // Debug
    // -----
    void write_lp();

  private:
    // Solve
    // -----
    void minimize_using_bc(const IntVar obj_var, const Float time_limit, const bool verbose);
    void minimize_using_lbbd(const IntVar obj_var, const Float time_limit, const bool verbose);
    void minimize_using_mip(const IntVar obj_var, const Float time_limit, const bool verbose);
    void minimize_using_cp(const IntVar obj_var, const Float time_limit, const bool verbose);

    // Timer
    // -----
    void start_timer(const Float time_limit);
    Float get_cpu_time() const;
    Float get_time_remaining() const;
};

}

#endif
