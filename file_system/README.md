# Filesystem

Implemented a FUSE filesystem driver that let's a 1MB disk image be mounted as a filesystem with the below functionality:

- Create files.
- List the files in the filesystem root directory (where you mounted it).
- Write to small files (under 4k).
- Read from small files (under 4k).
- Delete files.
- Rename files.
- Create hard links - multiple names for the same file blocks.
- Read and write from files larger than one block. For example, supports fifty 1k files or five 100k files.
- Create directories and nested directories.
- Remove directories.
- Support metadata (permissions and timestamps) for files and directories.

## Design

Inspired by the `ext` filesystem. Used 2 bitmaps for inodes and data blocks, 2 chunks of memory dedicated as array of inodes and array of data blocks. There is also an entry data structure, which keeps track of a file/directory's name and its
corresponding inode number.

## Usage

Install `libfuse` and `make mount` to mount filesystem. Run `make test`  to run filesystem correctness tests.