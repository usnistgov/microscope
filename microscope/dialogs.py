
# Qt5 imports
import PyQt5.uic
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import QSettings, pyqtSlot, QCoreApplication

import importlib.resources


def find_resource(filename):
    package_name = "microscope"
    return str(importlib.resources.files(package_name).joinpath(filename))


class AboutDialog(QtWidgets.QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)

        self.setWindowTitle("About Microscope")

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(QtWidgets.QLabel("<b>Microscope</b>"))
        layout.addWidget(QtWidgets.QLabel(f"{parent.title} Python version"))
        layout.addWidget(QtWidgets.QLabel("by Joe Fowler and NIST Boulder Labs, 2025"))
        layout.addWidget(QtWidgets.QLabel("Not subject to copyright in the United States"))
        self.setLayout(layout)


class HelpDialog(QtWidgets.QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)

        PyQt5.uic.loadUi(find_resource("ui/help.ui"), self)
