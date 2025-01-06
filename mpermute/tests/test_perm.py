import gc
from sys import getrefcount as grc

from mpermute import mperms, mpermute


def test_mpermute_1():
    obj = (1, 4, 2, 2)
    res = mpermute(obj)
    assert len(res) == 12
    # order is undefined, hence set check
    assert set(res) == {
        (4, 2, 2, 1), (1, 4, 2, 2), (4, 1, 2, 2), (2, 4, 1, 2), (1, 2, 4, 2),
        (2, 1, 4, 2), (4, 2, 1, 2), (2, 4, 2, 1), (2, 2, 4, 1), (1, 2, 2, 4),
        (2, 1, 2, 4), (2, 2, 1, 4)
    }


def test_mpermute_refcount():
    a = (-1, 99, 98, 97)
    b = (-2, 101, 102, 103)
    mset = (a, a, a, b)
    n_aref, n_bref, n_msetref = grc(a), grc(b), grc(mset)
    res = mpermute(mset)
    assert grc(res) == 2
    assert grc(a) == n_aref + len(res) * mset.count(a)
    assert grc(b) == n_bref + len(res) * mset.count(b)
    assert grc(mset) == n_msetref
    del res
    gc.collect()
    assert grc(a) == n_aref


def test_mpermute_custom():
    a, b, c = (0, 1, 1), (1, 2, 1), (1000, 2, 1)
    mset = a, b, c
    n_aref, n_bref, n_cref, n_msetref = grc(a), grc(b), grc(c), grc(mset)
    compare_by_second = lambda x: x[1]
    res = mpermute(mset, compare_by_second)
    assert len(res) == 3
    # note that while only one of b or c should appear in the elements of `res`,
    # _which_ one appears is undefined
    n_aref0, n_bref0, n_cref0 = grc(a), grc(b), grc(c)
    assert n_bref == n_bref0 or n_cref == n_cref0
    assert n_bref0 + n_cref0 == n_bref + n_cref + 6


def test_mperms_1():
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
    # order is undefined, hence set check
    assert set(res) == {
        (4, 2, 2, 1), (1, 4, 2, 2), (4, 1, 2, 2), (2, 4, 1, 2), (1, 2, 4, 2),
        (2, 1, 4, 2), (4, 2, 1, 2), (2, 4, 2, 1), (2, 2, 4, 1), (1, 2, 2, 4),
        (2, 1, 2, 4), (2, 2, 1, 4)
    }


def test_mperms_custom():
    obj = ("ralph", "randomly", "rankled", "at", "an", "estuary")
    gen = mperms(obj, lambda x: x.startswith("r"))
    res = tuple(gen)
    assert len(res) == 20
    assert len(set(res[0])) == 2
