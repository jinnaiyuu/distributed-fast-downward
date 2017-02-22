#include "sym_variables.h"

#include "sym_params.h" //Needed to get the parameters of CUDD init
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include "sym_util.h"
#include "../state_registry.h"

using namespace std;

//Constructor that makes use of global variables to initialize the symbolic_search structures
void SymVariables::init(const vector<int> & v_order,
		const SymParamsMgr & params) {
	cout << "Initializing Symbolic Variables" << endl;
	var_order = vector<int>(v_order);
	int num_fd_vars = var_order.size();

	//Initialize binary representation of variables.
	numBDDVars = 0;
	bdd_index_pre = vector<vector<int> >(v_order.size());
	bdd_index_eff = vector<vector<int> >(v_order.size());
	bdd_index_abs = vector<vector<int> >(v_order.size());

	//for each variable in the ordering, set the associated binary variables
	// for(int var : var_order){
	//   int var_len = ceil(log2(g_variable_domain[var]));
	//   for(int j = 0; j < var_len; j++){
	//     bdd_index_abs[var].push_back(numBDDVars);
	//     ++numBDDVars;
	//   }
	// }
	int _numBDDVars = 0; // numBDDVars;
	for (int var : var_order) {
		int var_len = ceil(log2(g_variable_domain[var]));
		numBDDVars += var_len;
		for (int j = 0; j < var_len; j++) {
			bdd_index_pre[var].push_back(_numBDDVars);
			bdd_index_eff[var].push_back(_numBDDVars + 1);
			_numBDDVars += 2;
		}
	}
	cout << "Num variables: " << var_order.size() << " => " << numBDDVars
			<< endl;

	/*  Numbddvars += kdflafklajfkljafjsak.
	 for (each abstract state){
	 numBDD
	 }*/

	//Initialize manager
	cout << "Initialize Symbolic Manager(" << _numBDDVars << ", "
			<< params.cudd_init_nodes / _numBDDVars << ", "
			<< params.cudd_init_cache_size << ", "
			<< params.cudd_init_available_memory << ")" << endl;
	_manager = unique_ptr<Cudd>(
			new Cudd(_numBDDVars, 0, params.cudd_init_nodes / _numBDDVars,
					params.cudd_init_cache_size,
					params.cudd_init_available_memory));

	/*  auto exceptionError = [this] (string message)
	 {
	 cerr << message << endl;
	 this->_manager->UnsetTimeLimit();
	 throw BDDError();
	 }*/

	_manager->setHandler(exceptionError);
	_manager->setTimeoutHandler(exceptionError);
	_manager->setNodesExceededHandler(exceptionError);

	cout << "Generating binary variables" << endl;
	//Generate binary_variables
	for (int i = 0; i < _numBDDVars; i++) {
		variables.push_back(_manager->bddVar(i));
	}

	/*  set<int> testsetaaa;
	 testsetaaa.insert(1);
	 testsetaaa.insert(5);

	 BDD TEST = getCubePre(testsetaaa);
	 TEST.print(2, 2);
	 */
	cout << "Generating predicate BDDs" << endl;
	preconditionBDDs.resize(num_fd_vars);
	effectBDDs.resize(num_fd_vars);
	biimpBDDs.resize(num_fd_vars);

	//Generate predicate (precondition (s) and effect (s')) BDDs
	for (int var : var_order) {
		for (int j = 0; j < g_variable_domain[var]; j++) {
			preconditionBDDs[var].push_back(createPreconditionBDD(var, j));
			effectBDDs[var].push_back(createEffectBDD(var, j));
		}
		biimpBDDs[var] = createBiimplicationBDD(bdd_index_pre[var],
				bdd_index_eff[var]);
	}

	binState.resize(_numBDDVars, 0);
	cout << "Symbolic Variables... Done." << endl;

	/*  for(int i = 0; i < g_variable_domain.size(); i++){
	 for(int j = 0; j < g_variable_domain[i]; j++){
	 cout << "Var-val: " << i << "-" << j << endl;
	 preconditionBDDs[i][j].print(1,2);
	 effectBDDs[i][j].print(1,2);
	 }
	 }*/
}

BDD SymVariables::getStateBDD(const State & state) const {
	BDD res = _manager->bddOne();
	for (int i = var_order.size() - 1; i >= 0; i--) {
		res = res * preconditionBDDs[var_order[i]][state[var_order[i]]];
	}
	return res;
}

State SymVariables::getStateFrom(const BDD & bdd) const {
//	vector<int> vals;
	state_var_t* variables = new state_var_t[g_variable_domain.size()];
	BDD current = bdd;
	for (int var = 0; var < g_variable_domain.size(); var++) {
		for (int val = 0; val < g_variable_domain[var]; val++) {
			BDD aux = current * preconditionBDDs[var][val];
			if (!aux.IsZero()) {
				current = aux;
				variables[var] = val;
				break;
			}
		}
	}
	return g_state_registry->build_state(variables);
//	return State(vals);
}

