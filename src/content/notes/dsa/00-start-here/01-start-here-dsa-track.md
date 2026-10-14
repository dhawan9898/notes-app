---
title: "Start Here: How to Use the DSA Track"
track: dsa
module: Start Here
moduleOrder: 0
order: 1
level: beginner
summary: "What data structures and algorithms are and why they matter, what C knowledge you need, three reading paths, and how to use the code, brute-force checks and measurements in each note."
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed."
  - label: "Sedgewick & Wayne — Algorithms, 4th ed."
  - label: "Bentley — Programming Pearls, 2nd ed."
---

A **data structure** is a way of organising data in memory so that the operations you need are fast: finding an item, adding one, removing the smallest. An **algorithm** is a step-by-step method that solves a problem. The same task can take a millisecond or a month depending on the choice: that is why they are the core of programming interviews and of real systems (databases, routers, compilers, file systems).

## What you need first

Basic C: variables, loops, functions, arrays and (for most notes) pointers. If any of that is new, do these first, in the C track: **C From Zero**, **Pointers for Beginners** and **Practice: C Exercises**. Then read **Thinking in Data Structures** in this track for the central ideas (counting work, Big-O, recursion) before the first technical note.

## Reading paths

| You are… | Read in this order |
|---|---|
| **Beginner** | *Thinking in Data Structures* → *Practice: DSA Exercises* → the six **DSA Essentials** notes (*Searching and Sorting for Beginners*, *Linked Lists, Stacks and Queues for Beginners*, *Hash Tables for Beginners*, *Trees and Graphs for Beginners*, *Measuring Growth*, *Depth-First Search and Ordering Tasks*; they need *Structs, Enums and Typedef* from the C track) → *Complexity Analysis and Amortized Analysis* → *Dynamic Arrays* → *Linked Lists* → *Stacks, Queues, Deques and Ring Buffers* → *Binary Search* → *Comparison Sorts* → *Hash Tables* → *Binary Trees and BSTs* → *Graph Representations and Traversals* |
| **Preparing for interviews** | modules 1 to 3, then *Hash Tables*, *Heaps*, *Graph Representations and Traversals*, *Shortest Paths*, *Dynamic Programming*, *Greedy Algorithms*, *Problem-Solving Patterns and the Complexity Cheat Sheet* |
| **Systems engineer** | *Hash Tables*, *Probabilistic Structures*, *Tries and Longest-Prefix Match*, *B-Trees and LSM Trees*, *Caches*, *Allocators*, *Concurrent and Lock-Free Structures* |

## How every note works

1. **Idea first.** The structure or algorithm in plain words, with the invariant that makes it correct.
2. **Working C code.** Every listing was compiled and run. Nothing is pseudo-code.
3. **Tested against brute force.** Each implementation is checked against a slow, obviously correct version on thousands of random small inputs. When a note says "matches brute force", that comparison was run.
4. **Measured.** Claims like "faster" come with numbers from a real run. Exact times depend on your machine; the *ratios* and growth shapes carry over.
5. **Pitfalls and Key takeaways.** The mistakes people actually make, and a summary for revision.

## How to study

- Implement each structure yourself before reading ours; then compare.
- Write a brute-force version and a test that compares both on random inputs. This one habit finds most bugs.
- Compile with `-fsanitize=address,undefined` while developing.
- Draw pictures of pointers and arrays on paper.
- After each note, re-solve the practice problems without looking.

Terms you do not know are in the [Glossary](../../../glossary/).

## Key takeaways

1. A data structure is chosen for the operations you need to be fast.
2. Learn counting work and Big-O first; most other ideas are variations on it.
3. Implement, test against brute force, measure.
