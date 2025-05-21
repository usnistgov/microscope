from dataclasses import dataclass


@dataclass(frozen=True)
class ChannelGroup:
    firstChan: int
    nChan: int
