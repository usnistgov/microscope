from dataclasses import dataclass


@dataclass(frozen=True)
class ChannelGroup:
    firstChan: int
    nChan: int

    @property
    def lastChan(self) -> int:
        return self.firstChan + self.nChan - 1
