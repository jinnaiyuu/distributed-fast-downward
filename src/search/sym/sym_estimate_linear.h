#ifndef SYM_ESTIMATE_LINEAR_H
#define SYM_ESTIMATE_LINEAR_H

#include "sym_params.h"
#include <iostream>

class SymStepCostEstimationLinear {

  double param_min_estimation_time;
  double param_penalty_time_estimation_sum, param_penalty_time_estimation_mult;
  double param_penalty_nodes_estimation_sum, param_penalty_nodes_estimation_mult;  

  // Last image was prevNodes ====(prevTime)====> currentNodes
  // Image to predict is currentNodes ====(estimatedTime)====> estimatedNodes
  double prevNodes, currentNodes, estimatedNodes;
  double prevTime, estimatedTime;

  void violated_time(double time_ellapsed); 
  void violated_nodes(double node_limit);
 //Recompute the estimation after reducing the frontier (notifyH)
  void recalculate(double nodes); 

 public:
  SymStepCostEstimationLinear(const SymParamsSearch & params);
  ~SymStepCostEstimationLinear(){}

  void stepTaken(double time, double nodes); //Called after any step, telling how much time was spent
  void nextStep(double nodes); //Called before any step, telling number of nodes to expand

  //Recompute the estimation if it has been exceeded
  void violated(double time_ellapsed, double time_limit, double node_limit);
  
  inline long time() const{
    return estimatedTime;
  }

  inline long nodes() const {
    return estimatedNodes;
  }

  inline long nextNodes() const {
    return currentNodes;
  }

  inline void recalculate(const SymStepCostEstimationLinear & o, double nodes){
    prevNodes = o.prevNodes;
    currentNodes = o.currentNodes;
    estimatedNodes = o.estimatedNodes;
    prevTime = o.prevTime;
    estimatedTime = o.estimatedTime;
    recalculate(nodes);
  }

  /* inline void update(double nodes, bool isNextStep){
    if(!isNextStep){
      recalculate(nodes);
    }else{
      nextStep(nodes);
    }
  }*/

  inline void violated_nodes(long nodes){
    violated(0, 1, nodes);
  }


  

  friend std::ostream & operator<<(std::ostream &os, const SymStepCostEstimationLinear & est);
  void write(std::ofstream & file) const;
  void read(std::ifstream & file);

};


#endif
