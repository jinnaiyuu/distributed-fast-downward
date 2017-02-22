#ifndef SYM_PH_SMAS_H
#define SYM_PH_SMAS_H

#include "sym_ph.h"
#include "smas_variable_order_finder.h" //Needed for MergeStrategy type
#include <vector>
#include <memory>

class SMASShrinkStrategy;
class SymSMAS;

class SymPHSMAS : public SymPH {
  const MergeStrategy merge_strategy;
  SMASShrinkStrategy * const shrink_strategy;
  const bool use_label_reduction;
  const bool use_expensive_statistics;
//const bool write_to_disk; //If bdds should be written to disk

  OperatorCost cost_type;
  bool is_unit_cost_problem;

  std::vector<SymSMAS *> atomic_abstractions;
  
  SymAbstraction * finalMASAbstraction;
 public:
  SymPHSMAS();
  SymPHSMAS(const Options & opts);
  
  virtual ~SymPHSMAS(){}

  virtual void dump_options() const;
  virtual bool init();
  virtual SymBDExp * relax(SymBDExp * bdExp, SymHNode * iniHNode, Dir dir, int num_relaxations);

  virtual bool relaxGetsHarder(){
    return true;
  }

 private:
  std::unique_ptr<SymAbstraction> abstract(SymAbstraction * other, double allotedTime);
  std::unique_ptr<SymAbstraction> noneAbstraction();
  std::unique_ptr<SymBDExp> relax_one_by_one(SymBDExp * bdExp, SymHNode * iniHNode, Dir dir, int num_relaxations);

  //Returns an abstraction that we are able to search explicitly
  virtual SymAbstraction * getExplicitAbstraction(){
    return finalMASAbstraction;
  }
};
#endif
