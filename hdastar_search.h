#ifndef HDASTAR_SEARCH_H
#define HDASTAR_SEARCH_H

#include <vector>

#include "open_lists/open_list.h"
#include "search_engine.h"
#include "search_space.h"
#include "state.h"
#include "timer.h"
#include "evaluator.h"
#include "search_progress.h"

class Heuristic;
class Operator;
class ScalarEvaluator;
class Options;

class HDAStarSearch: public SearchEngine {

	// Search Behavior parameters
	bool reopen_closed_nodes; // whether to reopen closed nodes upon finding lower g paths
	bool do_pathmax; // whether to use pathmax correction
	bool use_multi_path_dependence;

	OpenList<StateID> *open_list;
	ScalarEvaluator *f_evaluator;

	/////////////////////////////////
	// MPI related
	/////////////////////////////////
	// NodeToBytes
	// BytesToNodes
	// SendNodes
	// RecieveNodes
	// TerminateDetection
	int id; // MPI_RANK
	int world_size; // = tnum
	unsigned int n_vars; // number of variables for a state. used to convert Node <-> bytes
	unsigned int s_var; // = sizeof(state_var_t)
	unsigned int node_size;
	std::vector<std::vector<char*> > outgo_buffer; // TODO: std::vector<char*> ?

	template<typename T>
	void typeToBytes(T& p, unsigned char* d) const;

	template<typename T>
	void bytesToType(T& p, unsigned char* d) const;

//	void node_to_bytes(SearchNode* n, unsigned char* d);
	void generate_node_as_bytes(SearchNode* parent_node, const Operator* op,
			unsigned char* d);
	StateID bytes_to_node(unsigned char* d);
	void bytes_to_nodes(std::vector<SearchNode>* ns, unsigned char* d,
			unsigned int d_size);
	void receive_nodes_from_queue();
	bool termination_detection(bool& has_sent_first_term);
	void flush_outgo_buffers(int threshold);

protected:
	int step();
	std::pair<SearchNode, bool> fetch_next_node();
	bool check_goal(const SearchNode &node);
	void update_jump_statistic(const SearchNode &node);
	void print_heuristic_values(const std::vector<int> &values) const;
	void reward_progress();

	std::vector<Heuristic *> heuristics;
	std::vector<Heuristic *> preferred_operator_heuristics;
	std::vector<Heuristic *> estimate_heuristics;
	// TODO: in the long term this
	// should disappear into the open list

	virtual void initialize();

public:
	HDAStarSearch(const Options &opts);
	void statistics() const;
	void search();
	void dump_search_space();
};

#endif
