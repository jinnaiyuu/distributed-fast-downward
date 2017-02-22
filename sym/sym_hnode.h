#ifndef SYM_HNODE_H
#define SYM_HNODE_H

#include "sym_hnode.h"
#include "sym_abstraction.h"
#include <memory> 
#include <vector> 
#include <set> 

class SymBDExp;

class SymPH;
class SymManager;
class SymController;
class SymExploration;

class SymHNode {
private:
	SymController * engine;
	SymPH * ph;
	std::unique_ptr<SymAbstraction> abstraction;
	std::unique_ptr<SymManager> mgr;

	std::vector<SymHNode *> children; //Nodes more abstracted
	std::vector<SymHNode *> parents; //Nodes less abstracted

	std::vector<std::unique_ptr<SymBDExp>> exps;
	std::set<SymBDExp *> failedForExps; //Set of exps we failed to abstract
	std::set<SymBDExp *> notUsefulForExps; //Set of exps we are not useful for

public:
	SymHNode(SymController * engine, const SymParamsMgr & params); //Constructor for the original state space
	SymHNode(SymHNode * o, SymPH * ph, std::unique_ptr<SymAbstraction> abs); // Constructor for abstract state space
	SymHNode(const SymHNode & o) = delete;
	SymHNode(SymHNode &&) = default;
	SymHNode& operator=(const SymHNode&) = delete;
	SymHNode& operator=(SymHNode &&) = default;
	~SymHNode() = default;

	void getAllParents(std::set<SymHNode *> & setParents);

	SymHNode * getParent() {
		return parents[0];
	}

	void add_exploration(std::unique_ptr<SymBDExp> && newExp);
	void failed_exploration(SymBDExp * newExp);
	void notuseful_exploration(SymBDExp * newExp);

	void addChildren(SymHNode * newNode);
	void addParent(SymHNode * newNode);

	bool empty() const {
		return exps.empty() && failedForExps.empty() && notUsefulForExps.empty();
	}
	bool hasExpFor(SymBDExp * bdExp) const;
	bool isUsefulFor(SymBDExp * bdExp) const;

	inline int numVariablesToAbstract() const {
		return abstraction->numVariablesToAbstract();
	}

	inline int numVariablesAbstracted() const {
		return abstraction->numVariablesAbstracted();
	}

	inline bool isAbstracted() const {
		return abstraction && abstraction->isAbstracted();
	}

	/*inline const std::vector <SymHNode *> & getChildren(SymPH * of_ph){
	 return children;
	 }*/

	inline std::vector<SymHNode *> getChildren(SymPH * of_ph) {
		std::vector<SymHNode *> res;
		for (auto c : children) {
			if (c->ph == of_ph)
				res.push_back(c);
		}
		return res;
	}

	SymBDExp * relax(SymBDExp * exp) const;

	inline SymManager * getManager() const {
		return mgr.get();
	}

	inline SymAbstraction * getAbstraction() {
		return abstraction.get();
	}

	inline SymController * getEngine() const {
		return engine;
	}

	friend std::ostream & operator<<(std::ostream &os, const SymHNode & n);
};

#endif
