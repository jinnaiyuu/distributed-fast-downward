Parallel best-first search algorithms for classical planning.

# Hash Distributed A*
This is the source code for Hash Distributed A* (HDA*) and other parallel algorithms for classical planning. The algorithms are described in the paper:

Jinnai Y, Fukunaga A. 2017. On Hash-Based Work Distribution Methods for Parallel Best-First Search. Journal of Artificial Intelligence Research (JAIR). arXiv: https://arxiv.org/abs/1706.03254.

Domain specific solvers (15-puzzle, 24-puzzle, multiple sequence alignment, and grid pathfinding) are available here (https://github.com/jinnaiyuu/Parallel-Best-First-Searches).

# Dependencies

The code is built on top of fast-downward (http://www.fast-downward.org/) of Febuary 2014 (http://hg.fast-downward.org/shortlog/8532ca08bcac).
To obtain this version of fast-downward run

```
hg clone http://hg.fast-downward.org DIRNAME
hg checkout issue412
```

Download fast-downward and replace the src/search directory of fast-downward by this repository. Please read the instruction for fast-downward to learn the syntax (http://www.fast-downward.org/PlannerUsage). Note that you need to modify some part of the code if you are to use the newest version of the fast-downward.

To run, you need to install MPI library. We have confirmed that our code works with MPICH3, MPICH2, and OpenMPI (usually MPICH is faster than OpenMPI). MPICH2 and OpenMPI are in most of the package managers. For example in Debian/Ubuntu,

```
sudo apt-get install mpich2
```

You may also need to install libcr-dev to run MPI.

The other libraries are optional. We used jemalloc library (http://jemalloc.net/) for memory management (can disable by removing -ljemalloc in makefile). We recommend mpiP (http://mpip.sourceforge.net/) for profiling MPI programs to understand the bottleneck of parallel algorithms.

# Usage

The syntax is same as fast-downward. You can run Hash Distributed A* (HDA*) by 

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,zobrist)" 4
```

First parameter of hdastar is a heuristic function.
Second parameter is "dist" which selects a method for work distribution (hashing function).
The number you place on the last is the number of processors to run HDA*.

Work distribution methods:

ZHDA* (Kihimoto et al 2009)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,zobrist)" 4
```

AHDA* (Burns et al 2010)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,abstraction(10000))" 4
```

DAHDA* (Jinnai&Fukunaga 2016)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,aabstraction(0.7))" 4
```

GAZHDA* (Jinnai&Fukunaga 2016)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,freq\_depend(1.0,0.0))" 4
```

GRAZHDA*/sparsity (Jinnai&Fukunaga 2017)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,freq\_depend(cut=sparsest\_cut))" 4
```

FAZHDA* (Jinnai&Fukunaga 2016)

```
./src/plan ./pddl/blocks-4-0.pddl --search "hdastar(merge\_and\_shrink,freq\_depend(0.5,0.0))" 4
```


To run MPI algorithm using job schedulers (torque, SunGridEngine, etc.) you need to learn the syntax of each scheduler. ./src/pbs-plan works on torque but may not be compatible with other schedulers.


# Author

Yuu Jinnai <ddyuudd@gmail.com>
Please let me know if you wish to use my code but find my document unhelpful.
I am trying to make this program easy to use for everybody so I appreciate your comments.

# LICENSE

The code is published under GPL ver 3.

