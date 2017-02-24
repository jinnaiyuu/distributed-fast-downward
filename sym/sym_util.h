#ifndef SYM_UTIL_H
#define SYM_UTIL_H

#include "sym_variables.h"
#include "sym_transition.h"
#include <math.h>
#include <ext/hash_map>
#include <memory> 
#include <iostream> 
#include <fstream> 
#include <sstream> 
#include <vector> 
#include <string> 
#include <map> 
//#include "../cudd-2.5.0/include/dddmp.h"


bool domain_has_cond_effects();

bool domain_has_axioms();


template<class T>
T getData(std::string line, const std::string & separator,
		const std::string & separator_end) {
	if (separator != "") {
		line.erase(line.begin(), line.begin() + line.find(separator) + 1);
	}

	if (separator_end != "") {
		line.erase(line.begin() + line.find(separator_end), line.end());
	}

	T res;
	std::stringstream ss;
	ss << line;
	if (!(ss >> res)) {
		std::cout << std::endl << "ERROR, could not parse: " << line
				<< std::endl;
		exit(-1);
	}
	return res;
}

template<class T>
T getData(std::ifstream & file, const std::string & separator) {
	std::string line;
	getline(file, line);
	T res = getData<T>(line, separator, "");
	return res;
}

template<class T, class FunctionMerge>
void mergeAux(std::vector<T> & elems, FunctionMerge f, int maxTime, int maxSize) {
	std::vector<T> result;
	if (maxSize <= 1 || elems.size() <= 1) {
		return;
	}
	Timer merge_timer;
	//  cout << "Merging " << elems.size() << ", maxSize: " << maxSize << endl;

	//Merge Elements
	std::vector<T> aux;
	while (elems.size() > 1 && (maxTime == 0 || merge_timer() * 1000 < maxTime)) {
		if (elems.size() % 2 == 1) { //Ensure an even number
			int last = elems.size() - 1;
			try {
				T res = f(elems[last - 1], elems[last], maxSize);
				elems[last - 1] = res;
			} catch (BDDError e) {
				result.push_back(elems[last]);
			}
			elems.erase(elems.end() - 1);
		}
		//    cout << "Iteration: " << elems.size() << endl;
		for (int i = 1; i < elems.size(); i += 2) {
			try {
				T res = f(elems[i - 1], elems[i], maxSize);
				aux.push_back(res);
			} catch (BDDError e) {
				if (elems[i].nodeCount() < elems[i - 1].nodeCount()) {
					result.push_back(elems[i - 1]);
					aux.push_back(elems[i]);
				} else {
					result.push_back(elems[i]);
					aux.push_back(elems[i - 1]);
				}
			}
		}
		aux.swap(elems);
		std::vector<T>().swap(aux);
	}
	//  cout << "Finished: " << elems.size() << endl;
	if (!elems.empty()) {
		result.insert(end(result), begin(elems), end(elems));
	}
	result.swap(elems);
	//Add all the elements remaining in aux
	if (!aux.empty()) {
		elems.insert(end(elems), begin(aux), end(aux));
	}
	/*for(int i = 0; i < aux.size(); i++){
	 elems.push_back(aux[i]);
	 }*/

	//  cout << "Merged to " << elems.size() << ". Took "<< merge_timer << " seconds" << endl;
}

/*
 * Method that merges some elements, 
 * Relays on several methods: T, int T.size() and bool T.merge(T, maxSize)
 */
template<class T, class FunctionMerge>
void merge(SymVariables * vars, std::vector<T> & elems, FunctionMerge f, int maxTime,
		int maxSize) {
	vars->setTimeLimit(maxTime);
	mergeAux(elems, f, maxTime, maxSize);
	vars->unsetTimeLimit();
}

/*
 * Method that merges some elements, 
 * Relays on several methods: T, int T.size() and bool T.merge(T, maxSize)
 */
template<class T, class FunctionMerge>
void merge(std::vector<T> & elems, FunctionMerge f, int maxSize) {
	mergeAux(elems, f, 0, maxSize);
}

SymTransition mergeTR(SymTransition tr, const SymTransition & tr2, int maxSize);
BDD mergeAndBDD(const BDD & bdd, const BDD & bdd2, int maxSize);
BDD mergeOrBDD(const BDD & bdd, const BDD & bdd2, int maxSize);

inline std::string dirname(bool fw) {
	return (fw ? "fw" : "bw");
}

#endif
