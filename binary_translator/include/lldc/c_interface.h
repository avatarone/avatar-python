#ifndef _LLDC_C_INTERFACE_H
#define _LLDC_C_INTERFACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct 
{
    uint64_t address;
    uint64_t size;
    char *code;
} GeneratedCode;

typedef struct 
{
    const char * key;
    const char * value;
} DictionaryElement;

typedef struct 
{
    uint64_t start;
    uint64_t end;
} ProgramCounterRange;

typedef unsigned (*ReadCodeMemoryFunc)(void *opaque, uint64_t address, unsigned size, uint8_t *buffer);

/**
 * Instrument the given code - wrap it in a function, so that it can be invoked with two parameters:
 *  - The first parameter is the CPU state struct which contains all registers and flags.
 *  - The second parameter is the instrumentation struct, which contains the handler functions for memory read and write.
 * @param architecture A string that describes the architecture. Is used as the constructor for the LLVM triple first part.
 * @param entry_point Entry point where code analysis is started. Has to lie in one of the ranges specified by <code>pc_ranges</code>. bien sur.
 * @param pc_ranges An array of <code>ProgramCounterRange</code> structs (the last one has to have start = end = 0). The code is
 *   translated as long as the program counter of the current instruction falls in one of these ranges. If the program counter runs out of these
 *   ranges, a function return is emitted and the current processor state is saved to the processor state struct (first argument).
 * @param generated_code_address The base address of the code that is generated. This is at the same time the function entry address of the
 *   instrumented code.
 * @param get_code_callback This function is used to get data from code memory at compile time.
 * @param opaque Any pointer you want that is passed as first argument to the <code>get_code_callback</code> function.
 * @param generated_code The return structure where information about the generated code is stored. When size != 0 at return, you have to free
 *   the code pointer after usage.
 * @param opts More options for more narrow LLVM processor selection. Currently not used.
 * @return 0 on success, error code otherwise.
 */
int instrument_memory_access(const char * architecture,
                             uint64_t entry_point,
                             ProgramCounterRange * pc_ranges,
                             uint64_t generated_code_address,
                             ReadCodeMemoryFunc get_code_callback,
                             void *opaque,
                             GeneratedCode * generated_code,
                             DictionaryElement * opts);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LLDC_C_INTERFACE_H */