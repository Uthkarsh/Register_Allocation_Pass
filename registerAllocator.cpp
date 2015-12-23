//#include "passes.h"

#include<unordered_map>
#include "llvm/pass.h"


#include "llvm/ADT/DepthFirstIterator.h"         // df_ext_iterator
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"

#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/MachineFunctionPass.h"  // MachineBasicBlock
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/CodeGen/Passes.h" 

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"  
#include "llvm/Target/TargetInstrInfo.h"  
// Has the information for the target registers and also about the number of registers for a particular class

#include "llvm/MC/MCInstrDesc.h"

#define DEBUG_TYPE "regalloc"

using namespace llvm;
using namespace std;

static RegisterRegAlloc myRegAlloc("myregalloc","my register allocator help string", (llvm::RegisterRegAlloc::FunctionPassCtor)createMyRegisterAllocator);

//class llvm::MachineFunction;

namespace {
class  registerAllocator : public MachineFunctionPass
	{
		/* data */
		MachineFunction * machine_function; // Machine Function object

		public: static char ID;
		
		// To get a mapping of the physical registers That are in use.
		// This is the one that is going to be eventually used by the virtRegWriter Pass 
		// To actually assign the registers. 
		VirtRegMap *VRM;   // Virtual Register Mapping
		 
		unordered_map<int,bool> *physRegsInUse;

		// Keeps track of the spilled virtual registers.
		unordered_map<int, int> *spilledVRegs;

		// To keep track of the explicit physical registers stored in the program
		set<unsigned> *explicitPhyRegs;

		// 3 Spill Registers set aside for every class of registers. 
		unordered_map<int,vector<unsigned>> *SpillRegs;

		unordered_map<unsigned,bool> Spills;

		
    	const TargetRegisterInfo * reg_info; 
    	

    registerAllocator();

	virtual bool runOnMachineFunction(MachineFunction &MF) override;

    void initialize_DS(MachineFunction &mf);
	void printOperandType(int type);
	virtual void getAnalysisUsage(AnalysisUsage &) const override;


	void init(VirtRegMap &vrm);
	
	MachineInstr *reloadVirtReg(MachineBasicBlock &MBB, MachineInstr *MI,
                                unsigned OpNum, MachineFunction &MF,unordered_map<unsigned,bool> &localSpillRegs);
	
	unsigned getFreeReg(const TargetRegisterClass *RC);

    bool isPhysRegAvailable(unsigned PhysReg) const {
 			
      	if(physRegsInUse->find(PhysReg)!= physRegsInUse->end())
      		return false;
      	else return true;

	}


	void seedLiveRegs(MachineFunction &MF);

	void clearDS();


	// Save all the physical registers in a set. 
	void getAllPhysicalRegs(MachineFunction &MF);

	void getAllSpillRegs(MachineFunction &MF);

	unsigned findSpillReg(MachineOperand &MOP, const TargetRegisterClass *RC, unordered_map<unsigned,bool> &localSpillRegs );

	};

char registerAllocator::ID = 0;


}

//static RegisterPass<registerAllocator> X("registerAllocator", "Global Register Allocator",false,false);

