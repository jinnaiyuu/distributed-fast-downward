#ifndef SYM_CONTROLLER_H
#define SYM_CONTROLLER_H

//Shared class for SymEngine and smas_heuristic

#include "sym_manager.h"
#include "sym_enums.h"
#include "sym_params.h"
#include "../search_engine.h"

#include <vector>
#include <memory>
#include <utility>
#include <limits>

class SymExploration;
class SymAbstraction;
class SymSolution;
class SymHNode;
class SymVariables;
class Options;
class SymPH;
class SymBDExp;

class SymController { 
 protected:
  std::unique_ptr<SymVariables> vars; //The symbolic variables are declared here

  SymParamsMgr mgrParams; //Parameters for SymManager configuration.
  SymParamsSearch searchParams; //Parameters to search the original state space 
      
  // List of abstract state spaces. We store a list with the unique
  // pointer so that if we want ever to delete some hNode we just
  // remove it from this list. TODO: maybe we could use
  // shared_pointers instead....
  std::vector <std::unique_ptr<SymHNode>> nodes;

 public:
  SymController(const Options &opts);
  virtual ~SymController() {}

  virtual void new_solution(const SymSolution & /*sol*/){}
  virtual void setLowerBound(int /*lower*/){}
  virtual int getUpperBound() const{return std::numeric_limits<int>::max();}
  virtual int getLowerBound() const{return 0;}
  virtual bool solved () const{return false;}
  virtual SymBDExp * relax(SymBDExp * /*exp*/) const {return nullptr;}

  SymHNode * createHNode(SymHNode * node, std::unique_ptr <SymAbstraction> && abs, SymPH * ph);

  inline SymVariables * getVars(){
    return vars.get();
  }

  inline const SymParamsMgr & getMgrParams() const{
    return mgrParams;
  }

  inline const SymParamsSearch & getSearchParams() const{
    return searchParams;
  }

  static void add_options_to_parser(OptionParser &parser,
				    int maxStepTime, int maxStepNodes);
};

#endif
