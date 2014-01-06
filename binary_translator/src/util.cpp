#include "lldc/util.h"
#include <sstream>

std::string intToHexString(uint64_t val) {
    std::stringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}
