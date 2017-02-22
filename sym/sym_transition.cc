#include "sym_transition.h"
#include "../debug.h"
#include "sym_abstraction.h"
#include "sym_manager.h"
#include "sym_pdb.h" 
#include "sym_smas.h" 

#include "../timer.h"
using namespace std;

#ifdef __CDT_PARSER__
#undef DEBUG_MSG
#define DEBUG_MSG(a)  ;
#endif

// Transition of a single operator op.
SymTransition::SymTransition(SymVariables * sVars, const Operator * op,
		int cost_) :
		sV(sVars), cost(cost_), tBDD(sVars->oneBDD()), existsVars(
				sVars->oneBDD()), existsBwVars(sVars->oneBDD()), absAfterImage(
				nullptr) {

	ops.insert(op);
	for (int i = 0; i < op->get_prevail().size(); i++) { //Put precondition of label
		const Prevail &prevail = op->get_prevail()[i];
		tBDD *= sV->preBDD(prevail.var, prevail.prev);
	}

	map<int, BDD> effect_conditions;
	map<int, BDD> effects;
	// Get effects and the remaining conditions. We iterate in reverse
	// order because pre_post at the end have preference
	for (int i = op->get_pre_post().size() - 1; i >= 0; i--) {
		const PrePost &pre_post = op->get_pre_post()[i];
		int var = pre_post.var;
		if (std::find(begin(effVars), end(effVars), var) == end(effVars)) {
			effVars.push_back(var);
		}

		BDD condition = sV->oneBDD();
		BDD ppBDD = sV->effBDD(var, pre_post.post);
		if (effect_conditions.count(var)) {
			condition = effect_conditions.at(var);
		} else {
			effect_conditions[var] = condition;
			effects[var] = sV->zeroBDD();
		}

		for (const auto & cPrev : pre_post.cond) {
			condition *= sV->preBDD(cPrev.var, cPrev.prev);
		}
		effect_conditions[var] *= !condition;
		effects[var] += (condition * ppBDD);
		//Add precondition to the tBDD
		if (pre_post.pre != -1) {
			tBDD *= sV->preBDD(pre_post.var, pre_post.pre);
		}
	}

	//Add effects to the tBDD
	for (auto it = effects.rbegin(); it != effects.rend(); ++it) {
		int var = it->first;
		BDD effectBDD = it->second;
		//If some possibility is not covered by the conditions of the
		//conditional effect, then in those cases the value of the value
		//is preserved with a biimplication
		if (!effect_conditions[var].IsZero()) {
			effectBDD += (effect_conditions[var] * sV->biimp(var));
		}
		tBDD *= effectBDD;
	}
	if (tBDD.IsZero()) {
		cerr << "ERROR, DESAMBIGUACION: " << op->get_name() << endl;
		//exit(0);
	}

	sort(effVars.begin(), effVars.end());
	for (int var : effVars) {
		for (int bdd_var : sV->vars_index_pre(var)) {
			swapVarsS.push_back(sV->bddVar(bdd_var));
		}
		for (int bdd_var : sV->vars_index_eff(var)) {
			swapVarsSp.push_back(sV->bddVar(bdd_var));
		}
	}
	assert(swapVarsS.size() == swapVarsSp.size());
	// existsVars/existsBwVars is just the conjunction of swapVarsS and swapVarsSp
	for (int i = 0; i < swapVarsS.size(); ++i) {
		existsVars *= swapVarsS[i];
		existsBwVars *= swapVarsSp[i];
	}
	//DEBUG_MSG(cout << "Computing tr took " << tr_timer; tBDD.print(1, 1););
}

void SymTransition::shrink(const SymAbstraction & abs, int maxNodes) {
	tBDD = abs.shrinkTBDD(tBDD, maxNodes);

	// effVars
	vector<int> newEffVars;
	for (int var : effVars) {
		if (abs.isRelevantVar(var)) {
			newEffVars.push_back(var);
		}
	}
	newEffVars.swap(effVars);
}

