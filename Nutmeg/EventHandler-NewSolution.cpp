//#define PRINT_DEBUG

#include "EventHandler-NewSolution.h"
#include "Model.h"

#define EVENTHDLR_NAME         "bestsol"
#define EVENTHDLR_DESC         "event handler for best solutions found"

// Initialization method of event handler (called after problem was transformed)
static
SCIP_DECL_EVENTINIT(eventInitBestsol)
{
    // Check.
    debug_assert(scip);
    debug_assert(eventhdlr);
    debug_assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);

    // Notify SCIP that your event handler wants to react on the event type best solution found.
    scip_assert(SCIPcatchEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, NULL, NULL));

    // Exit.
    return SCIP_OKAY;
}

// Clean-up method of event handler (called before transformed problem is freed)
static
SCIP_DECL_EVENTEXIT(eventExitBestsol)
{
    // Check.
    debug_assert(scip);
    debug_assert(eventhdlr);
    debug_assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);

    // Notify SCIP that your event handler wants to drop the event type best solution found.
    scip_assert(SCIPdropEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, NULL, -1));

    // Exit.
    return SCIP_OKAY;
}

// Execution method of event handler
static
SCIP_DECL_EVENTEXEC(eventExecBestsol)
{
    // Check.
    debug_assert(eventhdlr);
    debug_assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);
    debug_assert(event);
    debug_assert(scip);
    debug_assert(SCIPeventGetType(event) == SCIP_EVENTTYPE_BESTSOLFOUND);

    // Print solution.
    auto model = reinterpret_cast<Nutmeg::Model*>(SCIPeventhdlrGetData(eventhdlr));
    debug_assert(model);
    model->print_new_solution();

    // Exit.
    return SCIP_OKAY;
}

// Include event handler for best solution found
SCIP_RETCODE Nutmeg::includeEventHdlrBestsol(SCIP* scip, Model* model)
{
    // Create event handler.
    SCIP_EVENTHDLR* eventhdlr = nullptr;
    scip_assert(SCIPincludeEventhdlrBasic(scip,
                                          &eventhdlr,
                                          EVENTHDLR_NAME,
                                          EVENTHDLR_DESC,
                                          eventExecBestsol,
                                          reinterpret_cast<SCIP_EVENTHDLRDATA*>(model)));
    debug_assert(eventhdlr);

    /// Attach initialisation and clean-up functions.
    scip_assert(SCIPsetEventhdlrInit(scip, eventhdlr, eventInitBestsol));
    scip_assert(SCIPsetEventhdlrExit(scip, eventhdlr, eventExitBestsol));

    // Exit.
    return SCIP_OKAY;
}