unsigned registerAllocator::findSpillReg(MachineOperand &MOP, const TargetRegisterClass *RC, unordered_map<unsigned,bool> &localSpillRegs)
{
	int ClassId = RC->getID();
	vector<unsigned> v = (SpillRegs->find(ClassId))->second;

	for(auto i = v.begin(); i!= v.end(); i++)
	{
		if(localSpillRegs.find(*i) == localSpillRegs.end())
		{
			localSpillRegs.insert(pair<unsigned,bool>(*i,1));
			return *i;
		}
	}

	assert(1 && "NO SPILL REGS FOUND");


	return 1;


}
void registerAllocator::getAllSpillRegs(MachineFunction &MF)
{
	

	for(auto i=reg_info->regclass_begin();i!=reg_info->regclass_end();i++)
	{
		// Iterate over every register class
		//const TargetRegisterClass TRC = *i;
		unsigned curClassID = (*i)->getID();

		for(auto j = (*i)->begin();j!=(*i)->end();j++)
		{
			if(SpillRegs->find(curClassID) == SpillRegs->end())
			{
				// First Time being encountered, so add and continue
				if(Spills.find(*j) == Spills.end() && explicitPhyRegs->find(*j) == explicitPhyRegs->end())
				{
				//errs()<<"\n ADDED:"<<*j<<"to the CLASS:"<<curClassID;
				SpillRegs->insert(pair<int, vector<unsigned>>(curClassID,vector<unsigned>()));
				(SpillRegs->find(curClassID))->second.push_back(*j);
				Spills.insert(pair<unsigned,bool>(*j,1));

				physRegsInUse->insert(pair<int,bool>(*j,1));
				}
				continue;
			}

			if(((SpillRegs->find(curClassID))->second).size() == 1)
			{
				/*for(auto temp = SpillRegs->find(curClassID)->second.begin(); temp!=SpillRegs->find(curClassID)->second.end(); temp++ )
					errs()<<"\n"<<*temp;*/
				//errs()<<"\n Found 3 in this Class so Breaking";
				break;
			}
			else
			{
				if(Spills.find(*j) == Spills.end() && explicitPhyRegs->find(*j) == explicitPhyRegs->end())
				{
					//errs()<<"\n ADDED:"<<*j<<"to the CLASS:"<<curClassID;
					(SpillRegs->find(curClassID))->second.push_back(*j);
					Spills.insert(pair<unsigned,bool>(*j,1));

					physRegsInUse->insert(pair<int,bool>(*j,1));
				}
			}
		}

	}


}


void registerAllocator::getAllPhysicalRegs(MachineFunction &MF)
{

	for(auto MBB = MF.begin();MBB!=MF.end();MBB++)
				{
					// For each Machine Basic Block
					//errs()<<"Next Basic Block"<<"\n";
					for(auto MI = MBB->begin();MI!=MBB->end();MI++)
					{
						// For each Machine Instruction

						const MCInstrDesc &desc = MI->getDesc();
						const MCOperandInfo *MCOI = desc.OpInfo;

						for (unsigned i = 0; i < MI->getNumOperands(); ++i) 
						{
      						MachineOperand& MO = MI->getOperand(i);
      						if (MO.isReg() &&
         					 TargetRegisterInfo::isPhysicalRegister(MO.getReg()))
      						{
      								//errs()<<"\n"<<"EXPLICIT REGISTER:"<<MO.getReg();
      								explicitPhyRegs->insert(MO.getReg());
      						}
      					}
					}

				}



}


registerAllocator::registerAllocator() : MachineFunctionPass(ID){

	initializeVirtRegMapPass(*PassRegistry::getPassRegistry());
	initializeLiveRegMatrixPass(*PassRegistry::getPassRegistry());
    initializeVirtRegRewriterPass(*PassRegistry::getPassRegistry());



}




void registerAllocator::getAnalysisUsage(AnalysisUsage &AU) const
{

  //assert(0 && "getAnalysisUsage CALLED");
  AU.addRequired<VirtRegMap>();
  AU.addPreserved<VirtRegMap>();

  //AU.addRequired<VirtRegRewriter>();
  //AU.addPreserved<VirtRegRewriter>();
  //AU.addRequired<LiveRegMatrix>();
  //AU.addPreserved<LiveRegMatrix>();
  MachineFunctionPass::getAnalysisUsage(AU);


}
void registerAllocator::initialize_DS(MachineFunction &MF)
{

	machine_function = &MF;
	const TargetMachine & target_machine = MF.getTarget();
	reg_info = target_machine.getRegisterInfo();

	physRegsInUse = new unordered_map<int, bool>();

	spilledVRegs = new unordered_map<int,int>();

	explicitPhyRegs = new set<unsigned>();

	SpillRegs = new unordered_map<int, vector<unsigned>>();

	unordered_map<unsigned,bool> Spills = unordered_map<unsigned,bool>();

}

/* An unused debugging function. */


void helper(MachineFunction &MF)
{

	const MachineRegisterInfo &regInfo = MF.getRegInfo();

	const TargetMachine & target_machine = MF.getTarget();
    const TargetRegisterInfo * reg_info = target_machine.getRegisterInfo();
    unsigned num_p_regs = reg_info->getNumRegs();



    errs()<<"The number of registers that are available are = "<< num_p_regs;

}