bool SymTransition::setMaSAbstraction(SymAbstraction */* abs*/,
		const BDD & bddSrc, const BDD & bddTarget) {
	// existsVars += abs.getRelVarsCubePre();
	// existsBwVars += abs.getRelVarsCubePre();
	// swapVarsS = getSwapPre(fullVars);
	// swapVarsSp = getSwapEff(fullVars);
	// swapVarsA = getSwapAbs(absVars);
	// swapVarsAp = getSwapPre(absVars);

	tBDD *= bddSrc;
	tBDD *= bddTarget;
	return true;
}

// SymTransition::SymTransition(const SymTransition & ot) : cost(ot.cost), tBDD(ot.tBDD), 
// 							 existsVars (ot.existsVars), existsBwVars (ot.existsBwVars), 
// 							 swapVarsS(ot.swapVarsS), swapVarsSp (ot.swapVarsSp),
// 							 id(ot.id), effVars(ot.effVars), ops(ot.ops){
//   }

BDD SymTransition::image(const BDD & from) const {
	BDD aux = from;
	if (!swapVarsA.empty()) {
		aux = from.SwapVariables(swapVarsA, swapVarsAp);
	}
	BDD tmp = tBDD.AndAbstract(aux, existsVars);
	BDD res = tmp.SwapVariables(swapVarsS, swapVarsSp);

	if (absAfterImage) {
		//TODO: HACK: PARAMETER FIXED
		res = absAfterImage->shrinkExists(res, 10000000);
	}
	return res;

}

BDD SymTransition::image(const BDD & from, int maxNodes) const {
	DEBUG_MSG(
			cout << "Image cost " << cost << " from " << from.nodeCount() << " with " << tBDD.nodeCount() ;);
	BDD aux = from;
	if (!swapVarsA.empty()) {
		aux = from.SwapVariables(swapVarsA, swapVarsAp);
	}
	Timer t;
	BDD tmp = tBDD.AndAbstract(aux, existsVars, maxNodes);
	DEBUG_MSG(cout << " tmp " << tmp.nodeCount() << " in " << t() ;);
	BDD res = tmp.SwapVariables(swapVarsS, swapVarsSp);
	DEBUG_MSG(cout << " res " << res.nodeCount() << " took " << t() ;);
	if (absAfterImage) {
		res = absAfterImage->shrinkExists(res, maxNodes);
		DEBUG_MSG(
				cout << " shrunk: " << res.nodeCount() << " tookTotal: " << t() ;);
	}DEBUG_MSG(cout << endl ;);

	return res;
}

BDD SymTransition::preimage(const BDD & from) const {
	BDD tmp = from.SwapVariables(swapVarsS, swapVarsSp);
	BDD res = tBDD.AndAbstract(tmp, existsBwVars);
	if (!swapVarsA.empty()) {
		res = res.SwapVariables(swapVarsA, swapVarsAp);
	}
	if (absAfterImage) {
		res = absAfterImage->shrinkExists(res, numeric_limits<int>::max());
	}
	return res;
}

BDD SymTransition::preimage(const BDD & from, int maxNodes) const {
	Timer t;
	DEBUG_MSG(
			cout << "Image cost " << cost << " from " << from.nodeCount() << " with " << tBDD.nodeCount() << flush ;);
	BDD tmp = from.SwapVariables(swapVarsS, swapVarsSp);
	DEBUG_MSG(cout << " tmp " << tmp.nodeCount() << " in " << t() << flush ;);
	BDD res = tBDD.AndAbstract(tmp, existsBwVars, maxNodes);
	if (!swapVarsA.empty()) {
		res = res.SwapVariables(swapVarsA, swapVarsAp);
	}DEBUG_MSG(cout << "res " << res.nodeCount() << " took " << t() ;);
	if (absAfterImage) {
		res = absAfterImage->shrinkExists(res, maxNodes);
		DEBUG_MSG(
				cout << " shrunk: " << res.nodeCount() << " tookTotal: " << t() ;);
	}DEBUG_MSG(cout << endl ;);

	return res;
}

