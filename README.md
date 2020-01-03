Nutmeg
======

Nutmeg is a solver that hybridizes mixed integer programming and constraint programming using branch-and-check. Given a constraint programming model, Nutmeg automatically derives a mixed integer programming relaxation that omits difficult constraints, and solves both problems side-by-side in a single branch-and-bound search tree. A linear relaxation computes dual bounds and proposes solutions, which are checked for feasibility of the omitted constraints in the full constraint programming model. In the case of infeasibility, conflict analysis generates Benders cuts, which are appended to the linear relaxation to cut off the candidate solution.

**Nutmeg is released for research purposes and is perpetually in alpha status. No support is provided. Do not trust results from it; verify all results with another solver.**

If you use it, please cite the following papers:

- Branch-and-Check with Explanations for the Vehicle Routing Problem with Time Windows. Lam, E. and Van Hentenryck, P. CP2017.
- Automatic Logic-Based Benders Decomposition with MiniZinc. Davies, T. O., Gange, G. and Stuckey, P. J. AAAI2017.

License
-------

Nutmeg is released under the GPL version 3. See LICENSE.txt for further details.

Dependencies
------------

Nutmeg is implemented in C++17 and is built using CMake so you will need a recent compiler and a recent version of CMake. It is tested with Clang 9 on Mac.

Nutmeg calls SCIP for MIP and Geas for CP. Source code to SCIP is available free (as in beer) strictly for academic use. Nutmeg is tested with SCIP 6.0.2. Download the [SCIP Optimization Suite 6.0.2](https://scip.zib.de) and extract it into the root of this repository. You should find the subdirectory `scipoptsuite-6.0.2/scip/src`.

SCIP is configured to use CPLEX to solve the LP relaxation but you can change it to SOPLEX if necessary. CPLEX is commercial software but has binaries available free under an [academic license](https://www.ibm.com/developerworks/community/blogs/jfp/entry/CPLEX_Is_Free_For_Students?lang=en). Nutmeg is tested with CPLEX 12.9. You should find the subdirectory `cplex`.

Compiling
---------

Download the source code by cloning this Git repository and all its submodules:
```
git clone --recurse-submodules https://github.com/ed-lam/nutmeg.git
```

Compile Nutmeg using CMake:
```
cd nutmeg
mkdir build
cd build
cmake -DCPLEX_DIR={PATH TO cplex SUBDIRECTORY} ..
cmake --build . --target nutmeg
```

If you use a custom compiler, you will need to tell CMake where the compiler is:
```
cmake -DCPLEX_DIR={PATH TO cplex SUBDIRECTORY} -DCMAKE_C_COMPILER={PATH TO C COMPILER} -DCMAKE_CXX_COMPILER={PATH TO C++ COMPILER} ..
```

If you have a multi-core CPU with N cores, you can perform a parallel compile by running the following command instead, replacing N with the number of cores:
```
cmake --build . --target nutmeg -j N
```

The MiniZinc interface needs to be compiled separately:
```
cd ../libminizinc
mkdir build
cd build
cmake -DCPLEX_DIR={PATH TO cplex SUBDIRECTORY} ..
cmake --build . --target minizinc
```

Usage
-----

You can declare problems using the native C++ API or via MiniZinc. Examples for the C++ interface are provided in the `examples` directory.

The MiniZinc interface is extremely limited and is highly experimental. To run a MiniZinc model:
```
./minizinc --solver nutmeg --stdlib-dir ../share/minizinc/ model.mzn data.dzn
```

Contributing
------------

We welcome code contributions and scientific discussion. Please contact us to discuss your ideas prior to submitting a pull request. We will engage in scientific discourse about Nutmeg subject to [Monash University’s equal opportunity and harassment policies](https://www.monash.edu/about/diversity-inclusion/staff/equal-opportunity).
 
Authors
-------

Nutmeg is implemented by Edward Lam with assistance from Jip Dekker. Edward can be reached at [ed-lam.com](https://ed-lam.com).