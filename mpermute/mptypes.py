from typing import Callable, Hashable, Protocol, TypeVar, TypeAlias

R = TypeVar('R')


class HasOrdering(Protocol):
    def __lt__(self: R, other: R) -> bool:
        pass

    def __eq__(self: R, other: R) -> bool:
        pass


E = TypeVar('E', bound=Hashable)
O = TypeVar('O', bound=HasOrdering)
Keyfunc: TypeAlias = Callable[[E], O]
