from typing import Callable, Hashable, Protocol, TypeVar, TypeAlias

T = TypeVar('T')


class SupportsOrder(Protocol):
    def __lt__(self: T, other: T) -> bool:
        pass

    def __eq__(self: T, other: T) -> bool:
        pass


class SupportsOrderAndHash(Protocol):
    def __lt__(self: T, other: T) -> bool:
        pass

    def __eq__(self: T, other: T) -> bool:
        pass

    def __hash__(self: T) -> int:
        pass


HashableT = TypeVar('HashableT', bound=Hashable)
OrderedT = TypeVar('OrderedT', bound=SupportsOrder)
HashableOrderedT = TypeVar(
    'HashableOrderedT', bound=SupportsOrderAndHash
)

GroupKeyfunc: TypeAlias = Callable[[T], HashableOrderedT]
MPKeyfunc: TypeAlias = Callable[[HashableT], OrderedT]
