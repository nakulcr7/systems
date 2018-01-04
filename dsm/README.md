# Distributed Shared Memory

Implemented Distributed Shared Memory in Userspace Library for Linux Applications. Modeled after the [Ivy](http://css.csail.mit.edu/6.824/2014/papers/li-dsm.pdf) system. Final project for [CS5600 Computer Systems](http://www.ccs.neu.edu/home/kapil/courses/cs5600f17/).

## Usage

Tested on Ubuntu 14.04 x86/64.

- Install dependencies: `sudo apt-get install libb64-dev`
- `cd src`
- `make`
- Build `cd src && make`
- Run the manager `python manager/manager.py &`

## Matrix multiplication benchmark

`matrixmultiply` uses the alternation-style sharding algorithm for nodes-to-rows
mapping, while matrixmultiply2 allocates rows to nodes in contiguous chunks to
reduce the effects of page thrashing. They are both launched the same way.

First, start the manager:

`$ python manager/manager.py &`

Next, start up instances of matrixultiply in parallel. To start three
instances, launch them like this:

```bash
$ cd src
$ make
$ ./matrixmultiply2 127.0.0.1 4444 1 3 && \
  ./matrixmultiply2 127.0.0.1 4444 2 3 && \
  ./matrixmultiply2 127.0.0.1 4444 3 3:
```

## Collaborators

- Rashmi Dwaraka
- Abhay Kasturia