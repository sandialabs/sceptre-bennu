[![Build Status](https://travis-ci.com/GMLC-TDC/containers.svg?branch=master)](https://travis-ci.com/GMLC-TDC/containers)
[![Build Status](https://dev.azure.com/phlptp/containers/_apis/build/status/GMLC-TDC.containers?branchName=master)](https://dev.azure.com/phlptp/containers/_build/latest?definitionId=2&branchName=master)
[![CircleCI](https://circleci.com/gh/GMLC-TDC/containers/tree/master.svg?style=svg)](https://circleci.com/gh/GMLC-TDC/containers/tree/master)
[![codecov](https://codecov.io/gh/GMLC-TDC/containers/branch/master/graph/badge.svg)](https://codecov.io/gh/GMLC-TDC/containers)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/357c0c3dfea243079af3e3a8faedea57)](https://www.codacy.com/app/GMLC-TDC/containers?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=GMLC-TDC/containers&amp;utm_campaign=Badge_Grade)
[![](https://img.shields.io/badge/License-BSD-blue.svg)](https://github.com/GMLC-TDC/containers/blob/master/LICENSE)

# Containers
A header only library consisting of a Collection of containers used inside [HELICS](https://github.com/GMLC-TDC/HELICS) and supporting repos.  These containers are used for data storage for different purposes inside the code.  In some cases they were mage more general than needed to support other types or be useful elsewhere.

## General Containers

### StackBuffer
A raw data stack taking blocks of raw memory in a LIFO queue.  It works on chunks of memory with push and pop functions.  It also has a reverse function to reverse the extraction order.  There are two classes an underlying StackBufferRaw that works on a block of memory.  And a StackBuffer class which handles some memory operations and resizing.  Unlike Vector it does not automatically resize, but it does have a method to handle resizing manually.  Push functions return a bool if they were successful.  

### CircularBuffer
A raw data FIFO queue as a circular buffer.   The buffer and element indexing is generated in a raw data block.  The underlying class itself consists of 3 pointers and a capacity value.  A wrapper class around the raw stack can manage memory, whereas the raw CircularBuffer operates on a given raw memory block.  

### StableBlockVector
The stable block vector works like std::vector with a few exceptions.  The memory is not contiguous but is instead allocated in specific block sizes which contain a specified number of the elements.  The intention is to have a stable growth pattern and maintain reference and pointer stability on any elements.  Similar to deque but with no push/pop front.  It does not allow insertion or removal of elements except at the end.   This means any elements are never moved by the Class itself once the initial creation has occurred.  It can take a custom allocator and might be ideal to have a block allocator.  It has begin and end() and works with range based for loops.  The iterators are random access so can be used with various algorithms from the standard library.  

### StableBlockDeque
Very similar to stableBlockVector but has push/pop front and can grow and contract on both sides.   But is very similar in concept to std::deque.  The memory blocks are reused once allocated, but can be freed with the `shink_to_fit()` method.  

## Mapped Vector containers
All of these types are basically a vector(or vector like construct) along with a searchable map of some kind.  The map contains an index and additional terms may be added to existing elements.  So there is not a 1 to 1 map from search terms to objects.   The map is either a std::map or unordered_map if the search type can be hashed easily.  see MapTraits.  This may change to use one of the other better performing hash maps in the future.  It can take an additional template argument to allow reference stability of the objects so may use StableBlockVector in the future

### MappedVector
Just a single element with the objects stored directly

### MappedPointerVector
Similar to MappedVector but a vector of unique_ptrs of the element data type.  This keeps pointer stability with all operations.    

### DualMappedVector
Similar to MappedVector but with two search types. One or both search terms may be omitted for any given element.  

### DualMappedPointerVector
Similar to MappedPointerVector but with two search terms.

### MapTraits  
Not a container but some typetraits that help in the construction of the containers.  

### MapOps
Some Common operations on a map

## ThreadSafe Queues
These containers are intended for the purpose of transferring data between threads.  The general idea is that the containers have two vectors in them one for the push data and one for the pull data. So producers and consumer threads interact less often.  This is generally better performant than a simple queue protected with a mutex.  The mutex type and condition variable for the blocking queues is a template parameter.   The characteristics are mostly independent but occasional longer blocking periods as the vectors swap and reverse.  So if you need stable insert and pop times this may  not be ideal.  These are NOT lock-free data types they most definitely use locks.  

### SimpleQueue
A simple thread safe queue with no blocking,  uses an optional data type for pop methods

### BlockingQueue
A variation on the SimpleQueue that can wait using a condition variable for an element to be inserted into the queue.  

### BlockingPriorityQueue
Add a priority channel to the BlockingQueue so data can be inserted at high or normal priority. (only two modes).  The priority data is handled in a separate structure with different methods for emplacement and pushing.  But extraction is identical.  

## other

### AirLock

A threadsafe container for transferring a single object between thread.  Its intention is to transfer objects between threads in an asynchronous fashion.  Sort of like a reusable promise/future object,  or a single element blockingQueue.  The one interesting use case was to store a std::any object in a mailbox that was opened as a message went through processing.  The message contained the location and type information, while the Airlock held the actual data waiting for the message to be delivered.  

### WorkQueue

A threaded WorkQueue using a set of 3 SimpleQueue object.  work blocks are added with a priority high/medium/low.   High is executed first,  medium and low are rotated with a priority ratio  N medium block for each low block, if both are full.  

## Release
The GMLC-TDC containers library is distributed under the terms of the BSD-3 clause license. All new
contributions must be made under this license. [LICENSE](LICENSE)

SPDX-License-Identifier: BSD-3-Clause
