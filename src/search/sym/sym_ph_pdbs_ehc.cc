#include "sym_ph_pdbs_ehc.h"

// #include "sym_engine.h" 
#include "sym_hnode.h"
#include "sym_bdexp.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../globals.h"
#include "../rng.h"
#include "sym_pdb.h"
#include "../debug.h"

SymPHPDBsEHC::SymPHPDBsEHC(const Options &opts) :
		SymPH(opts) {
}

bool SymPHPDBsEHC::init() {
	return true;
}

SymBDExp * SymPHPDBsEHC::relax(SymBDExp * bdExp, Dir /*dir*/) {
	cout << ">> Abstract " << *bdExp << " total time: " << g_timer << endl;

	//1) Get initial abstraction

	//2) For every possible child of the abstraction
	// 2a)  Search the child
	// 2b) Check if it is the best child so far
	// 2c) Add heuristic to bdExp (if necessary)

	return nullptr;
}

static SymPH *_parse(OptionParser &parser) {
	//maximum of 100 PDBs by default
	SymPH::add_options_to_parser(parser, "IND_TR_SHRINK", 100);

	Options opts = parser.parse();

	SymPH *policy = 0;
	if (!parser.dry_run()) {
		policy = new SymPHPDBsEHC(opts);
	}

	return policy;
}

void SymPHPDBsEHC::dump_options() const {
	SymPH::dump_options();
}

static Plugin<SymPH> _plugin("pdbs_ehc", _parse);
