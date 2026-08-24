#include "../engine.h"

void S_ExtraUpdate()
{
    eng()->S_ExtraUpdate.InvokeChained();
}
