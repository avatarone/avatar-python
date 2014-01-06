#ifndef _ARM_REVERSE_TRANSLATORS_H
#define _ARM_REVERSE_TRANSLATORS_H

#include "ReverseTranslationConfiguration.h"
#include "ExtendedMCInst.h"

void getArmReverseTranslators(std::map<unsigned, ReverseInstructionTranslatorFunction>& translators);

#endif /* _ARM_REVERSE_TRANSLATORS_H */
