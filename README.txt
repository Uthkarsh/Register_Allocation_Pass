Concepts: 

For the mapping of the virtual to physical registers, VirtRegMap has been used. 
This mapping's reference is got by ANalysisUsage pass. Once this is populated with 
the virtual register to physical register/Stack slot address (For spilled registers), 
the virtRegRewriter will rewrite the registers (replace, rather) with the mapped 
physical registers/Stack Slot which will be used just before the target code is emitted. 

For the purpose of handling the spill registers, I have used various data structures (hash maps) to set aside 'n' (changable) number of registers from each class of register to be spill registers. So, whenever we run out of physical registers to be mapped to the virtual registers, I look at the map for the register set of that particular register class in the spilled register map. If that has not being used by that instruction ( other operands in the same instruction), then I shall replace the virtual register with the physical spill register. 

The assumption here is that no instruction will have more than 3 registers as operands. Of course, this assumption should be true. 

Before setting aside certain registers as spill registers, I am using another data structure to keep track of the explicit phyisical registers in the LLVM-IR and will avoid setting them to the spilled register's list. 

For the given test code, I have set aside 1 spill register per class as it seems that it is enough. But ideally, you can safely keep 3 and assume that it will be always allocated. 

Load and Store Instructions: 

I have added load and store instructions accordingly depending on if the instruction's operand is a use or def instruction. 

I use storeRegToStackSlot and loadRegFromStackSlot from the TargetInstructionInfo class to achieve the same. 


Snapshot Files: 

Register_Allocator_With_Spill_Func1().png - Register allocator run on the given test file with the spill (load and store) instructions along with setting aside spill registers for every class. Has the map for func1()

Register_Allocator_With_Spill_Func2().png - Same as above but for func2()

Register_Allocator_Without_Spill_Func1().png - Snapshot of a more naive approach where no spilling was done  by setting spill registers aside but required less spilling

Register_Allocator_Without_Spill_Func2().png -- Same as above but for func2()


Comments: As you can see, there is less spilling required if we don't set aside spill registers from each class separately, but for the above algorithm, although it works for the current test case, I believe that is not the correct way to do it. 


Advancements:  As mentioned, since this is kind of a naive way of doing things (Although correct with respect to the specifications of the project), it can of course be done more effeciently with more analysis. This can't be used in production.

Instructions to run the make file command: 

make reg_allocation 

Assumptions in the makefile: 

llc located in ../../../bin
The dynamic library file located in : ../../../lib/cs565opt_reg.dylib
Name of the register allocator = regalloc
outTestCase.bc present in: ../tests/outTestCase.bc
Output file name in the current directory: MyRegAllocTest.s

So, the final command will look something like:

../../../bin/llc -load=../../../lib/cs565opt_reg.dylib -regalloc=myregalloc ../tests/outTestCase.bc -o regTest.s -O0


Other Comments: 

1) Please ignore other miscellaneous debug print statements on the terminal.
2) If required, change the directory structure of the test or bin files and change the makefile accordingly. 