void SymTransition::merge(const SymTransition & t2, int maxNodes) {
	assert(cost == t2.cost);
	if (cost != t2.cost) {
		cout << "Error: merging transitions with different cost: " << cost
				<< " " << t2.cost << endl;
		exit(-1);
	}

	//  cout << "set_union" << endl;
	//Attempt to generate the new tBDD
	vector<int> newEffVars;
	set_union(effVars.begin(), effVars.end(), t2.effVars.begin(),
			t2.effVars.end(), back_inserter(newEffVars));

	BDD newTBDD = tBDD;
	BDD newTBDD2 = t2.tBDD;

	//    cout << "Eff vars" << endl;
	vector<int>::const_iterator var1 = effVars.begin();
	vector<int>::const_iterator var2 = t2.effVars.begin();
	for (vector<int>::const_iterator var = newEffVars.begin();
			var != newEffVars.end(); ++var) {
		if (var1 == effVars.end() || *var1 != *var) {
			//cout << "a" << newTBDD.getNode() << endl;
			BDD tmp = sV->biimp(*var);
			//cout << "a " << newTBDD.getNode() << " and " << tmp.getNode() << endl;
			newTBDD = newTBDD * tmp;
			//      cout << "done." << endl;

			//newTBDD *= sV->biimp(*var);
			//cout << "b" << tmp.getNode() << endl;
			//cout << "c" <<  newTBDD.getNode()  << endl;
			// cout << "Biimp 1: "; sV->biimp(*var).print(1, 2);
		} else {
			++var1;
		}

		if (var2 == t2.effVars.end() || *var2 != *var) {
			//cout << "a2" << newTBDD.getNode() << endl;
			BDD tmp = sV->biimp(*var);
			//      cout << "a2 " << newTBDD.getNode() << " and " << tmp.getNode() << endl;
			newTBDD2 = newTBDD2 * tmp;
			//      cout << "done." << endl;
			//newTBDD2 *= sV->biimp(*var);
			//      cout << "b2" << tmp.getNode() << endl;
			//cout << "c2" <<  newTBDD.getNode()  << endl;
			//cout << "Biimp 2: "; sV-> biimp(*var).print(1, 2);
		} else {
			++var2;
		}
	}
	newTBDD = newTBDD.Or(newTBDD2, maxNodes);

	if (newTBDD.nodeCount() > maxNodes) {
		DEBUG_MSG(
				cout << "TR size exceeded: " << newTBDD.nodeCount() << ">" << maxNodes << endl ;);
		throw BDDError();
//return false; //We could not sucessfully merge
	}

	tBDD = newTBDD;

	/*cout << "Eff vars: ";
	 for(int i = 0; i < effVars.size(); i++) cout << effVars[i] << " ";
	 cout << endl;*/

	effVars.swap(newEffVars);

	/*  cout << "New eff vars: ";
	 for(int i = 0; i < effVars.size(); i++) cout << effVars[i] << " ";
	 cout << endl;*/

	/*cout << "EXIST BW VARS: " << endl;
	 existsBwVars.print(1, 2);
	 t2.existsBwVars.print(1, 2);*/
	existsVars *= t2.existsVars;
	existsBwVars *= t2.existsBwVars;

	/*  existsBwVars.print(1, 2);

	 cout << "Swap vars" << endl;
	 for(int i = 0; i < swapVarsS.size(); i++){
	 cout << "Swap "; swapVarsS[i].PrintMinterm();
	 cout << "with "; swapVarsSp[i].PrintMinterm();
	 }

	 cout << "Swap vars 2" << endl;
	 for(int i = 0; i < swapVarsS.size(); i++){
	 cout << "Swap "; swapVarsS[i].PrintMinterm();
	 cout << "with "; swapVarsSp[i].PrintMinterm();
	 }*/

	for (int i = 0; i < t2.swapVarsS.size(); i++) {
		if (find(swapVarsS.begin(), swapVarsS.end(), t2.swapVarsS[i])
				== swapVarsS.end()) {
			swapVarsS.push_back(t2.swapVarsS[i]);
			swapVarsSp.push_back(t2.swapVarsSp[i]);
		}
	}

	ops.insert(t2.ops.begin(), t2.ops.end());

	/*  cout << "New Swap vars" << endl;
	 for(int i = 0; i < swapVarsS.size(); i++){
	 cout << "Swap "; swapVarsS[i].PrintMinterm();
	 cout << "with "; swapVarsSp[i].PrintMinterm();
	 }*/

	//  return true;
}

