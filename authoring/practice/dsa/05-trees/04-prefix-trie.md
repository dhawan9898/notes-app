---
kind: code
title: Prefix trie
module: Trees
difficulty: Medium
tags:
  - trie
  - strings
starter: dsa/starters/trees/ex4-trie.c
examples:
  - input: words car card care cart cat dog do; count_prefix("car")
    output: '4'
  - input: count_prefix("")
    output: '7'
    why: The empty prefix matches every word.
  - input: contains("ca")
    output: '0'
    why: A prefix of a stored word is not itself a word.
constraints:
  - Words use only `a`–`z`.
  - Create child nodes with the given `new_node()`.
hints:
  - While inserting, increment `words_below` on every node along the path, including the last one.
  - Mark the last node with `is_word = 1`.
  - '`count_prefix` walks the prefix and returns `words_below` of the node it ends on (0 if the path breaks).'
---

Implement `trie_insert`, `count_prefix` (how many stored words start with a prefix) and `contains` (is this exact word stored) on a trie of lowercase words. `count_prefix` must take time proportional to the **prefix length**, which is why each node keeps `words_below`.

## Explanation

Each node counts how many inserted words pass through it (`words_through`), so after walking the prefix in O(length of prefix), that node's counter *is* the answer: 4 words start with `car`, 5 with `ca`. The separate `is_word` flag is needed because a prefix is not necessarily a word: `ca` is on the path of five words but is not itself one (`contains("ca")` is 0), whereas `car` is both a word and a prefix of three others. The cost is memory: this layout uses 26 pointers per node; compressed (radix) tries and hash- or array-mapped children reduce it.
