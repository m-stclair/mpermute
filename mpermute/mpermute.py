from typing import Collection

from mpermute._mpermute import _mpermute


def mpermute(elements: Collection) -> tuple[tuple, ...]:
    try:
        elements = tuple(elements)
    except (TypeError, ValueError):
        raise TypeError("Argument must be castable to tuple")
    # ass't type and protocol checks occur in C layer
    return _mpermute(elements)
