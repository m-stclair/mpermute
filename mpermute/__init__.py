from typing import Collection

from mpermute.mptypes import E, Keyfunc
from mpermute.mpermute_core import mpermute, MPGenerator, unique


def mperms(
    elements: Collection[E], key: Keyfunc | None = None
) -> MPGenerator:
    return MPGenerator(elements, key)

