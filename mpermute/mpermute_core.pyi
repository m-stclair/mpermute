from typing import Collection, Iterator

from mpermute.mptypes import (
    GroupKeyfunc, HashableOrderedT, HashableT, MPKeyfunc, T
)

def groupby(
    elements: Collection[T], key: GroupKeyfunc
) -> dict[HashableOrderedT, tuple[T, ...]]: ...

def mpermute(
    elements: Collection[HashableT], key: MPKeyfunc | None = None
) -> tuple[tuple[HashableT, ...], ...]: ...

class MPGenerator(Iterator):
    def __init__(
        self, elements: Collection[HashableT], key: MPKeyfunc | None = None
    ): ...
    def __next__(self) -> tuple[HashableT, ...]: ...

def unique(
    elements: Collection[HashableT], key: MPKeyfunc | None = None
) -> dict[str, int | tuple[HashableT, ...]]: ...

def dictbuild(elements: Collection[HashableT], key: GroupKeyfunc) -> dict: ...