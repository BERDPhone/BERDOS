# Style Guide
This document contains the requirements for code to submitted to this repository.

## Lines of code per file
No hard limit, however keep it around 500 or less, and more, and we may ask that you refactor the code. We know this is an arbitrary limit, and that presumably justifiable reasons to not enforce this to increase readability.

## Variable/function naming conventions
Every variable will be defined with snake case. Additionally two underscores are to be used by functions, which are not meant to be exposed to other files via headers. For example in a file like `kernel.c`, there could be two functions defined named `pico_create_task` and `__piccolo_task_init`. Due to the naming convention, `pico_create_task` ought to be in kernel.h. On the other hand `__piccolo_task_init` must be self contained. This applies for variables also.

Variables should prioritize readability over being short. Never use domain specific abbreviations, we have a N.A.P. (no abbreviations policy) unless extremely obvious like HTML or OS.


## File names
All file names are expected to be snake cased, short and concise to as their function. Additionally, keep files where it logically makes sense. For example, nothing from `os/` but the `api.c` should be interacting with the `programs/` folder and vice versa, so if a file does anything to `boot.c` or `kernel.c` it should be included in `os/`.