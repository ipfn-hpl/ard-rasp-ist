#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PyQt6 App for Display Arduino SCPI Temperatures
author:  B. Carvalho / IPFN-IST
email: bernardo.carvalho@tecnico.ulisboa.pt
"""
import sys
import argparse
import serial
import pyqtgraph as pg
import numpy as np
from datetime import datetime

from PyQt6.QtCore import (
        QSize,
        QTimer,
        )

from PyQt6.QtWidgets import (
    # QButtonGroup,
    QWidget,
    QApplication,
    # QLineEdit, QTableView,
    QMainWindow,
    QVBoxLayout,
    QHBoxLayout,
    QGridLayout,
    QDialog,
    # QDialogButtonBox, QCheckBox, QComboBox, QGroupBox, QMessageBox,
    QPushButton,
    # QSpinBox, QRadioButton, QTabWidget,
    QLabel,
)

DEVICE = '/dev/ttyUSB0'
BAUDRATE = 57600
SAMPLE_PERIOD = 1000  # Sample Period in millisec


def parseCommandLine():
    """Use argparse to parse the command line for specifying
    a device to use.
    parser.add_argument("-f", "--file", type=str,
        help="A path to a .qml file to be the source.",
        required=True)
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--device', type=str,
                        help='Serial device to use', default=DEVICE)
    parser.add_argument('-s', '--samples', type=int,
                        help='Number of samples to Show/Store', default=120)
    parser.add_argument('-u', '--simUl',
                        action='store_true', help='Simulation Mode')
    args = vars(parser.parse_args())
    return args


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Calorimeter Temperatures")
        args = parseCommandLine()  # Return command line arguments
        dev = args['device']
        self.simul = args['simUl']
        print(self.simul)
        self.serialCon = serial.Serial(dev, BAUDRATE, timeout=1)
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
        plotT = pg.PlotWidget(title="Plot")
        layoutMain.addWidget(plotT, 2, 0, 1, 2)
        samples = args['samples']  # Number of sample to store
        self.TredArray = 20.0 * np.ones(samples)
        self.TblueArray = 17.0 * np.ones(samples)
        self.CurveTred = plotT.plot(self.TredArray, pen='red')
        self.CurveTblue = plotT.plot(self.TblueArray, pen='blue')
        plotT.setYRange(10, 105)
        # plotT.plot(self.TredArray)  # pen=({'color: red', 'width: 1'}), name="ch{1}")

        saveBtn = QPushButton('Save Temp Data')
        saveBtn.clicked.connect(self.save_data)
        layoutMain.addWidget(saveBtn, 3, 0)

        container.setLayout(layoutMain)
        self.setMinimumSize(QSize(1200, 700))
        self.setCentralWidget(container)

        self.iter = 1

        self.timer = QTimer()
        self.timer.timeout.connect(self.update_panels)
        self.timer.start(SAMPLE_PERIOD)  # Sample Period in millisec

    def save_data(self):
        T_mat = np.array([self.TredArray, self.TblueArray])

        filename = f"Temperature_log_{datetime.now():%Y-%m-%d_%H-%m-%d}.csv"
        np.savetxt(filename, T_mat.T, delimiter=",")

    def get_temp(self, probe):
        p_str = f'{probe:d}'
        if self.simul:
            return 20.0 + probe * 10.0 + 4.0 * np.random.random_sample()
        else:
            command = b'MEAS:SCAL:TEMP:DS' + p_str.encode('utf-8') + b'?\n'
            # print(command)
            self.serialCon.write(command)
            line = self.serialCon.readline()
            print(line)
            return float(line)

    def update_panels(self):
        # xdata.append((float(data[0]) - timeStart )/1000.0 )
        try:
            tRed = self.get_temp(0)
            self.Tredlabel.setStyleSheet("font: 30pt; color: red; background-color: "
                                            "gray; border: 1px solid black")
            self.Tredlabel.setText(f'Tred: {tRed:.2f}')
            self.TredArray = np.roll(self.TredArray, -1)  # rotate circular buffer
            self.TredArray[-1] = tRed
            self.CurveTred.setData(self.TredArray)
            # self.plotT.clear()
            # self.plotT.plot(self.TredArray)  #, pen=({'color: red', 'width: 1'}), name="ch{1}")
            tBlue = self.get_temp(1)
            self.Tbluelabel.setText(f'{tBlue:.2f}')
            self.TblueArray = np.roll(self.TblueArray, -1)  # rotate circular buffer
            self.TblueArray[-1] = tBlue
            self.CurveTblue.setData(self.TblueArray)

        except ValueError:
            print('error read value ')
        except KeyboardInterrupt:
            sys.exit()
        #    print('Error reading Serial')


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())

# vim: syntax=python ts=4 sw=4 sts=4 sr et
