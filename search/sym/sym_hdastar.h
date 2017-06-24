#ifndef SYM_HDASTAR_H
#define SYM_HDASTAR_H

#include "sym_engine.h"
#include "../hash/distribution_hash.h"
#include <set>
#include <mpi.h>
class SymExploration;
class SymHNode;

class SymHDAStar: public SymEngine {


	// If g_timer() > t_orig => force to search on original state space.
	// A hack to avoid wasting the last remaining time in abstract state space searches
	double t_orig;
	int currentPH;

	/**
	 * MPI Related functions and variables
	 *
	 */
	int probe(); // Retrieve messages
	BDD receive_bdd(MPI_Status& status);
	void put_in_open(BDD msg);
	int send();  // Send messages
	int send_bdd(BDD bdd, int dist);
	int parition(SymExploration* search);
	int mpi_initialize(const Options &opts);
	bool do_probe(); // only run Iprobe every X times;
	int stepReturn() const;
	int terminate_detection() const;
	int id; // MPI_RANK
	int world_size; // = tnum
	unsigned int incumbent; // incumbent goal cost
	std::vector<std::vector<unsigned char> > outgo_buffer;
	DistributionHash* hash;
	unsigned int node_sent;
	unsigned int msg_sent;
	unsigned int term_msg_sent;
	unsigned int termination_counter;
	bool has_sent_first_term;
	unsigned int income_counter;
	unsigned int incumbent_counter;
	unsigned char* mpi_buffer;
	std::pair<unsigned int,int> incumbent_goal_state; // goal state and its cost

	int probe_threshold;
	int probe_counter;

	//Parameters to control how much to relax the search => moved to PH
	//int maxRelaxTime, maxRelaxNodes; // maximum allowed nodes to relax the search
	//int maxAfterRelaxTime, maxAfterRelaxNodes; // maximum allowed nodes to accept the abstraction after relaxing the search
	//double ratioRelaxTime, ratioRelaxNodes;
	//double ratioAfterRelaxTime, ratioAfterRelaxNodes;
	// Proportion of time in an abstract state space wrt the original state space.
	// If ratio is 0.5 then it is better to explore original with 10 seconds than abstract with 5.1 seconds
	//double ratioAbstract;
	//Percentage of nodes that can potentially prune in the frontier for an heuristic to be useful
	// double percentageUseful;
	//Other parameters to actually prove that their default values are the right ones :-)
	// bool forceHeuristic; //always forces heuristic computation
	//bool heuristicAbstract;  //If abstract state spaces are allowed to use others as heuristic

	inline bool forceOriginal() const {
		return g_timer() > t_orig;
	}

	//Functions that determine the criterion
	//bool canExplore(const SymExploration & exp);

	SymExploration * selectExploration();


public:
	SymHDAStar(const Options &opts);
	virtual ~SymHDAStar() {
	}

	virtual void initialize();
	virtual int step();

	virtual void print_options() const;
};

#endif
