from typing import Collection

from mpermute.mptypes import HashableT, MPKeyfunc
from mpermute.mpermute_core import (
    # dictbuild, groupby,
    mpermute, MPGenerator, unique, unique_2
)


def mperms(
    elements: Collection[HashableT], key: MPKeyfunc | None = None
) -> MPGenerator:
    return MPGenerator(elements, key)
