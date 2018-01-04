# Parallel QuickSort

## Expectation

Input an array of N items and an integer P for number of threads to use. Output an array of N items, sorted.

## Design

*1. Setup*
- Read input file into a "floats" in memory.
- Allocate an array, sizes, of P longs.
- Use open / ftruncate to create the output file, same size as the input file.

*2. Sample*
- Select 3*(P-1) items from the array.
- Sort those items.
- Take the median of each group of three in the sorted array, producing an array (samples) of (P-1) items.
- Add 0 at the start and +inf at the end (or the min and max values of the type being sorted) of the samples array so 
it has (P+1) items numbered (0 to P).

*3. Partition*
- Spawn P threads, numbered p in (0 to P-1).
- Each thread builds a local array of items to be sorted by scanning the
   full input and taking items between samples[p] and samples[p+1].
- Write the local size to sizes[p].

*4. Sort locally*
- Each thread uses quicksort to sort the local array.

*5. Write the data out to the file.*
- Open a separate file descriptor to the output file in each thread. 
- Use lseek to move to the correct location to write in the file.

*6. Cleanup*
- Terminate the P subthreads.

## Usage

The program `tssort` takes three arguments:

- An integer specifying the number of threads to sort with

- The input file
- The output file

Run `make test` to build and run tests.

### Example session

```c
$ ./tools/gen-input 20 data.dat
$ ./ssort 4 data.dat
0: start 0.0000, count 3
1: start 7.5690, count 5
2: start 27.1280, count 11
3: start 95.5110, count 1
$ ./tools/check-sorted data.dat
$
```