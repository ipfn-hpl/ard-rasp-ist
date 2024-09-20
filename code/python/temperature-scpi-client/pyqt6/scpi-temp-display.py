#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PyQt6 App for Display Arduiono SCPI Temperatures 
author:  B. Carvalho
email: bernardo.carvalho@tecnico.ulisboa.pt
"""
import sys
import serial
import pyqtgraph as pg
import numpy as np

from PyQt6.QtCore import (
        QSize,
        # Qt,
        QTimer,
        )

from PyQt6.QtWidgets import (
    QButtonGroup,
    QWidget,
    QApplication,
    # QLineEdit,
    QMainWindow,
    # QTableView,
    QVBoxLayout,
    QHBoxLayout,
    QGridLayout,
    QDialog,
    QDialogButtonBox,
    # QCheckBox,
    # QComboBox,
    # QGroupBox,
    QMessageBox,
    QPushButton,
    # QSpinBox,
    # QRadioButton,
    # QTabWidget,
    QLabel,
)

PORT = '/dev/ttyUSB0'
BAUDRATE = 57600


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Calorimeter Temperatures")
        self.serialCon = serial.Serial(PORT, BAUDRATE, timeout=1)
        self.serialCon.reset_input_buffer()
        container = QWidget()
        layoutMain = QGridLayout()
        lblTr = QLabel('Temperature RED')
        lblTr.setStyleSheet("color: red")
        lblTr.setFixedHeight(80)
        layoutMain.addWidget(lblTr, 0, 0)
        lblTb = QLabel('Temperature BLUE')
        lblTb.setStyleSheet("color: blue")
        layoutMain.addWidget(lblTb, 0, 1)
        self.Tredlabel = QLabel('10.0')
        layoutMain.addWidget(self.Tredlabel, 1, 0)
        self.Tbluelabel = QLabel('11.0')
        layoutMain.addWidget(self.Tbluelabel, 1, 1)
        self.plotT = pg.PlotWidget(title="Plot")
        layoutMain.addWidget(self.plotT, 2, 0, 1, 2)
        self.TredArray = np.zeros(120) # Number of sample to store
        self.TblueArray = np.zeros(120) # Number of sample to store

        container.setLayout(layoutMain)
        self.setMinimumSize(QSize(1200, 700))
        self.setCentralWidget(container)

        self.iter = 1

        self.timer = QTimer()
        self.timer.timeout.connect(self.update_panels)
        self.timer.start(1000) # Sample Period in millisec

    def get_temp(self, probe):
        p_str = f'{probe:d}'
        command = b'MEAS:SCAL:TEMP:DS' + p_str.encode('utf-8') + b'?\n'
        # print(command)
        self.serialCon.write(command)
        line = self.serialCon.readline()
        print(line)
        return float(line)

    def update_panels(self):
        try:
            # xdata.append((float(data[0]) - timeStart )/1000.0 )
            tRed = self.get_temp(0)
            self.Tredlabel.setStyleSheet("font: 30pt; color: red; background-color: "
                                         "gray; border: 1px solid black")
            self.Tredlabel.setText(f'Tred: {tRed:.2f}')
            self.TredArray[0] = tRed
            self.TredArray = np.roll(self.TredArray, -1)  # rotate circular buffer
            self.plotT.clear()
            self.plotT.setYRange(0, 100)
            self.plotT.plot(self.TredArray)  #, pen=({'color: red', 'width: 1'}), name="ch{1}")

            tBlue = self.get_temp(1)
            self.Tbluelabel.setText(f'{tBlue:.2f}')

        # if the try statement throws an error, just do nothing
        except: 
            print('Error reading Serial')
"""
#self.TredArray[0] = self.iter
        #self.iter += 1
        #self.TredArray = np.roll(self.TredArray, -1) # rotate circular buffer
        #self.plotT.clear()
        #self.plotT.setYRange(0, 50)
        #self.plotT.plot(self.TredArray) # , pen=({'color: red', 'width: 1'}), name="ch{1}")
"""

app = QApplication(sys.argv)
window = MainWindow()
window.show()
sys.exit(app.exec())

# vim: syntax=python ts=4 sw=4 sts=4 sr et
