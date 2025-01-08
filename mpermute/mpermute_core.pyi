from typing import Collection, Iterator

from mpermute.mptypes import E, Keyfunc


def mpermute(
    elements: Collection[E], key: Keyfunc | None = None
) -> tuple[tuple[E, ...], ...]: ...

class MPGenerator(Iterator):
    def __init__(self, elements: Collection[E], key: Keyfunc | None = None): ...
    def __next__(self) -> tuple[E, ...]: ...

def unique(
    elements: Collection[E], key: Keyfunc | None = None
) -> dict[str, int | tuple[E, ...]]: ...
