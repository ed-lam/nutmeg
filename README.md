Nutmeg
======

Nutmeg is a solver that hybridizes mixed integer linear programming and constraint programming using the branch-and-cut style of logic-based Benders decomposition known as branch-and-check. Given a high-level constraint programming model, Nutmeg automatically derives a mixed integer programming master problem that omits difficult global constraints (i.e., those with a weak linear relaxation) and a constraint programming subproblem identical to the original model. At every node in the branch-and-bound search tree, the linear relaxation computes dual bounds and proposes solutions, which are checked for feasibility of the omitted constraints in the constraint programming subproblem. In the case of infeasibility, conflict analysis generates Benders cuts, which are appended to the linear relaxation to cut off the candidate solution.

Nutmeg serves as a valuable tool for novice modelers to try hybrid solving and for expert modelers to quickly compare different logic-based Benders decompositions of their problems. Nutmeg provides a low-level C++ interface and a high-level MiniZinc interface. The MiniZinc interface is recommended over the C++ interface.

**Nutmeg is released for research purposes and is perpetually in alpha status. No support is provided. Do not trust results from it; verify all results with another solver.**

If you use it, please cite the following papers:

- Branch-and-Check with Explanations for the Vehicle Routing Problem with Time Windows. Lam, E. and Van Hentenryck, P. CP2017.
- Automatic Logic-Based Benders Decomposition with MiniZinc. Davies, T. O., Gange, G. and Stuckey, P. J. AAAI2017.

License
-------

Nutmeg is released under the GPL version 3. See LICENSE.txt for further details.

Dependencies
------------

Nutmeg is implemented in C++17 and is built using CMake so you will need a recent compiler and a recent version of CMake. It is tested with Clang 10 on Mac.

Nutmeg calls SCIP for MIP and Geas for CP. Source code to SCIP is available free (as in beer) strictly for academic use. Nutmeg is tested with SCIP 6.0.2. SCIP is configured to use CPLEX to solve the LP relaxation but you can change it to SOPLEX if necessary. CPLEX is commercial software but has binaries available free under an academic license. Nutmeg is tested with CPLEX 12.10.

Compiling
---------

Download all the source code to Nutmeg and MiniZinc with Git:
```
git clone --recurse-submodules https://github.com/ed-lam/nutmeg-libminizinc.git
```

Download the [SCIP Optimization Suite 6.0.2](https://scip.zib.de). Extract it into `nutmeg-libminizinc/nutmeg`. You should find the subdirectory `nutmeg-libminizinc/nutmeg/scipoptsuite-6.0.2`.

[Download CPLEX 12.10](https://www.ibm.com/developerworks/community/blogs/jfp/entry/CPLEX_Is_Free_For_Students?lang=en). Locate the `cplex` subdirectory.

Compile Nutmeg and the MiniZinc interface using CMake:
```
cd nutmeg-libminizinc/nutmeg
mkdir build
cd build
cmake -DCPLEX_DIR={PATH TO cplex SUBDIRECTORY} ..
cmake --build . --target nutmeg
cd ../..
mkdir build
cd build
cmake -DCPLEX_DIR={PATH TO cplex SUBDIRECTORY} ..
cmake --build . --target minizinc
```

If you have a multi-core CPU with N cores, you can perform a parallel compile by running the following command instead, replacing N with the number of cores:
```
cmake --build . --target nutmeg -- -jN
cmake --build . --target minizinc -- -jN
```

Usage
-----

You can declare problems using the native C++ API or via MiniZinc. Examples for the C++ interface are provided in the `examples` directory.

To run a MiniZinc model:
```
./minizinc --solver nutmeg -a -s model.mzn data.dzn
```

Contributing
------------

We welcome code contributions and scientific discussion subject to [Monash University’s equal opportunity and harassment policies](https://www.monash.edu/about/diversity-inclusion/staff/equal-opportunity).
 
Authors
-------

Nutmeg is implemented by Edward Lam with assistance from Jip Dekker. Edward can be reached at [ed-lam.com](https://ed-lam.com).