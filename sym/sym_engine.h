#ifndef SYM_ENGINE_H
#define SYM_ENGINE_H

#include <vector>
#include <memory>

#include "sym_controller.h"
#include "sym_enums.h"
#include "../search_engine.h"

class SymExploration;
class SymAbstraction;
class SymSolution;
class SymHNode;
class SymVariables;
class Options;
class SymPH;
class SymBDExp;

class SymEngine : public SearchEngine, public SymController { 
 protected:
  //unique_ptr<SymVariables> vars; //The symbolic variables are declared here
  
  Dir searchDir; //Direction of search in the original state space
  
  // List of hierarchy policies to derive new abstractions
  std::vector <SymPH *> phs;
    
  // List of abstract state spaces. We store a list with the unique
  // pointer so that if we want ever to delete some hNode we just
  // remove it from this list. TODO: maybe we could use
  // shared_pointers instead....
  //std::vector <std::unique_ptr<SymHNode>> nodes;

  //Variable to keep the current lower bound. Used to know when we have proven an optimal solution.
  int lower_bound;

  //The exploration and state space of the original problem
  SymHNode * originalStateSpace;
  SymBDExp * originalSearch;

  //Inherited methods
  virtual void initialize();
  virtual int step() = 0;

  //Auxiliar method to get the return code for step()
  int stepReturn() const;
 public:
  SymEngine(const Options &opts);
  virtual ~SymEngine(){}

  void statistics() const;
  void dump_search_space();
  virtual void new_solution(const SymSolution & sol);

  virtual void setLowerBound(int lower){
    if(lower > lower_bound){
      lower_bound = lower;
      std::cout << "BOUND: " << lower_bound << " < " << bound << ", total time: " << g_timer << std::endl;
    }
  }

  virtual int getUpperBound() const{
    return bound;
  }

   virtual int getLowerBound() const{
    return lower_bound;
  }
  virtual bool solved () const {
    return lower_bound >= bound;
  }

  virtual SymBDExp * relax(SymBDExp * exp) const;

  static void add_options_to_parser(OptionParser &parser);
  static void set_default_options(Options & opts);
};

#endif
