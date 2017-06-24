
#include "sym_heuristic.h"

#include "../debug.h"
using namespace std;

SymHeuristic::SymHeuristic(const SymVariables & vars,
			   const map<int, BDD> & heur) : zeroBDD(vars.zeroBDD()),
							 spuriousStates(vars.zeroBDD()) {
  BDD accumulated = vars.zeroBDD();
  for(const auto & entry : heur){
    DEBUG_MSG(cout << "SymHeuristic value: " << entry.first << 
	      ", bdd: " << entry.second.nodeCount() << endl;);
    if(entry.first >= 0){
      accumulated += entry.second;
      heuristic[entry.first] = accumulated;
      hValues.insert(entry.first);
    }else{
      spuriousStates += entry.second; 
    }
  }
}

BDD SymHeuristic::prunedStates(int hVal) const{

  auto it = heuristic.lower_bound(hVal);
  if(it == heuristic.end()){
    return zeroBDD;
  }
  if(it->first > hVal){
    if(it == heuristic.begin()) {
      return !zeroBDD; //We prune everything with this heuristic
    } else {
      it --;
    }
  }
  DEBUG_MSG(cout << "Pruning everything not closed with h equal or less to " << it->first << endl;);
  return !(it->second);
}

std::ostream & operator<<(std::ostream &os, const SymHeuristic & h){
  os << "Heur: ";
  for (const auto & e : h.heuristic){
    os << e.first << " : " << e.second.nodeCount() << ", ";
  }
  return os;
}

