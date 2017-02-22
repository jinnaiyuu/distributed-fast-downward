#ifndef SMAS_SHRINK_STATE_H
#define SMAS_SHRINK_STATE_H

#include "sym_variables.h"
#include <ext/slist>
#include "smas_shrink_strategy.h"

class SMASAbsState;
class SMASShrinkState {
 public:
  BDD bdd; //bdd that describes the abstract state
  BDD cube;// variables used to describe the abstract state
  bool marked;

 SMASShrinkState(SymVariables * vars) :  
  bdd(vars->oneBDD()), cube(vars->oneBDD()), 
    marked(false){
  }

 SMASShrinkState(const std::shared_ptr<SMASShrinkState> & o1,
		 const std::shared_ptr<SMASShrinkState> & o2): 
  bdd(o1->bdd*o2->bdd), cube(o1->cube*o2->cube), marked(false){
  }
  
  SMASShrinkState(SymVariables * vars, 
		  std::vector<std::shared_ptr<SMASAbsState>> & absStates,
		  const __gnu_cxx::slist<AbstractStateRef> & group);
 
  inline BDD shrinkExists(const BDD & from, int maxNodes) const{  
    return from.AndAbstract(bdd, cube, maxNodes)*bdd;
  }

  inline BDD shrinkForall(const BDD & from, int maxNodes) const{
    BDD aux = (from*bdd) + !bdd;
    return aux.UnivAbstract(cube, maxNodes)*bdd;
  }

  friend std::ostream & operator<<(std::ostream &os, const SMASShrinkState & other);
};

#endif
