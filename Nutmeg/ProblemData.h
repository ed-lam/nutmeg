#ifndef NUTMEG_PROBLEMDATA_H
#define NUTMEG_PROBLEMDATA_H

#include "Includes.h"
#include "Variable.h"
#include "Solution.h"
#include "geas/solver/solver.h"
#include "geas/constraints/builtins.h"
#include "geas/vars/pred_var.h"
#include "geas/vars/monitor.h"

namespace Nutmeg
{

class Model;

struct ProblemData
{
    // Model
    Model& model_;

    // CP solver
    SCIP_CONS* cp_cons_;
    geas::solver& cp_;

    // Boolean variables
    Vector<SCIP_VAR*> mip_bool_vars_;
    Vector<Int> mip_neg_vars_idx_;
    Vector<geas::patom_t> cp_bool_vars_;
    Vector<String> bool_vars_name_;
    geas::bounds_monitor<geas::atom_var, int>& bool_vars_monitor_;

    // Integer variables
    Vector<SCIP_VAR*> mip_int_vars_;
    Vector<Vector<Int>> mip_indicator_vars_idx_;
    Vector<geas::intvar> cp_int_vars_;
    Vector<Int> int_vars_lb_;
    Vector<Int> int_vars_ub_;
    Vector<String> int_vars_name_;
    geas::bounds_monitor<geas::intvar, int>& int_vars_monitor_;

    // Objective variable
    Int obj_var_idx_;
    Int cp_dual_bound_;

    // Solution
    Solution& sol_;

  public:
    // Constructors
    ProblemData() noexcept = delete;
    ProblemData(Model& model, geas::solver& cp, Solution& sol) noexcept;
    ProblemData(const ProblemData& probdata) = default;
    ProblemData(ProblemData&& probdata) noexcept = delete;
    ProblemData& operator=(const ProblemData& probdata) noexcept = delete;
    ProblemData& operator=(ProblemData&& probdata) noexcept = delete;
    ~ProblemData() = default;

    // Functions to get variables
    Int nb_bool_vars() const;
    Int nb_int_vars() const;
//    bool has_mip_var(const IntVar var) const;
    SCIP_VAR* mip_var(const BoolVar var) const;
    SCIP_VAR* mip_var(const IntVar var) const;
    bool is_pos_var(const BoolVar var) const;
    bool is_pos_var(const Int bool_var_idx) const;
    bool has_mip_indicator_vars(const IntVar var) const;
    SCIP_VAR* mip_indicator_var(const IntVar var, const Int val) const;
    geas::patom_t cp_var(const BoolVar var) const;
    geas::intvar cp_var(const IntVar var) const;
    Int lb(const IntVar var) const;
    Int ub(const IntVar var) const;
    const String& name(const BoolVar var) const;
    const String& name(const IntVar var) const;
};

}

#endif
