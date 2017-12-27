# PM-Sat
A parallel SAT-solver based on openmpi

## Content
This repository holds a copy of the parallel solver from Luis Gil based on MiniSat from Niklas Een, Niklas Sorensson. The original source code can be accessed from the website [PMSat website](algos.inesc-id.pt/algos/software.php).

## Modification
The original code has minor bugs related to compilation. The bugs are fixed here so that it can be directly compiled on Ubuntu 16.04 machine with openmpi 3.0.0 and g++ 5.4.0. Now you can simply "make" in the directory of all the source files.

## Why put it here
The original author doesn't have a github repo and they no longer update their code, so I put the modified source code here for public review. I hope it can bring inspiration to those who are learning openmpi and SAT solvers. I studied this SAT solver for a parallel computing course project. It takes me quite some time to locate the compilation bugs before I can really use them to compare my own implemenation. Hope it can save your time.