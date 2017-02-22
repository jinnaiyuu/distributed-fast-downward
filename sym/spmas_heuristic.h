#ifndef SPMAS_HEURISTIC_H
#define SPMAS_HEURISTIC_H

#include "sym_controller.h"
#include "sym_variables.h"
#include "../heuristic.h"
#include "sym_params.h"
#include <memory> 
#include "sym_solution.h"

class SymPH;
class Options;
class State;
class SymClosed;

class SPHeuristic{
 public:
  std::vector<ADD> heuristicADDs;
  std::vector<int> max_heuristic_value;
  int compute_heuristic(int * state) const;
};

class SPMASHeuristic : public Heuristic, public SymController {
  //std::unique_ptr<SymVariables> vars; //The symbolic variables are declared here
  // Search Behavior parameters
  std::vector<SymPH *> phs; 

  int generationTime;
  double generationMemory;

  std::vector<SPHeuristic> spHeuristics;
  std::vector<ADD> explicitHeuristics;

  std::vector<BDD> notMutexBDDs;

  SymSolution solution; 
  virtual void new_solution(const SymSolution & sol){
    if(!solution.solved() || 
       sol.getCost() < solution.getCost()){
      solution = sol;
    }
  }

  void dump_options() const;
  void insert_heuristic(SymClosed * closed);

 protected:
  virtual void initialize();
  virtual int compute_heuristic(const State &state);

 public:
  SPMASHeuristic(const Options &opts);
  virtual ~SPMASHeuristic(){}
};

#endif
