# Parallel Fast Downward

Parallel fast-downward is a classical planner for distributed environments.
It extends the state-of-the-art planner fast-downward (http://www.fast-downward.org/) to distributed computers.
Parallel fast-downward implements the state-of-the-art parallel best-first search algorithms including Abstract Zobrist hashing (AZH) and Hash Distributed A\* (HDA\*).

# Hash Distributed A*
This is the source code for Hash Distributed A* (HDA*) and other parallel algorithms for classical planning. The algorithms are described in the paper:

Jinnai Y, Fukunaga A. 2017. On Hash-Based Work Distribution Methods for Parallel Best-First Search. Journal of Artificial Intelligence Research (JAIR). arXiv: https://arxiv.org/abs/1706.03254.

Domain specific solvers (15-puzzle, 24-puzzle, multiple sequence alignment, and grid pathfinding) are available here (https://github.com/jinnaiyuu/Parallel-Best-First-Searches).

# Dependencies

The code is built on top of fast-downward (http://www.fast-downward.org/) of Febuary 2014 (http://hg.fast-downward.org/shortlog/8532ca08bcac).
Please read the instruction for fast-downward to learn the syntax (http://www.fast-downward.org/PlannerUsage). Note that you need to modify some part of the code if you want to integrate parallel searches to the newest version of the fast-downward.

To run, you need to install MPI library. We have confirmed that our code works with MPICH3, MPICH2, and OpenMPI (usually MPICH is faster than OpenMPI). MPICH2 and OpenMPI are in most of the package managers. For example in Debian/Ubuntu,

```
sudo apt-get install mpich2
```

You may also need to install libcr-dev to run MPI.

The other libraries are optional. We recommend mpiP (http://mpip.sourceforge.net/) for profiling MPI programs to understand the bottleneck of parallel algorithms.

# Usage

The syntax is same as fast-downward. You can run Hash Distributed A* (HDA*) by 

```
./src/plan PDDLFILE --search "hdastar(HEURISTIC,HASH-FUNCTION)" NUMPROCESSES
```

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,zobrist)" 4
```


First parameter of hdastar is a heuristic function.
Second parameter is "dist" which selects a method for work distribution (hashing function).
The number you place on the last is the number of processors to run HDA*.

Work distribution methods:

GRAZHDA*/sparsity (Jinnai&Fukunaga 2017)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,freq_depend(cut=sparsest_cut))" 4
```

DAHDA* (Jinnai&Fukunaga 2016)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,aabstraction(0.7))" 4
```

GAZHDA* (Jinnai&Fukunaga 2016)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,freq_depend(1.0,0.0))" 4
```

FAZHDA* (Jinnai&Fukunaga 2016)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,freq_depend(0.5,0.0))" 4
```

ZHDA* (Kihimoto et al 2009)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,zobrist)" 4
```

AHDA* (Burns et al 2010)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge_and_shrink,abstraction(10000))" 4
```


To run MPI algorithm using torque, ./src/pbs-plan should work. To run it in other schedulers, you probably need to edit the parameters put in mpiexec ($PBS_NODEFILE and $PBS_NUM_PPN) to appropriate variables.


# Memo

In our experiments GRAZHDA*/sparsity was the best performing algorithms for merge&shrink heuristic (low computation cost) and lmcut (high computation cost) on single-machine multicore environment (8 cores), commodity cluster (48 cores), and cloud cluster on EC2 (128 virtual core).
Thus we expect GRAZHDA*/sparsity to be the best for most of the modern computer systems.
I am interested to see the results in other environments (e.g. a heterogeneous environment consists of multiple types of machines, CPUs).

# Author

Yuu Jinnai <ddyuudd@gmail.com> implemented parallel algorithms on top of the fast-downward (http://www.fast-downward.org/).
Please let me know if you wish to use my code but find my document unhelpful.
I am trying to make this program easy to use for everybody so I appreciate your comments.

# LICENSE

The code is published under GPL ver 3.

