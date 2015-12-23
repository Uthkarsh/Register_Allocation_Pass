# Makefile for hello pass

# Path to top level of LLVM hierarchy
LEVEL = ../..

# Name of the library to build
LIBRARYNAME = cs565opt_reg

# Make the shared library become a loadable module so the tools can
# dlopen/dlsym on the resulting library.
LOADABLE_MODULE = 1

# Might need to change this to .so in LINUX
LIB_EXT = .dylib

TEST_PRINT = $(LEVEL)/../bin/opt -load $(LEVEL)/../lib/$(LIBRARYNAME)$(LIB_EXT) -FunctionPassAnalysis <
FUNC_CALLS = $(LEVEL)/../bin/opt -load $(LEVEL)/../lib/$(LIBRARYNAME)$(LIB_EXT) -funcCall <
# Include the makefile implementation stuff

bitcode_file = ../tests/regTest.bc
test_file = ../tests/outTestCase.bc
output_native_assembly = MyRegAllocTest.s

REG_ALLOC = $(LEVEL)/../bin/llc -load=$(LEVEL)/../lib/$(LIBRARYNAME)$(LIB_EXT) -regalloc=basic $(bitcode_file) -o $(output_native_assembly)

REG_ALLOC2 = $(LEVEL)/../bin/llc -load=$(LEVEL)/../lib/$(LIBRARYNAME)$(LIB_EXT) -regalloc=myregalloc $(test_file) -o $(output_native_assembly) -O0

REG_ALLOC3 =  $(LEVEL)/../bin/llc -load=$(LEVEL)/../lib/$(LIBRARYNAME)$(LIB_EXT) -regalloc=myregalloc $(bitcode_file) -o $(output_native_assembly) -O0 -filetype=obj

include $(LEVEL)/Makefile.common

typeCheck:
	$(TEST_PRINT) ../tests/Test4.bc > /dev/null

funcCall:
	$(FUNC_CALLS) ../tests/Test7.bc > /dev/null

reg_allocation: 
	$(REG_ALLOC2)

basicR:
	$(REG_ALLOC)

reg_allocation_obj:
	$(REG_ALLOC3)


gdb: run `../../../bin/llc -load=../../../lib/cs565opt_reg.dylib -regalloc=myregalloc ../tests/regTest.bc -o regTest.s -O0`

