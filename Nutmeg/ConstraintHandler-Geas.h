#ifndef NUTMEG_CONSTRAINTHANDLER_GEAS_H
#define NUTMEG_CONSTRAINTHANDLER_GEAS_H

#include "Includes.h"
#include "ProblemData.h"

#include "geas/solver/solver.h"
#include "geas/constraints/builtins.h"

namespace Nutmeg
{

struct NogoodData
{
    Vector<SCIP_VAR*> vars;
    Vector<SCIP_BOUNDTYPE> signs;
    Vector<SCIP_Real> bounds;
    bool all_binary{true};
#ifndef NDEBUG
    String name;
#endif
};

}

// Create the constraint handler
extern
SCIP_RETCODE SCIPincludeConshdlrGeas(
    SCIP* scip    // SCIP
);

// Create a constraint
extern
SCIP_RETCODE SCIPcreateConsBasicGeas(
    SCIP* scip,          // SCIP
    SCIP_CONS** cons,    // Pointer to hold the created constraint
    const char* name     // Name of constraint
);
extern
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
);

bool make_bool_assumptions(
    SCIP* scip,                       // SCIP
    SCIP_SOL* sol,                    // Solution
    Nutmeg::ProblemData& probdata,    // Problem data
    geas::solver& cp                  // CP solver
);

bool make_int_assumptions(
    SCIP* scip,                       // SCIP
    SCIP_SOL* sol,                    // Solution
    Nutmeg::ProblemData& probdata,    // Problem data
    geas::solver& cp                  // CP solver
);

bool make_obj_assumptions(
    SCIP* scip,                       // SCIP
    SCIP_SOL* sol,                    // Solution
    Nutmeg::ProblemData& probdata,    // Problem data
    geas::solver& cp                  // CP solver
);

Nutmeg::NogoodData get_nogood(
    geas::solver& cp,                // CP solver
    Nutmeg::ProblemData& probdata    // Problem data
);

#ifndef NDEBUG
Nutmeg::String make_nogood_name(
    Nutmeg::ProblemData& probdata,    // Problem data
    vec<geas::patom_t>& conflict      // Nogood
);
#endif

#ifdef USE_CUT_MINIMIZATION
void minimize_cut(
    geas::solver& cp,                 // CP solver
    Nutmeg::ProblemData& probdata,    // Problem data
    vec<geas::patom_t>& conflict      // Nogood
);
#endif

#endif
