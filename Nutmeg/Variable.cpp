#include "Variable.h"
#include "Model.h"

namespace Nutmeg
{

BoolVar BoolVar::operator~() { return model->get_neg(*this); }

//BoolVar IntVar::operator==(const Int val) { return model->get_eq(*this, val); }
//BoolVar IntVar::operator<=(const Int val) { return model->get_le(*this, val); }
//BoolVar IntVar::operator>=(const Int val) { return model->get_ge(*this, val); }
//BoolVar IntVar::operator<(const Int val) { return model->get_le(*this, val - 1); }
//BoolVar IntVar::operator>(const Int val) { return model->get_ge(*this, val + 1); }

}