void registerAllocator::printOperandType(int type){

	switch(type)
	{
		case 0: errs()<<"Type = OPERAND_UNKNOWN\t";
		break;
		case 1: errs()<<"Type = OPERAND_IMMEDIATE\t";
		break;
		case 2: errs()<<"Type = OPERAND_REGISTER\t";
		break;
		case 3: errs()<<"Type = OPERAND_MEMORY\t";
		break;
		case 4: errs()<<"Type = OPERAND_PCREL\t";
		break;

	}

}



void registerAllocator::seedLiveRegs(MachineFunction &MF) {
  //NamedRegionTimer T("Seed Live Regs", TimerGroupName, TimePassesIsEnabled);
  for (unsigned i = 0, e = MF.getRegInfo().getNumVirtRegs(); i != e; ++i) {
    unsigned Reg = TargetRegisterInfo::index2VirtReg(i);
    if(!VRM->hasPhys(Reg))
    {
    	const MachineRegisterInfo &regInfo = MF.getRegInfo();

    	const TargetRegisterClass *RC = regInfo.getRegClass(Reg);

 		 unsigned PhysReg = getFreeReg(RC);
 
  		if(PhysReg>0)
		  {
  			  // Now that we have found a physical register to allocate, we have to 
 			 // Put it in the map. 
  			physRegsInUse->insert(pair<int, bool>(PhysReg,1));

  			VRM->assignVirt2Phys(Reg,PhysReg);
 		 }

 		 else
 		 {

 		 	// Spill code to be written

 		 	errs()<<"No available physical registers. Have to be spilled";

 		 	if(spilledVRegs->find(Reg) == spilledVRegs->end())
 		 	{
 		 		VRM->assignVirt2StackSlot(Reg);
 		 		spilledVRegs->insert(pair<int,int>(Reg,1));
 		 	}
 		 	else errs()<<"\n\nFound a VREG Attempting to Spill for the second time";
 		 }

 		 // Have to set the operands to the stack variable

    
    	errs()<<"\n Register with no physical register found = "<<Reg;
	}
  }
}

void registerAllocator::clearDS()
{

	delete physRegsInUse;
	delete spilledVRegs;
	delete explicitPhyRegs;
	delete SpillRegs;

}


bool registerAllocator::runOnMachineFunction(MachineFunction &MF)
		{

				errs() << "List of function calls:" << "::"<<MF.getName()<<"\n";
				//helper(MF);
				init(getAnalysis<VirtRegMap>());				
				initialize_DS(MF);
				getAllSpillRegs(MF);
				getAllPhysicalRegs(MF);

				for(auto MBB = MF.begin();MBB!=MF.end();MBB++)
				{
					// For each Machine Basic Block
					//errs()<<"Next Basic Block"<<"\n";
					for(auto MI = MBB->begin();MI!=MBB->end();MI++)
					{
						// For each Machine Instruction

						const MCInstrDesc &desc = MI->getDesc();
						const MCOperandInfo *MCOI = desc.OpInfo;

						unordered_map<unsigned,bool> localSpillRegs;

						for (unsigned i = 0; i < MI->getNumOperands(); ++i) 
						{
      						MachineOperand& MO = MI->getOperand(i);
      						if (MO.isReg() &&
         					 TargetRegisterInfo::isVirtualRegister(MO.getReg()))
      						{
      								reloadVirtReg(*MBB,MI,i,MF,localSpillRegs);
      						}
      					}
					}

				}

				seedLiveRegs(MF);

				errs()<<"\nPrinting The VRM\n";
				 errs()<<*VRM<<"Done\n";

				 MF.getRegInfo().clearVirtRegs();

				 clearDS();


				return true;
		}


void registerAllocator::init(VirtRegMap &vrm){

	VRM = &vrm; // Initializing the virtual register mapping


}



