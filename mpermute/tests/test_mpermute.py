import gc
import random
from sys import getrefcount as grc

import pytest

from mpermute import mperms, mpermute


@pytest.mark.parametrize("func", (mperms, mpermute))
def test_mp_1(func):
    obj = (1, 4, 2, 2)
    res = tuple(func(obj))
    assert len(res) == 12
    # order is undefined, hence set check
    assert set(res) == {
        (4, 2, 2, 1), (1, 4, 2, 2), (4, 1, 2, 2), (2, 4, 1, 2), (1, 2, 4, 2),
        (2, 1, 4, 2), (4, 2, 1, 2), (2, 4, 2, 1), (2, 2, 4, 1), (1, 2, 2, 4),
        (2, 1, 2, 4), (2, 2, 1, 4)
    }


# NOTE: running refcount tests on Python builtin hash()-equivalent objects under
# pytest.mark.parametrize (or other functions that might perform monkey business
# with the stack) is a fool's game. Identifiers to be strictly reference-counted
# -- e.g., `a` and `b` below -- should refer to objects that do not have the
# same hash in different versions of the test. (Hence random.randint() calls.)
@pytest.mark.parametrize("func", (mperms, mpermute))
def test_mp_refcount(func):
    a = tuple(random.randint(-13, 13) for _ in range(8))
    b = tuple(random.randint(-13, 13) for _ in range(8))
    mset = (a, a, a, b)
    n_aref, n_bref, n_msetref = grc(a), grc(b), grc(mset)
    res = tuple(func(mset))
    assert grc(res) == 2
    assert grc(a) == n_aref + len(res) * mset.count(a)
    assert grc(b) == n_bref + len(res) * mset.count(b)
    assert grc(mset) == n_msetref
    del res
    gc.collect()
    assert grc(a) == n_aref


@pytest.mark.parametrize("func", (mperms, mpermute))
def test_mp_custom_1(func):
    a = (random.randint(-1000, 1000), 1, 5)
    b = (random.randint(-1000, 1000), 2, 3)
    c = (random.randint(-1000, 1000), 2, 8)
    mset = a, b, c
    n_aref, n_bref, n_cref, n_msetref = grc(a), grc(b), grc(c), grc(mset)
    compare_by_second = lambda x: x[1]
    res = tuple(func(mset, compare_by_second))
    assert len(res) == 3
    # note that while only one of b or c should appear in the elements of `res`,
    # _which_ one appears is undefined
    n_aref0, n_bref0, n_cref0 = grc(a), grc(b), grc(c)
    assert n_bref == n_bref0 or n_cref == n_cref0
    assert n_bref0 + n_cref0 == n_bref + n_cref + 6


@pytest.mark.parametrize("func", (mperms, mpermute))
def test_mp_custom_2(func):
    obj = ("ralph", "randomly", "rankled", "at", "an", "estuary")
    res = tuple(func(obj, lambda x: x.startswith("r")))
    assert len(res) == 20
    assert len(set(res[0])) == 2


def test_mperm_iteration():
    obj = (1, 4, 2, 2)
    gen = mperms(obj)
    res = [next(gen)]
    assert len(res[0]) == 4
    for _ in range(11):
        res.append(next(gen))
    try:
        next(gen)
        raise RuntimeError("Should have raised StopIteration")
    except StopIteration:
        pass
