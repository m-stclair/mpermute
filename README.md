# mpermute

## overview

Python extension for fast multiset permutations. Basically a straight C
port of https://github.com/ekg/multipermute/, which is itself a straightforward
implementation of "Loopless Generation of Multiset Permutations using a Constant
Number of Variables by Prefix Shifts (Williams 2009). This implementation is
3-4x faster and slightly more space-efficient than the pure Python version.

It also exposes a backend function, `unique`, which is useful for counting
unique elements of a collection (optionally grouped by a key function). 

Experimental code. May crash your computer. Not suitable for any purpose.
Use at your own peril.

## requirements

* Python >= 3.12
* `setuptools`

## installation

Install from source with `pip install .`.

## examples of use

```
>>> from mpermute import mperms, mpermute, unique

>>> mpermute("akjj")

(('k', 'j', 'j', 'a'),
('a', 'k', 'j', 'j'),
('k', 'a', 'j', 'j'),
('j', 'k', 'a', 'j'),
('a', 'j', 'k', 'j'),
('j', 'a', 'k', 'j'),
('k', 'j', 'a', 'j'),
('j', 'k', 'j', 'a'),
('j', 'j', 'k', 'a'),
('a', 'j', 'j', 'k'),
('j', 'a', 'j', 'k'),
('j', 'j', 'a', 'k'))

>>> next(mperms("akjj")

('k', 'j', 'j', 'a')

>>> lists = [1], [2, 3], [9, 1], [1, 4]
>>> mpermute(lists + (lists[0],), )

(([9, 1], [2, 3], [1, 4], [1], [1]),
 ([1], [9, 1], [2, 3], [1, 4], [1]),
 ([9, 1], [1], [2, 3], [1, 4], [1]),
 ([2, 3], [9, 1], [1], [1, 4], [1]),
...

# when passed a second argument, all high-level functions in the `mpermute` 
# library will treat it as a 'key' function that defines equivalence classes.
# Outputs are guaranteed to include only one member of each equivalence class.
# This will _usually_ be the last member of that equivalence class, but this 
# behavior is not guaranteed.

>>> mpermute(lists + (lists[0],), lambda x, y: len(x) > len(y))

(([1], [1], [2, 3], [2, 3], [2, 3]),
 ([2, 3], [1], [1], [2, 3], [2, 3]),
 ([1], [2, 3], [1], [2, 3], [2, 3]),
 ([2, 3], [1], [2, 3], [1], [2, 3]),
 ([2, 3], [2, 3], [1], [1], [2, 3]),
 ([1], [2, 3], [2, 3], [1], [2, 3]),
 ([2, 3], [1], [2, 3], [2, 3], [1]),
 ([2, 3], [2, 3], [1], [2, 3], [1]),
 ([2, 3], [2, 3], [2, 3], [1], [1]),
 ([1], [2, 3], [2, 3], [2, 3], [1]))

# `unique()` offers performance improvements 
# over pure-Python methods only when passed a key function. Calling it with 
# no second argument should produce the same result as 
# ```{k: seq.count(v) for v in set(seq)}``` (note that order of dict keys is 
# undefined in both cases), with basically the same performance. 
# Note that all elements of the collection passed to `unique` must be hashable.

>>> rands = [random.randint(0, 10000) for _ in range(1000000)] 
>>> unique(rands, lambda r: r % 3)

{645: 333306, 5839: 332940, 4193: 333754}
```

## licensing

This library bears the BSD 3-clause license, copyright Michael St. Clair.
Although it contains no verbatim code from https://github.com/ekg/multipermute,
it is essentially a port of that library and also bears its license.