MachineInstr *registerAllocator::reloadVirtReg(MachineBasicBlock &MBB, MachineInstr *MI,
                                unsigned OpNum, MachineFunction &MF, unordered_map<unsigned,bool> &localSpillRegs) {

	const MachineRegisterInfo &regInfo = MF.getRegInfo();

	unsigned VirtReg = MI->getOperand(OpNum).getReg();

	errs()<<"\n The virtual register is="<<VirtReg;

	if (VRM->hasPhys(VirtReg)) {
	// Already have this value available!
	errs()<<"IT ALREADY HAS REGISTER:";
	unsigned PR = VRM->getPhys(VirtReg);
    MI->getOperand(OpNum).setReg(PR);  // Assign the input register
    return MI;
  }

  //errs()<<"\nTEST="<<(reg_info->regclass_end()-reg_info->regclass_begin());
  errs()<<" It does not have register";

   const TargetRegisterClass *RC = regInfo.getRegClass(VirtReg);

  unsigned PhysReg = getFreeReg(RC);
 
  if(PhysReg>0)
  {
  	  // Now that we have found a physical register to allocate, we have to 
  // Put it in the map. 
  	physRegsInUse->insert(pair<int, bool>(PhysReg,1));

  	VRM->assignVirt2Phys(VirtReg,PhysReg);

  	MI->getOperand(OpNum).setReg(PhysReg);

  	//errs()<<"\n Number of Virtual Register value = "<<MF.getRegInfo().getNumVirtRegs();


  }
  else
  {
  	  errs()<<"\n NO REGISTERS:SPILL in operand";

	 if(spilledVRegs->find(VirtReg) == spilledVRegs->end())
 		 	{

 		 		const TargetInstrInfo *TII;

 		 		const TargetMachine & TM = MF.getTarget();

 		 		const TargetRegisterInfo *TRI = TM.getRegisterInfo();

 		 		TII = TM.getInstrInfo();

 		 		// Assuming that this function returns the spill register in this class. 
 		 		unsigned spillPhysReg= findSpillReg(MI->getOperand(OpNum), RC,localSpillRegs);

 		 		//errs()<<"\n Found a Spill Register:"<<spillPhysReg;

 		 		MI->getOperand(OpNum).setReg(spillPhysReg);


 		 		int frameIndex = VRM->assignVirt2StackSlot(VirtReg);

 		 		MachineBasicBlock::iterator IT = MI;
		 		 MachineBasicBlock *parentBlock = MI->getParent();

 		 		if (MI->getOperand(OpNum).isDef()) 
 		 		{
		 		 	
 		 			errs()<<"\n Adding a Store instruction to a definition";
      		  		IT++;
		  			TII->storeRegToStackSlot(*parentBlock, IT, spillPhysReg, true, frameIndex, RC, TRI);
				} 
				else 
				{
					errs()<<"\n Adding a load instruction to a definition";
		  			TII->loadRegFromStackSlot(*parentBlock, IT, spillPhysReg,
					frameIndex, RC, TRI);
				}

 		 			spilledVRegs->insert(pair<int,unsigned>(VirtReg,spillPhysReg));
 		 	}

 		 	else 
 		 		{

 		 			unsigned spillPhysReg = spilledVRegs->find(VirtReg)->second;
 		 			MI->getOperand(OpNum).setReg(spillPhysReg);

 		 			//errs()<<"\nFound a VREG Attempting to Spill for the second time";

 		 		}
 		 
}
  // If no registers are found, then spill. 

	
	return MI;




}

/// getFreeReg - Look to see if there is a free register available in the
/// specified register class.  If not, return 0.
///
unsigned registerAllocator::getFreeReg(const TargetRegisterClass *RC) {
  // Get iterators defining the range of registers that are valid to allocate in
  // this class, which also specifies the preferred allocation order.
  auto RI = RC->begin();
  auto RE = RC->end();

  for (; RI != RE; ++RI)
    if (isPhysRegAvailable(*RI)) {       // Is reg unused?
      assert(*RI != 0 && "Cannot use register!");
      return *RI; // Found an unused register!
    }
  return 0;
}




FunctionPass* llvm::createMyRegisterAllocator() {
  return new registerAllocator();
}

//static RegisterRegAlloc myRegAlloc("myregalloc","my register allocator help string", (llvm::RegisterRegAlloc::FunctionPassCtor)createMyRegisterAllocator);

