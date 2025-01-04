# mpermute

## overview

Python extension for fast multiset permutations. Basically a straight C
port of https://github.com/ekg/multipermute/, which is itself a straightforward
implementation of "Loopless Generation of Multiset Permutations using a Constant
Number of Variables by Prefix Shifts (Williams 2009). This implementation is
3-4x faster and slightly more space-efficient than the pure Python version.

Experimental code. May crash your computer. Not suitable for any purpose.
Use at your own peril.

## requirements

* Python >= 3.12
* `setuptools`

## installation

Install from source with `pip install .`.

## usage

A high-level API is forthcoming. For now, figure it out.

## licensing

This library bears the BSD 3-clause license, copyright Michael St. Clair.
Although it contains no verbatim code from https://github.com/ekg/multipermute,
it is essentially a port of that library and also bears its license.
