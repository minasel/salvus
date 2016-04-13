# salvus
[![Build Status](https://travis-ci.org/SalvusHub/salvus.svg?branch=master)](https://travis-ci.org/SalvusHub/salvus)

Ask not what your spectral element wave propagator can do for you, but
what you can do for your spectral element wave propagator.

## Dependencies


1. You need at least g++ 4.8 (for C++11 features)

    ```bash
    brew install gcc48         # OSX
    ```
    and on Ubuntu 12.04

    ``` bash
    sudo apt-get install gcc-4.8 g++-4.8
    ```

2. CMake (https://cmake.org)

    ```bash
    brew install cmake         # OSX
    ```
    and on Ubuntu 12.04

    ``` bash
    sudo apt-get install cmake
    ```

3. Eigen (https://eigen.tuxfamily.org)\
    You will need to install version 3.x of the     `eigen` library

    ```bash
    brew install eigen         # OSX
    ```
    and on Ubuntu 12.04
    
    ``` bash
    sudo apt-get install libeigen3-dev
    ```

4. MPI (Message Passing Interface)\
    We recommend the MPICH implementation (https://www.mpich.org)

5. PETSc (https://www.mcs.anl.gov/petsc)\
    You need at least version 3.6.x. Download it from http://www.mcs.anl.gov/petsc/download, unpack it, and install it with all the required libraries. Adjust the `prefix` to where you want it installed.

    If you do not have MPI installed on your machine, PETSc can install it for you
    ``` bash
    ./configure --prefix=/opt/petsc --with-cc=gcc-4.8 --with-cxx=g++-4.8 --download-mpich=yes --download-exodusii --download-netcdf --download-hdf5 --download-chaco
    ```
    If you have a working MPI implementation (e.g. located in /path/to/mpi), then configure like this
    
    ``` bash
    ./configure --prefix=/opt/petsc --with-cc=/path/to/mpicc --with-cxx=/path/to/mpicxx --with-mpi-dir=/path/to/mpi --download-exodusii --download-netcdf --download-hdf5 --download-chaco
    ```
    At the end of each command it tells you to run some other command. Do that
until it is done with everything.


## Installation

It is a good idea to do an "out of source build" to keep the `salvus`
directory clean of build artifacts.

``` bash
mkdir build
cd build
```

Several library variables will need setting in order to successfully
compile. From you build directory, you can directly set the
`PETSC_DIR`, and `EIGEN_INCLUDE` directories via `ccmake ../`. Hit the
`c` key to "configure", make your changes, and hit `g` to generate the
Makefiles. Alternatively, you can achieve this via `cmake` on the
command line via

``` bash
CC=/opt/petsc/bin/mpicc CXX=/opt/petsc/bin/mpicxx cmake ../ -DPETSC_DIR=/opt/petsc -DEIGEN_INCLUDE=/usr/include/eigen3
```

Note the usage of `CC=gcc-4.8` `CXX=g++-4.8`, which is used to change the
default compiler used. This only works the **first** time `cmake` is
run (it gets cached). `cmake` manages the linking to mpi includes and
libraries itself, so no need to use a wrapper such as `mpicc` or
`mpic++`.

By default, Salvus is compiled with optimizations (i.e., a release
build). To compile for debugging (which adds `-g` and removes `-O3`),
add `-DCMAKE_BUILD_TYPE=Debug` (instead of `Release`) to the **first**
run of `cmake ../`.

Finally compile `salvus` with

```bash
$ make -j4
```

### Running it

We aim to collect a number of examples including all required data here: https://github.com/SalvusHub/salvus_data
