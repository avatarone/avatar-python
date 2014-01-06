#ifndef __TEST_CODE
#define __TEST_CODE

#include <stdint.h>
#include <stdlib.h>

struct DataInMemory {uint64_t address; uint64_t length; const char * code;};
extern struct DataInMemory memory_data[];
extern uint64_t entry_point;
extern size_t memory_data_entries;

#endif /*  __TEST_CODE */
