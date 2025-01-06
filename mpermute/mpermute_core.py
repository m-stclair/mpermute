from functools import wraps
from typing import Callable, Collection, Hashable, TypeVar

from mpermute._mpermute import do_mpermute, MPGenerator

T = TypeVar('T')


def _mpwrap(
    func: Callable[
        [Collection[T], Callable[[T], Hashable] | None],
        dict[str, int | tuple[T, ...]] | tuple[T, ...]
    ],
    elements: Collection[T],
    key: Callable[[T], Hashable] | None
):
    try:
        iter(elements)
    except (TypeError, ValueError):
        raise TypeError("Elements must be iterable")
    # ass't type and protocol checks occur in C layer
    if key is None:
        return func(elements, None)
    return func(elements, key)

#
# def unique(
#     elements: Collection[T], key: Callable[[T], Hashable] | None = None
# ) -> dict[str, int | tuple[T, ...]]:
#     return _mpwrap(comp_unique, elements, key)


def mperms(
    elements: Collection[T], key: Callable[[T], Hashable] | None = None
) -> MPGenerator:
    return _mpwrap(MPGenerator, elements, key)


def mpermute(
    elements: Collection[T], key: Callable[[T], Hashable] | None = None
) -> tuple[tuple[T, ...], ...]:
    return _mpwrap(do_mpermute, elements, key)
