CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT (lldc)

SET (LLVM_FIND_REQUIRED TRUE)
INCLUDE (${PROJECT_SOURCE_DIR}/cmake/FindLLVM.cmake)

FIND_PACKAGE(PythonLibs 3.2 REQUIRED)

INCLUDE_DIRECTORIES(${LLVM_INCLUDE_DIR} 
                    ${PROJECT_SOURCE_DIR}/include
                    ${LLVM_OBJ_ROOT}/lib/Target/ARM
                    ${LLVM_SRC_ROOT}/lib/Target/ARM/MCTargetDesc
                    ${PYTHON_INCLUDE_PATH})

LINK_DIRECTORIES(${LLVM_LIB_DIR})

#SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS} -std=c++11")
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS} -fno-rtti")

LLVM_GET_LIBRARIES (LLVM_LIBRARIES core mc x86Info x86Desc x86Disassembler x86AsmPrinter x86CodeGen armInfo armDesc armAsmPrinter armDisassembler armCodeGen mcDisassembler mcParser mcJit jit mc)
MESSAGE ("Using LLVM libraries: ${LLVM_LIBRARIES}")
SET (LLDC_SOURCE
    src/PathState.cpp
    src/FunctionCache.cpp
    src/BasicBlockCache.cpp
    src/ArmReverseTranslators.cpp
    src/ArrayMemoryObject.cpp
    src/util.cpp
    src/PathsManager.cpp
    src/InstructionDecoder.cpp
    src/ReverseTranslationConfiguration.cpp
    src/ProxyMemoryObject.cpp
    src/InstructionTranslationUnitCache.cpp
    src/RecordingMemoryManager.cpp
    src/InstrumentMemoryAccessPass.cpp
    src/c_interface.cpp)

SET (BINARY_TRANSLATOR_SOURCE
    src/python_interface.c)

ADD_LIBRARY(lldc STATIC ${LLDC_SOURCE})
TARGET_LINK_LIBRARIES(lldc ${LLVM_LIBRARIES} ${LLVM_LDFLAGS})

ADD_LIBRARY (binary_translator SHARED ${BINARY_TRANSLATOR_SOURCE})
#SET_TARGET_PROPERTIES(lldc PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
#We don't want this pesky "lib" in front of our python library
SET_TARGET_PROPERTIES(binary_translator PROPERTIES PREFIX "")
IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    #Python wants the libary's postfix to be .so instead of .dylib on MacOS
    SET_TARGET_PROPERTIES(binary_translator PROPERTIES SUFFIX ".so")
ENDIF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
TARGET_LINK_LIBRARIES (binary_translator lldc ${PYTHON_LIBRARIES})

ADD_EXECUTABLE(bt_test test_translation.c)
TARGET_LINK_LIBRARIES(bt_test lldc)
