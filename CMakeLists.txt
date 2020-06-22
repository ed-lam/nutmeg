cmake_minimum_required(VERSION 3.8)
project(Nutmeg)

# Set C version.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED 1)

# Set C++ version.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

# Set release build by default.
set(DEFAULT_BUILD_TYPE "Release")

# Include CPLEX.
set(LPS cpx CACHE STRING "options for LP solver")
find_path(CPLEX_INCLUDE_DIR
        NAMES cplex.h
        HINTS "${CPLEX_DIR}/include/ilcplex" "$ENV{CPLEX_DIR}/include/ilcplex")
find_library(CPLEX_LIBRARY
        NAMES cplex
        HINTS "${CPLEX_DIR}/lib/*/static_pic" "$ENV{CPLEX_DIR}/lib/*/static_pic")
if (NOT CPLEX_INCLUDE_DIR)
    message(FATAL_ERROR "CPLEX includes not found")
endif ()
if (NOT CPLEX_LIBRARY)
    message(FATAL_ERROR "CPLEX library not found")
endif ()
include_directories(${CPLEX_INCLUDE_DIR})

# Set stuff for SCIP.
option(SHARED "Build shared libraries" off)
set(WITH_SCIPDEF on)
set(TPI none)
set(tpisources tpi/tpi_none.c)
set(THREAD_LIBRARIES "")
set(TPI_NONE on)
configure_file(scipoptsuite-6.0.2/scip/src/scip/config.h.in ${CMAKE_BINARY_DIR}/scip/config.h @ONLY)
include_directories(${CMAKE_BINARY_DIR})
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ldl")
endif ()
option(ZLIB "should zlib be linked" OFF)
option(READLINE "should readline be linked" OFF)
option(GMP "should gmp be linked" OFF)

# Include SCIP.
add_subdirectory(scipoptsuite-6.0.2/scip/ EXCLUDE_FROM_ALL)
include_directories(SYSTEM scipoptsuite-6.0.2/scip/src/)

# Include geas.
add_subdirectory(geas EXCLUDE_FROM_ALL)
include_directories(SYSTEM geas/include)

# Include fmt.
add_subdirectory(fmt EXCLUDE_FROM_ALL)
include_directories(SYSTEM fmt/include)

# Create Nutmeg target.
include_directories(.)
set(NUTMEG_FILES
        Nutmeg/Nutmeg.h
        Nutmeg/Includes.h
        Nutmeg/Debug.h
        Nutmeg/ProblemData.h
        Nutmeg/ProblemData.cpp
        Nutmeg/Solution.h
        Nutmeg/Variable.h
        Nutmeg/Variable.cpp
        Nutmeg/Model.h
        Nutmeg/Model.cpp
        Nutmeg/Model-Variables.cpp
        Nutmeg/Model-UncheckedConstraints.cpp
        Nutmeg/Model-Constraints.cpp
        Nutmeg/Model-SolveBC.cpp
        Nutmeg/Model-SolveLBBD.cpp
        Nutmeg/Model-SolveMIP.cpp
        Nutmeg/Model-SolveCP.cpp
        Nutmeg/ConstraintHandler-Geas.h
        Nutmeg/ConstraintHandler-Geas.cpp
        Nutmeg/EventHandler-NewSolution.h
        Nutmeg/EventHandler-NewSolution.cpp
        )
add_library(nutmeg STATIC ${NUTMEG_FILES})
target_link_libraries(nutmeg fmt::fmt-header-only geas libscip)

# Capacity- and distance-constrained plant location problem
add_executable(cdcplp
        ${NUTMEG_FILES}
        examples/cdcplp/InstanceData.h
        examples/cdcplp/InstanceData.cpp
        examples/cdcplp/cdcplp.cpp)
target_include_directories(cdcplp PRIVATE examples/cdcplp)
target_link_libraries(cdcplp fmt::fmt-header-only geas libscip)

# Planning and scheduling - cost objective function (1)
add_executable(ps_cost
        ${NUTMEG_FILES}
        examples/ps/InstanceData.h
        examples/ps/InstanceData.cpp
        examples/ps/ps_cost.cpp)
target_include_directories(ps_cost PRIVATE examples/ps_cost)
target_link_libraries(ps_cost fmt::fmt-header-only geas libscip)

# Planning and scheduling - cost objective function (2)
add_executable(ps_cost2
        ${NUTMEG_FILES}
        examples/ps/InstanceData.h
        examples/ps/InstanceData.cpp
        examples/ps/ps_cost2.cpp)
target_include_directories(ps_cost2 PRIVATE examples/ps_cost2)
target_link_libraries(ps_cost2 fmt::fmt-header-only geas libscip)

# Planning and scheduling - cost objective function (3)
add_executable(ps_cost3
        ${NUTMEG_FILES}
        examples/ps/InstanceData.h
        examples/ps/InstanceData.cpp
        examples/ps/ps_cost3.cpp)
target_include_directories(ps_cost3 PRIVATE examples/ps_cost3)
target_link_libraries(ps_cost3 fmt::fmt-header-only geas libscip)

# Planning and scheduling - makespan objective function
add_executable(ps_makespan
        ${NUTMEG_FILES}
        examples/ps/InstanceData.h
        examples/ps/InstanceData.cpp
        examples/ps/ps_makespan.cpp)
target_include_directories(ps_makespan PRIVATE examples/ps_makespan)
target_link_libraries(ps_makespan fmt::fmt-header-only geas libscip)

# Vehicle routing problem with location congestion - cost objective function
add_executable(vrplc_cost
        ${NUTMEG_FILES}
        examples/vrplc/InstanceData.h
        examples/vrplc/InstanceData.cpp
        examples/vrplc/vrplc_cost.cpp)
target_include_directories(vrplc_cost PRIVATE examples/vrplc)
target_link_libraries(vrplc_cost fmt::fmt-header-only geas libscip)

# Vehicle routing problem with location congestion - makespan objective function
add_executable(vrplc_makespan
        ${NUTMEG_FILES}
        examples/vrplc/InstanceData.h
        examples/vrplc/InstanceData.cpp
        examples/vrplc/vrplc_makespan.cpp)
target_include_directories(vrplc_makespan PRIVATE examples/vrplc)
target_link_libraries(vrplc_makespan fmt::fmt-header-only geas libscip)

# Turn on link-time optimization for Linux.
#if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
#    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
#endif ()

# Set flags.
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSOLVE_USING_BC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCHECK_AT_LP")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CUT_MINIMIZATION")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -m64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -funroll-loops")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE}")