// SymTransition::SymTransition(const SymAbstraction & abs, 
// 			     const SymVariables * sV, 
// 			     const Operator * op, int cost_) :
//   cost (cost_), 
//   existsVars(sV->oneBDD()), existsBwVars(sV->oneBDD()), id (op->get_name()) {
//   //  Timer tr_timer;

//   //  cout << "Creating transition for: " <<  id << endl;
//   /* In order to generate tBDD we compute two parts. First, the label takes
//      into account the pre/eff in the operator over non-relevant variables.
//   Then, we use t_by_op over variables describing the abstract states. 
//   tBDD is the product of those BDDs. 

//   swapVarsS/swapVarsSp
//   existsVars/existsBwVars is just the conjunction of swapVarsS and swapVarsSp
//   */

//   BDD labelTransitions = sV->oneBDD();
//   //Put precondition of label
//   for (int i = 0; i < op->get_prevail().size(); i++) {
//     const Prevail &prevail = op->get_prevail()[i];
//     if(!abs.is_in_varset(prevail.var)){
//       labelTransitions *= sV->preBDD(prevail.var, prevail.prev);
//     }
//   }

//   //  op->dump();

//   // Update values affected by operator.
//   for (int i = 0; i < op->get_pre_post().size(); i++) {
//     const PrePost &pre_post = op->get_pre_post()[i];
//     //    cout << "Effect over variable: " << pre_post.var << " " <<
//     //      g_variable_name[pre_post.var] <<  endl;
//     if(!abs.is_in_varset(pre_post.var)){      
//       BDD ppBDD = sV->effBDD(pre_post.var, pre_post.post);
//       if(pre_post.pre != -1){
// 	ppBDD *= sV->preBDD(pre_post.var, pre_post.pre);
//       }
//       labelTransitions *= ppBDD;
//       effVars.push_back(pre_post.var);
//       //We are applying an effect so we must "forget" the value of that variables
//       vector <int> varsS = sV->varsS(pre_post.var);
//       //effVarsS.insert(effVarsS.end(), varsS.begin(), varsS.end());
//       for(int var = 0; var < varsS.size(); var++){
// 	swapVarsS.push_back(sV->bddVar(varsS[var]));
// 	swapVarsSp.push_back(sV->bddVar(varsS[var]+1));
// 	//TODO: Check if we are doing this in the right order.
//       }
//     }
//   }

//   //    cout << "Abstract state" << endl;

//   // Consider all the pairs of abstract states with that transition
//   BDD absStateTransitions = sV->zeroBDD();
//   for(int i = 0; i < t_by_op.size(); i++){
//     AbstractStateRef sSrc = t_by_op[i].src;
//     AbstractStateRef sTar = t_by_op[i].target;

//     BDD bddSrc = abs.absStatePreBDD(sSrc);
//     BDD bddTar = abs.absStateEffBDD(sTar);

//     absStateTransitions += bddSrc*bddTar;
//     //    cout << "TInfo Adding " << sSrc << " -> " << sTar << endl; 
//     //(bddSrc*bddTar).print(1, 1);
//   }

//   swapVarsS.insert(swapVarsS.end(),
// 		   abs.getAbsVarsS().begin(), abs.getAbsVarsS().end());
//   swapVarsSp.insert(swapVarsSp.end(),
// 		    abs.getAbsVarsSp().begin(), abs.getAbsVarsSp().end());

//   assert(swapVarsS.size() == swapVarsSp.size());
//   for(int i = 0; i < swapVarsS.size(); i++){
//     existsVars *= swapVarsS[i];
//     existsBwVars *= swapVarsSp[i];
//   }  
//   tBDD = labelTransitions*absStateTransitions;

//   sort(effVars.begin(), effVars.end());
//   //  cout << "Computing tr with " << t_by_op.size() << " abstract state transitions took " << tr_timer; tBDD.print(1, 1);
//   /*  cout << "TInfo: " << op->get_name() << endl;
//   labelTransitions.print(1, 1);
//   absStateTransitions.print(1, 1);
//   tBDD.print(1, 1);*/
//   //lalalatBDD *= absStateTransitions;
//   //vector <int> varsS = sV->absStateVarsS();
//   //effVarsS.insert(effVarsS.end(), varsS.begin(), varsS.end());
// }

