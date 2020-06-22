#ifndef NUTMEG_EVENTHANDLER_NEWSOLUTION_H
#define NUTMEG_EVENTHANDLER_NEWSOLUTION_H

#include "Includes.h"
#include "ProblemData.h"

namespace Nutmeg
{

class Model;

SCIP_RETCODE includeEventHdlrBestsol(SCIP* scip, Model* model);

}

#endif
