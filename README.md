Flip
====

Automatically solving Simon Tatham's Flip puzzle (Breath-First-Search, optimizing memory use).

I implemented my first attempt in OCaml, and when I realized memory usage is an issue,
I switched to C++ 11 - where I lessened memory use by almost 2 orders of magnitude
via bitsets - and got a 10x speedup.

I then realized I can do the same via bit-level handling in OCaml, and proceeded to do so.

The two are now within 15% - execution time-wise. Memory wise, C++ is still king: in the
sample board used to test, it only needed 280MB, whereas OCaml needed 595MB.

To run and see it solve a board, just "make test".

Have any questions? Just mail me...

Thanassis Tsiodras, Dr.-Ing.
ttsiodras@gmail.com
