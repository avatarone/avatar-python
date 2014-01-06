#ifndef _ARM_REGISTERS_H
#define _ARM_REGISTERS_H

#define GET_REGINFO_ENUM
#include "ARMBaseInfo.h"

namespace llvm {
    namespace ARM {
        static const unsigned FLAG_V = 0x101c;
        static const unsigned FLAG_C = 0x101d;
        static const unsigned FLAG_Z = 0x101e;
        static const unsigned FLAG_N = 0x101f;
    }
}

#endif /* _ARM_REGISTERS_H */