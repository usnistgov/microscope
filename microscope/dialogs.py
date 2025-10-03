
# Qt5 imports
import PyQt5.uic
from PyQt5 import QtWidgets

import importlib.resources


def find_resource(filename: str) -> str:
    package_name = "microscope"
    return str(importlib.resources.files(package_name).joinpath(filename))


class AboutDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)

        self.setWindowTitle("About Microscope")

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(QtWidgets.QLabel("<b>Microscope</b>"))
        if parent is not None:
            layout.addWidget(QtWidgets.QLabel(f"{parent.title} Python version"))
        layout.addWidget(QtWidgets.QLabel("by Joe Fowler and NIST Boulder Labs, 2025"))
        layout.addWidget(QtWidgets.QLabel("Not subject to copyright in the United States"))
        self.setLayout(layout)


class HelpDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)

        PyQt5.uic.loadUi(find_resource("ui/help.ui"), self)


class AxisRangeDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)

        PyQt5.uic.loadUi(find_resource("ui/axesdialog.ui"), self)

    def get_values(self) -> dict[str, float | None]:
        """Get the line-edit values, as a dict.

        Returns
        -------
        dict[str, float | None]
            The axis range limit, or None if none, for each of ("xmax", "xmin", "ymax", "ymin")
        """

        def get(lineedit) -> float | None:
            val = lineedit.text()
            try:
                return float(val)
            except ValueError:
                return None

        return {
            "xmax": get(self.xMaxLineEdit),
            "xmin": get(self.xMinLineEdit),
            "ymax": get(self.yMaxLineEdit),
            "ymin": get(self.yMinLineEdit),
        }
