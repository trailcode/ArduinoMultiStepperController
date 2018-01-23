import os
import operator
import time
from time import sleep
from PyQt4 import uic, QtGui
from PyQt4.QtCore import Qt, QTimer, QSettings, QEvent
from PyQt4.QtGui import QMainWindow, QHBoxLayout, QLabel, QSpinBox, QSlider, QCheckBox, QDockWidget, QFileDialog
from scipy.interpolate import interp1d
from scipy.interpolate import splrep, splev, UnivariateSpline
from spyderlib.widgets import internalshell
import numpy as np
import sys
import serial
import telnetlib

#"""
servoOut = None
#try: s = serial.Serial(port='/dev/cu.wchusbserial1420', baudrate=115200)
#try: s = serial.Serial(port='/dev/cu.usbmodem1411', baudrate=115200)
#try: s = serial.Serial(port='/dev/cu.usbserial-A700JNGX', baudrate=115200)
try: servoOut = serial.Serial(port='/dev/cu.usbserial-AI041TLS', baudrate=115200)
except: pass
#"""

def _str(s): return str.encode(str(s))

gui = None

class DriverTestGUI(QMainWindow):

    def __init__(self):
        super(DriverTestGUI, self).__init__()
        self.ui = uic.loadUi("GUI.ui", self)
        self.ui.show()

        self.setWindowTitle('Multi Stepper Controller')

        self.setupPythonConsole()

    def setupPythonConsole(self):
        global gui
        gui = self
        self.pythonshell = internalshell.InternalShell(self, namespace=globals(), commands=[], multithreaded=False,
                                                       light_background=False)
        self.ui.consoleLayout.addWidget(self.pythonshell)

if __name__ == '__main__':
    app = QtGui.QApplication(sys.argv)

    window = DriverTestGUI()

    window.show()
    window.raise_()

    sys.exit(app.exec_())