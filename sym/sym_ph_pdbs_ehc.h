#ifndef SYM_PH_PDBS_EHC_H
#define SYM_PH_PDBS_EHC_H

#include "sym_ph.h"

typedef std::set<int> VarSet;

class SymHNode;
class SymPHPDBsEHC : public SymPH {
  std::map<VarSet, SymHNode *> generatedSets;
 public:
  SymPHPDBsEHC(const Options & opts);
  virtual ~SymPHPDBsEHC(){}

  virtual bool init();
  virtual SymBDExp * relax(SymBDExp * bdExp, Dir dir);
  virtual void dump_options() const;

};
#endif
