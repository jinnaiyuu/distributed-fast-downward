#include "sym_hnode.h"

#include "sym_ph.h"
#include "sym_bdexp.h"
#include "sym_manager.h"
#include "sym_engine.h" 

#include "../debug.h"

using namespace std;

SymHNode::
SymHNode(SymController * eng, const SymParamsMgr & params) : 
  engine(eng), ph(nullptr), abstraction(nullptr), 
  mgr(new SymManager(eng->getVars(), abstraction.get(),
		     params)){
}

SymHNode::
SymHNode(SymHNode * o, SymPH * ph_, 
	 unique_ptr<SymAbstraction> abs) : 
  engine(o->engine), ph(ph_),  abstraction(std::move(abs)), 
  mgr(new SymManager(o->getManager(), abstraction.get(),
		     ph_->getMgrParams())){
}

void SymHNode::failed_exploration(SymBDExp * newExp){
  failedForExps.insert(newExp);
  for(auto & p : parents){
    p->failed_exploration(newExp);
  }
}

void SymHNode::notuseful_exploration(SymBDExp * newExp){
  if(newExp){
    notUsefulForExps.insert(newExp);
    notuseful_exploration(newExp->getParent()); //This is not useful for newExp or its parent
    //All my childs are also not useful
    for(auto & c : children){
      c->notuseful_exploration(newExp);
    }
  }
}

void SymHNode::add_exploration(unique_ptr<SymBDExp> && newExp){
  //  if(res->init(this, searchDir, parent, maxTime, maxNodes)){
  exps.push_back(move(newExp));
    // parent->setHeuristic(*res, true);
    // cout << "I relaxed the exploration!!" << endl;
    // return true;
    //}else{
    //cout << "I cannot relax the exploration!!!!" << endl;
    //Ensure that we do not add another exploration in this HNode for the same exp
    //failedForExps.insert(parent);
    //return false;
    //}
}

bool SymHNode::hasExpFor(SymBDExp * bdExp) const{
  for(auto & exp : exps){
    if(exp->isExpFor(bdExp)){
      return true;
    }
  }
  if(failedForExps.count(bdExp)){
    return true;
  }
  if(notUsefulForExps.count(bdExp)){
    return true;
  }
  return false;
}

bool SymHNode::isUsefulFor(SymBDExp * bdExp) const {
  return notUsefulForExps.count(bdExp) == 0;
}

void SymHNode::getAllParents(set<SymHNode *> & setParents){
  for(auto p : parents){
    setParents.insert(p);
    p->getAllParents(setParents);
  }
}

void SymHNode::addChildren(SymHNode * newNode){
  DEBUG_MSG(cout << "ADD CHILDREN IN " << *this << ": " << *newNode << endl;);
  //Create and add the new explorations
  children.push_back(newNode);
}


void SymHNode::addParent(SymHNode * n){
  //Create and add the new explorations
  parents.push_back(n);
}


std::ostream & operator<<(std::ostream &os, const SymHNode & n){
  if(n.abstraction){
    os << *(n.abstraction) << "{" << n.exps.size() << "}";
  }else{
    os << "original {" << n.exps.size() << "}";
  }

/*os << " with explorations: ";
  for(auto & exp : n.exps){
    os << *(exp.get());
    }*/
  return os;
}


SymBDExp * SymHNode::relax(SymBDExp * exp) const {
    if(ph){
      return ph->relax(exp);
    }else{
      return engine->relax(exp);
    }
  }
