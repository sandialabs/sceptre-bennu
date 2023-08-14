[![Build Status](https://travis-ci.com/GMLC-TDC/concurrency.svg?branch=master)](https://travis-ci.com/GMLC-TDC/concurrency)
[![Build Status](https://dev.azure.com/phlptp/concurrency/_apis/build/status/GMLC-TDC.concurrency?branchName=master)](https://dev.azure.com/phlptp/concurrency/_build/latest?definitionId=2&branchName=master)
[![codecov](https://codecov.io/gh/GMLC-TDC/concurrency/branch/master/graph/badge.svg)](https://codecov.io/gh/GMLC-TDC/concurrency)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/357c0c3dfea243079af3e3a8faedea57)](https://www.codacy.com/app/GMLC-TDC/concurrency?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=GMLC-TDC/concurrency&amp;utm_campaign=Badge_Grade)
[![](https://img.shields.io/badge/License-BSD-blue.svg)](https://github.com/GMLC-TDC/concurrency/blob/master/LICENSE)

# Concurrency
The concurrency library is a header only Collection of data structures used inside [HELICS](https://github.com/GMLC-TDC/HELICS) and supporting repos to support thread synchronization and concurrency.
A significant chunk of the library is based on [libGuarded](https://github.com/copperspice/libguarded)

## General Structures

### TriggerVariable
A wrapper around a condition variable to allow waiting and testing of trigger which can be activated, triggered, and reset.  

### DelayedObject
A container holding a set of promises that can be used for storing an index of future values allowing access by string or index instead of the future and promise classes.

### DelayedDestructor
A container which holds shared pointers of objects so they can be destroyed at a later time that is more convenient or from a particular thread only.  Essentially a modular garbage collector.  

### SearchableObjectHolder
A container to hold shared pointers to object so they can be searched and retrieved later if necessary by name.

### TripWire
A set of classes class to detect that a scope has closed in an independent location in a thread safe fashion.  The main use case so far is detecting that a program is being closed by the OS from different threads, such that in that case a simpler closeout procedure is performed.  

### Barrier
A barrier class which does the typical things of a barrier roughly based on the C++20 standard version

### Latch
A latch class which does the typical things of a latch

## [libGuarded](gmlc/libguarded/README.md)
The main modifications to libGuarded were the use of a dedicated handles class to allow for a wider assortment of locks to be used and other convenient operations to be used on the handles.  A few additional classes were added including
-   atomic_guarded  which is useful for wrapping things which are assigned but may allocate, so can't be used in a regular atomic like std::string or std::function and a few other things of that nature.
-   staged_guarded  class which operates like a guarded during an initialization phase, then transitions to const usage only
-   guarded_opt    similar to guarded but has a construction time boolean that can disable the locking if needed if it was known to only be used in a single thread context.  
-   shared_guarded_opt  same as guarded_opt but on a shared_guarded object 

## Release
GMLC-TDC/Concurrency library is distributed under the terms of the BSD-3 clause license. All new
contributions must be made under this license. [LICENSE](LICENSE)

SPDX-License-Identifier: BSD-3-Clause

portions of the code written by LLNL with release number
LLNL-CODE-739319
