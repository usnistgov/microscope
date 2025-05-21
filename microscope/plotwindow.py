# Qt5 imports
import PyQt5.uic
from PyQt5.QtCore import Qt, pyqtSlot
from PyQt5 import QtWidgets

# other non-user imports
# import numpy as np
import os


def clear_grid_layout(grid_layout):
    while grid_layout.count():
        item = grid_layout.itemAt(0)
        widget = item.widget()
        if widget is not None:
            grid_layout.removeWidget(widget)
            widget.deleteLater()
        else:
            grid_layout.removeItem(item)


class PlotWindow(QtWidgets.QWidget):
    """Provide the UI inside each Plot Window."""

    NUM_TRACES = 8
    SPECIAL_INVALID = -1

    standardColors = (
        # For most, use QColor to replace standard named colors with slightly darker versions
        "black",
        "#b400e6",  # Purple
        "#00b4ff",  # Blue
        "#00bebe",  # Cyan
        "darkgreen",
        "#cdcd00",  # Gold
        "#ff8000",  # Orange
        "red",
        "gray"
    )

    def __init__(self, parent, isTDM=False):
        QtWidgets.QWidget.__init__(self, parent)
        PyQt5.uic.loadUi(os.path.join(os.path.dirname(__file__), "ui/plotwindow.ui"), self)
        self.isTDM = isTDM
        self.highestChan = 39
        self.setupChannels()

    def setupChannels(self):
        layout = self.channelNameLayout
        clear_grid_layout(layout)
        self.channelSpinners = []
        self.checkers = []
        for i in range(self.NUM_TRACES):
            c = "ABCDEFGHIJKL"[i]
            label = QtWidgets.QLabel(f"Trace {c}", self)
            label.setStyleSheet(f"color: {self.standardColors[i]};")
            layout.addWidget(label, i, 0)

            s = QtWidgets.QSpinBox(self)
            s.setRange(self.SPECIAL_INVALID, self.highestChan)
            s.setSpecialValueText("--")
            s.setValue(self.SPECIAL_INVALID)
            s.setPrefix("Ch ")
            s.setAlignment(Qt.AlignRight)
            s.setMinimumWidth(75)
            s.valueChanged.connect(self.channelChanged)
            self.channelSpinners.append(s)
            layout.addWidget(s, i, 1)

            if self.isTDM:
                box = QtWidgets.QCheckBox(self)
                tool = f"Trace {c} use error signal"
                box.setToolTip(tool)
                box.toggled.connect(self.errStateChanged)
                self.checkers.append(box)
                layout.addWidget(box, i, 2)

    @pyqtSlot(bool)
    def pausePressed(self, bool): pass
    @pyqtSlot()
    def clearGraphs(self): pass
    @pyqtSlot()
    def savePlot(self): pass

    @pyqtSlot(int)
    def channelChanged(self, value):
        sender = self.channelSpinners.index(self.sender())
        print(f"Next chan: {value} sent by spinner #{sender}")

    @pyqtSlot(bool)
    def errStateChanged(self, value):
        sender = self.checkers.index(self.sender())
        print(f"Err state: {value} sent by checkbox #{sender}")