void SymTransition::edeletion(SymManager & mgr) {
	if (ops.size() != 1) {
		cerr
				<< "Error, trying to apply edeletion over a transition with more than one op"
				<< endl;
		exit(-1);
	}

	//For each op, include relevant mutexes
	for (const Operator * op : ops) {
		for (const PrePost & pp : op->get_pre_post()) {
			//edeletion bw
			if (pp.pre == -1) {
				//We have a post effect over this variable.
				//That means that every previous value is possible
				//for each value of the variable
				for (int val = 0; val < g_variable_domain[pp.var]; val++) {
					tBDD *= mgr.getNotMutexBDDBw(pp.var, val);
				}
			} else {
				//In regression, we are making true pp.pre
				//So we must negate everything of these.
				tBDD *= mgr.getNotMutexBDDBw(pp.var, pp.pre);
			}
			//edeletion fw
			tBDD *= mgr.getNotMutexBDDFw(pp.var, pp.post).SwapVariables(
					swapVarsS, swapVarsSp);

			//edeletion invariants
			tBDD *= mgr.getExactlyOneBDD(pp.var, pp.post);
		}
	}
}

ostream & operator<<(std::ostream &os, const SymTransition & tr) {
	os << "TR(";
	for (auto op : tr.ops) {
		os << op->get_name() << ", ";
	}
	return os << "): " << tr.tBDD.nodeCount() << endl;
}

// SymTransition::SymTransition(const SymVariables * sV, const Operator * op,
// 	      int cost_, const SymAbstraction & abs):
//   cost (cost_), tBDD (sV->oneBDD()),
//   existsVars(sV->oneBDD()), existsBwVars(sV->oneBDD()), id (op->get_name()) {

//   // Timer tr_timer;

//   //  cout << "Creating abstract transition for: " <<  id << endl;
//   /* In this case, we only take into account the label: 
//      pre/eff in the operator over non-relevant variables.

//      swapVarsS/swapVarsSp
//      existsVars/existsBwVars is just the conjunction of swapVarsS and swapVarsSp
//   */
//   ops.insert(op);
//   BDD labelTransitions = sV->oneBDD();
//   //Put precondition of label
//   for (int i = 0; i < op->get_prevail().size(); i++) {
//     const Prevail &prevail = op->get_prevail()[i];
//     if(abs.isRelevantVar(prevail.var)){
//       labelTransitions *= sV->preBDD(prevail.var, prevail.prev);
//     }
//   }

//   //  op->dump();

//   // Update values affected by operator.
//   for (int i = 0; i < op->get_pre_post().size(); i++) {
//     const PrePost &pre_post = op->get_pre_post()[i];
//     //  cout << "Effect over variable: " << pre_post.var << " " <<
//     //      g_variable_name[pre_post.var] <<  endl;
//     if(abs.isRelevantVar(pre_post.var)){
//       effVars.push_back(pre_post.var);
//       BDD ppBDD = sV->effBDD(pre_post.var, pre_post.post);
//       if(pre_post.pre != -1){
// 	ppBDD *= sV->preBDD(pre_post.var, pre_post.pre);
//       }
//       labelTransitions *= ppBDD;

//       //We are applying an effect so we must "forget" the value of that variables
//       vector <int> varsS = sV->varsS(pre_post.var);
//       //effVarsS.insert(effVarsS.end(), varsS.begin(), varsS.end());
//       for(int var = 0; var < varsS.size(); var++){
// 	swapVarsS.push_back(sV->bddVar(varsS[var]));
// 	swapVarsSp.push_back(sV->bddVar(varsS[var]+1));
// 	//TODO: Check if we are doing this in the right order.
//       }
//     }
//   }

//   assert(swapVarsS.size() == swapVarsSp.size());
//   for(int i = 0; i < swapVarsS.size(); i++){
//     existsVars *= swapVarsS[i];
//     existsBwVars *= swapVarsSp[i];
//   }  

//   tBDD = labelTransitions;
//   sort(effVars.begin(), effVars.end());

//   //cout << "Computing tr took " << tr_timer; tBDD.print(1, 1);
// }