BDD SymVariables::getPartialStateBDD(
		const vector<pair<int, int> > & state) const {
	BDD res = _manager->bddOne();
	for (int i = state.size() - 1; i >= 0; i--) {
		// if(find(var_order.begin(), var_order.end(),
		// 		 state[i].first) != var_order.end()) {
		res = res * preconditionBDDs[state[i].first][state[i].second];
		//}
	}
	return res;
}

bool SymVariables::isIn(const State & state, const BDD & bdd) const {
	BDD sBDD = getStateBDD(state);
	return !((sBDD * bdd).IsZero());
}

double SymVariables::numStates(const BDD & bdd) const {
	return bdd.CountMinterm(numBDDVars);
}

void SymVariables::writeBucket(const string & fname, ofstream & filenames,
		const vector<BDD> & bucket) const {
	for (int i = 0; i < bucket.size(); ++i) {
		stringstream file;
		file << fname << "_" << i;
		bucket[i].write(file.str());
		filenames << file.str() << endl;
	}
	filenames << endl;
}

void SymVariables::readBucket(ifstream & filenames,
		vector<BDD> & bucket) const {
	string line;
	while (getline(filenames, line) && !line.empty()) {
		bucket.push_back(_manager->read_file(line));
	}
}

void SymVariables::writeMapBucket(const string & fname, ofstream & filenames,
		const map<int, vector<BDD>> & mb) const {
	for (const auto & entry : mb) {
		filenames << entry.first << endl;
		stringstream file;
		file << fname << entry.first;
		writeBucket(file.str(), filenames, entry.second);
	}
	filenames << -1 << endl;
}

void SymVariables::readMapBucket(ifstream & filenames,
		map<int, vector<BDD>> & data) const {
	int key = getData<int>(filenames, "");
	while (key != -1) {
		vector<BDD> bucket;
		readBucket(filenames, bucket);
		data[key] = bucket;
		key = getData<int>(filenames, "");
	}

}

void SymVariables::writeMap(const string & fname, ofstream & filenames,
		const map<int, BDD> & m) const {
	for (const auto & entry : m) {
		filenames << entry.first << endl;
		stringstream file;
		file << fname << entry.first;
		filenames << file.str() << endl;
		entry.second.write(file.str());
	}
	filenames << -1 << endl;
}

void SymVariables::readMap(ifstream & filenames, map<int, BDD> & data) const {
	int key = getData<int>(filenames, "");
	string filename;
	while (key != -1) {
		getline(filenames, filename);
		data[key] = readBDD(filename);
		key = getData<int>(filenames, "");
	}
}

BDD SymVariables::readBDD(const string & filename) const {
	cout << "Read BDD: " << filename << endl;
	return _manager->read_file(filename);
}

// Generate a BDD which represents a value using BDDvariables in _bddVars.
// _bddVars typically consists of variables which represent a single SAS+ variable.
BDD SymVariables::generateBDDVar(const std::vector<int> & _bddVars,
		int value) const {
	BDD res = _manager->bddOne();
	for (int i = 0; i < _bddVars.size(); i++) {
		if (value % 2) { //Check if the binary variable is asserted or negated
			res = res * variables[_bddVars[i]];
		} else {
			res = res * (!variables[_bddVars[i]]);
		}
		value /= 2;
	}
	return res;
}

// x y | XNOR(x,y)
// 0 0 | 0
// 0 1 | 1
// 1 0 | 1
// 1 1 | 0
BDD SymVariables::createBiimplicationBDD(const std::vector<int> & vars,
		const std::vector<int> & vars2) const {
	BDD res = _manager->bddOne();
	for (int i = 0; i < vars.size(); i++) {
		res *= variables[vars[i]].Xnor(variables[vars2[i]]);
	}
	return res;
}

vector<BDD> SymVariables::getBDDVars(const vector<int> & vars,
		const vector<vector<int>> & v_index) const {
	vector<BDD> res;
	for (int v : vars) {
		for (int bddv : v_index[v]) {
			res.push_back(variables[bddv]);
		}
	}
	return res;
}

BDD SymVariables::getCube(int var, const vector<vector<int>> & v_index) const {
	BDD res = oneBDD();
	for (int bddv : v_index[var]) {
		res *= variables[bddv];
	}
	return res;
}

BDD SymVariables::getCube(const set<int> & vars,
		const vector<vector<int>> & v_index) const {
	BDD res = oneBDD();
	for (int v : vars) {
		for (int bddv : v_index[v]) {
			res *= variables[bddv];
		}
	}
	return res;
}

void exceptionError(string /*message*/) {
	//cout << message << endl;
	throw BDDError();
}

