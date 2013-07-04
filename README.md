Flip
====

Automatically solving Simon Tatham's Flip puzzle (Breath-First-Search, optimizing memory use).

I implemented my first attempt in OCaml, and when I realized memory usage is an issue,
I switched to C++ 11 - where I lessened memory use by almost 2 orders of magnitude,
and got a 10x speedup.

To run, just "make test".
