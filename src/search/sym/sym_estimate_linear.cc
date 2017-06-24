#include "sym_estimate_linear.h"

#include <algorithm>
#include <iostream>

using namespace std;

SymStepCostEstimationLinear::SymStepCostEstimationLinear(const SymParamsSearch & p): 
  param_min_estimation_time(p.min_estimation_time),
  param_penalty_time_estimation_sum(p.penalty_time_estimation_sum ),
  param_penalty_time_estimation_mult(p.penalty_time_estimation_mult),
  param_penalty_nodes_estimation_sum(p.penalty_nodes_estimation_sum), 
  param_penalty_nodes_estimation_mult (p.penalty_nodes_estimation_mult),  
  prevNodes(-1), currentNodes(-1),  estimatedNodes(1),
  prevTime(-1), estimatedTime(-1)  {
}


void SymStepCostEstimationLinear::stepTaken(double time, double/* nodes*/){
  //cout << "!STEP TAKEN: " << time << " " << *this << " to ";
  prevNodes = currentNodes;
  prevTime = time + 1; //Ensure positive time even if it is 0
  //cout << *this << endl;
}

void SymStepCostEstimationLinear::nextStep(double nodes){
  //cout << "!NEXT STEP: " << nodes << " " << *this << " to ";
  currentNodes = nodes;
  if(prevTime < param_min_estimation_time){
    estimatedTime = prevTime;
    estimatedNodes = currentNodes;
  }else{
    double incrementNodes = max(0.0, currentNodes - prevNodes);
    estimatedNodes = currentNodes + incrementNodes;

    double proportionNodes = ((double)estimatedNodes)/((double)currentNodes);
    estimatedTime = prevTime*proportionNodes;
  }

  //cout << *this << endl;
  if(estimatedNodes < 0){
    cout << "ERROR: estimated nodes is lower than 0 after nextStep" << endl;
    exit(-1);
  }
}

void SymStepCostEstimationLinear::violated(double time_ellapsed, double time_limit, double node_limit){
  //cout << "VIOLATED " << time_ellapsed << " " << time_limit << " " << node_limit << endl;
  if(time_ellapsed > time_limit){
    violated_time(time_ellapsed);
  }else{
    violated_time(time_ellapsed);
    violated_nodes(node_limit);
  }
}

void SymStepCostEstimationLinear::violated_time(double time_ellapsed){
  //cout << "!VIOLATED TIME: " << time_ellapsed << "  " << *this << " to ";
  estimatedTime = param_penalty_time_estimation_sum + 
    max(estimatedTime, time_ellapsed)*param_penalty_time_estimation_mult;
  //Scale prevTime as well for the recalculation.
  prevTime = param_penalty_time_estimation_sum + 
    prevTime*param_penalty_time_estimation_mult;
  //cout << *this << endl;

}

void SymStepCostEstimationLinear::violated_nodes(double node_limit){
  //cout << "!VIOLATED NODES: " << *this << " to ";
  estimatedNodes = param_penalty_nodes_estimation_sum + 
    max(node_limit, estimatedNodes)*param_penalty_nodes_estimation_mult;
  
  //cout << *this << endl;
  if(estimatedNodes < 0){
    cout << "ERROR: estimated nodes is lower than 0 after violation" << endl;
    exit(-1);
  }
}

void SymStepCostEstimationLinear::recalculate(double nodes){
  if(currentNodes > 0){ // Only recalculate if estimation has been initialized 
    double proportion = ((double) nodes)/currentNodes;
    //cout << "!RECALCULATE PROPORTION: " << proportion << " = " << nodes << "/" << currentNodes << " to ";
    prevNodes *= proportion;
    prevTime *= proportion;
    currentNodes = nodes;
    estimatedNodes *= proportion;
    estimatedTime *= proportion;

    //cout << " RECALCULATED: " << *this << endl;
    if(estimatedNodes < 0){
      cout << "ERROR: estimated nodes is lower than 0 after recalculate with proportion: " 
	   << proportion << " = " << nodes << "/" << currentNodes << endl;
      exit(-1);
    }
  }
}


ostream & operator<<(ostream &os, const SymStepCostEstimationLinear & est){
  return os << "Est nodes: " << est.prevNodes << " => " << est.currentNodes << " => " << est.estimatedNodes
	    << " time: " << est.prevTime << " => " << est.estimatedTime;
}
