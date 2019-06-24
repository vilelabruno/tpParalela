cmake_minimum_required(VERSION 3.1)
project(tpParalela C)
set(CMAKE_C_STANDARD 99)

find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})
set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpicxx)

find_package(OpenMP REQUIRED)
set(CMAKE_C_FLAGS ${OpenMP_C_FLAGS})

find_package(PNG REQUIRED)
include_directories(${LIBPNG_INCLUDE_DIRS})
link_directories(${LIBPNG_LIBRARY_DIRS})
link_libraries(${LIBPNG_LIBRARIES})

include_directories(./include)

add_executable(tpParalela main.c q.c q.h count.c count.h lk.h lk.c)
target_link_libraries(tpParalela ${PNG_LIBRARY})
target_link_libraries(tpParalela m)
