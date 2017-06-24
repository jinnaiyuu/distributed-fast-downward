#ifndef SYM_BDEXP_H
#define SYM_BDEXP_H

#include "sym_hnode.h"
#include "sym_exploration.h"
#include <memory>
#include <set>

class SymBDExp {

private:
	SymHNode * hnode;
	SymBDExp * parent;

	// In order to initialize a bd_exp, we keep the states that have
	// been closed by the exploration meant to use this exploration as
	// heuristic.  If the parent exploration is the original state
	// space: then, closed by parent holds the fw and bw states, else we
	// have closedByParent with the fw closed and closedByParentBw with
	// the Bw closed.
	BDD closedByParent;
	BDD closedByParentBw;

	std::unique_ptr<SymExploration> fw, bw;
	Dir searchDir;
	bool mayRelax; //If this is true, we forbid to abstract this exploration anymore

	//F-value of the main diagonal. The main diagonal is the first f
	//value in which there is a collision of both frontiers. We need to
	//know this value to stop abstract searches when they finish the
	//diagonal.
	int fMainDiagonal;

public:
	SymBDExp(SymController * engine, const SymParamsSearch & params, Dir dir); // Create with initial states
	SymBDExp(SymBDExp * other, const SymParamsSearch & params, Dir dir); // Create with other frontiers

	//Initialization is performed in two steps
	//initFrontier: sets the abstract state space and relax the frontier
	bool initFrontier(SymHNode * node, int maxTime, int maxNodes);
	//initAll: relax the rest of the search (should have called initFrontier first
	bool initAll(int maxTime, int maxNodes);

	void setHeuristic(SymBDExp & other);

	//Returns the best direction to search the bd exp
	SymExploration * selectBestDirection(bool skipUseful = false) const;

	bool finished() const;
	bool finishedMainDiagonal() const;

	inline SymHNode * getHNode() const {
		return hnode;
	}

	inline SymBDExp * getParent() const {
		return parent;
	}

	inline const BDD & getClosedByParent(bool fw) const {
		if (fw || (parent && !parent->isAbstracted())) {
			return closedByParent;
		} else {
			return closedByParentBw;
		}
	}

	inline SymManager * getManager() const {
		return hnode->getManager();
	}

	inline bool isAbstracted() const {
		return hnode->isAbstracted();
	}

	bool isExpFor(SymBDExp * bdExp) const;

	inline bool isUseful() const {
		return fw->isUseful() || bw->isUseful();
	}

	inline bool isUsefulAfterRelax(double ratio) const {
		return fw->getParent()->getClosed()->isUsefulAfterRelax(ratio,
				fw->getS())
				|| bw->getParent()->getClosed()->isUsefulAfterRelax(ratio,
						bw->getS());
		//bw->isUsefulAfterRelax(ratio);
	}

	inline bool isSearchable() {
		return isSearchableAfterRelax();
	}

	inline bool isSearchableAfterRelax(int num_relaxations = 0) const {
		return (searchDir != Dir::BW
				&& fw->isSearchableAfterRelax(num_relaxations))
				|| (searchDir != Dir::FW
						&& bw->isSearchableAfterRelax(num_relaxations));
	}

	inline bool isSearchableWithNodes(int maxNodes) const {
		return (searchDir != Dir::BW && fw->isSearchableWithNodes(maxNodes))
				|| (searchDir != Dir::FW && bw->isSearchableWithNodes(maxNodes));
	}

	inline bool isRelaxable() const {
		return mayRelax;
	}

	inline int getF() const {
		return std::max<int>(fw->getF(), bw->getF());
	}

	inline int getFMainDiagonal() const {
		return fMainDiagonal;
	}

	inline void setFMainDiagonal(int newVal) {
		DEBUG_MSG(std::cout << "SET F MAIN DIAGONAL: " << newVal << std::endl
		;
		);
		if (fMainDiagonal == -1) {
			fMainDiagonal = newVal;
			DEBUG_MSG(
					std::cout
							<< "FOUND CUT ON ABSTRACT STATE SPACE IN fMainDiagonal="
							<< fMainDiagonal << std::endl
					;
		);
	}
}

inline void forbidRelax() {
	mayRelax = false;
}

inline SymBDExp * relax() {
	SymBDExp * res = nullptr;
	if (mayRelax) {
		res = hnode->relax(this);
		if (!res) {
			mayRelax = false;
		}
	}
	return res;
}

inline std::set<SymExploration *> getExps() {
	std::set<SymExploration *> res;
	if (searchDir != Dir::BW)
		res.insert(fw.get());
	if (searchDir != Dir::FW)
		res.insert(bw.get());
	return res;
}

inline Dir getDir() const {
	return searchDir;
}

inline SymExploration * getFw() const {
	return fw.get();
}

inline SymExploration * getBw() const {
	return bw.get();
}

void desactivate() {
	fw->desactivate();
	bw->desactivate();
}

void reactivate() {
	fw->reactivate();
	bw->reactivate();
}

friend std::ostream & operator<<(std::ostream &os, const SymBDExp & other);

void getPlan(const BDD & cut, int g, int h,
		std::vector<const Operator *> & path) const;

//Two methods useful for debugging: store/load into/from a file
void write(const std::string & filename) const;
SymBDExp(SymController * engine, const SymParamsSearch & params, Dir dir,
		const std::string & filename, SymHNode * node);
};

#endif
