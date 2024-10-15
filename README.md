# Overview

Libmem is a C library for managing the state of memory blocks in a Linux 
system. This library provides a user or orchestration application the ability 
to online / offline individual memory blocks of the system or specific blocks 
within a CXL region. 

The library consists of a `libmem.h` header file and a `libmem.a` library archive. 
The library also builds a CLI tool `mem` that allows the user user to 
view and manipulate the state of system memory blocks from the command line.

# Dependencies 

To compile, the library requires the following libraries to be installed in 
addition to typical packages to build C programs (e.g. `make`)

```
daxctl uuid cxl
```

The `uuid` library can be installed from the system package manager:

```bash
apt install uuid-dev
```

The `daxctl` and `libcxl` libraries included with current Linux distributions are 
typically too old to support the features needed. Consequently, the user is 
required to build the current version of the `ndctl` project and install the 
manually built `daxctl` and `cxl` libraries. 

See [ndctl](https://github.com/pmem/ndctl) on build and installation 
instructions.  

attention: `libdax` `libcxl` and `libndctl` already in `lib/`

# Build 

To build simply type:

```bash
cd build

cmake ..

make
```

<del>
To install to `/usr/local/*` locations type:

```bash
make installed
```
</del>

# CLI Usage

The `mem` CLI tool can be used to display system memory block information:

```bash 
mem show block

mem info
```

To display the state of memory blocks in the system: 

```bash
mem list
```

To online system memory block 306 

```bash 
mem block online 306 
```

To offline a system memory block 

```bash 
mem block offline 306
```

To online memory block 2 of a CXL region: 

```bash 
mem block online 2 region0 
```

To offline memory block 2 of a CXL region: 

```bash 
mem block offline 2 region0 
```


