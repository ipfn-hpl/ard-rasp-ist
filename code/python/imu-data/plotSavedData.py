#!/usr/bin/env python3
"""
This script plots

"""

import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets, mkQApp
# from mdsClient import mdsClient
# from mdsthin.exceptions import TreeNNF
import argparse
import influxdb_client
print("Influxdb_client Version : {}".format(influxdb_client.__version__))
from influxdb_client import InfluxDBClient

import numpy as np
import pandas as pd

#client = InfluxDBClient(
IF_URL = 'http://kane584.tecnico.ulisboa.pt:8086'
IF_TOK = 'H1H3MkH8E3L_HUEDJRFLhQuFOyUWR87_bjvgqXda7KzZnzQF3aN4NQs9OrYTJixsqslcyAXsJzP4j41-uRiz4Q=='
IF_ORG ='7ffb1a7998038e38'

DATE = '2025-03-13'
START = 'T14:39:00.100'
STOP  = 'T14:40:00'
RANGE = f'start:{DATE:s}{START:s}Z, stop:{DATE:s}{STOP:s}Z'
query_iflx3 = (f'from(bucket:"ardu-rasp") |> range({RANGE}) '
              '|> filter(fn: (r) => r._measurement == "imu_data" ) '        
              '|> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")')

def select_data(dataframe, date, start_time, end_time):
    t_time = dataframe['_time']
    st = date + ' ' + start_time
    et = date + ' ' + end_time
    mask = (t_time > st) & (t_time <= et)
    return dataframe.loc[mask]

class MainWindow(QtWidgets.QMainWindow):

    """ main window """
    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)
        parser = argparse.ArgumentParser(
            description="""Script to 
            """)
        parser.add_argument('-d', '--date', default=DATE,
                                help='Date ')
        parser.add_argument('-s', '--start', default=START,
                                help='Start time')
        parser.add_argument('-e', '--end', default=STOP,
                                help='End time')
        parser.add_argument('-u', '--url', default=IF_URL,
                                help='MDSplus host url')

        args = parser.parse_args()
        client = InfluxDBClient(url=args.url,
                                token=IF_TOK,
                                org=IF_ORG)
        self.query_api = client.query_api()
        cw = QtWidgets.QWidget()
        gr_lay = QtWidgets.QGridLayout()
        cw.setLayout(gr_lay)
        # cw.setLayout(glay)
        # gr_lay = pg.GraphicsLayoutWidget(show=True)

        self.setCentralWidget(cw)
        self.imuDf = self.getData(args.date, args.start, args.end)


        # exp_df = select_data(self.imuDf, args.date, args.start, args.end)
        exp_df = self.imuDf
        timeAbs = np.array(exp_df['_time'].astype(np.int64)) / 1.0e9
        timeRel = timeAbs - timeAbs[0]

        accelX = np.array(exp_df['accel.x'].astype(np.float64))
        accelY = np.array(exp_df['accel.y'].astype(np.float64))
        pw1 = pg.PlotWidget(name='Plot1')  ## giving the plots names allows us to link their axes
        #l2 = gr_wid.addLayout(colspan=2, border=(50,0,0))
        # gr_lay.addLabel('Vertical Axis Label', angle=-90, rowspan=2)
        # pw1 = gr_lay.addPlot(title="non-interactive")
        pw1.addLegend()
        pw1.plot(timeRel, accelX, pen=pg.mkPen(0, width=1),
                 name="accelX")
        pw1.plot(timeRel, accelY, pen=pg.mkPen(1, width=1),
                 name="accelY")
        pw1.scene().sigMouseClicked.connect(self.mouse_clicked)
        self.pw1 = pw1

        monoRadio = QtWidgets.QRadioButton('mono')
        rgbaRadio = QtWidgets.QRadioButton('rgba')
        topXradio = QtWidgets.QRadioButton('topX')
        gr_lay.addWidget(monoRadio, 0, 1)
        gr_lay.addWidget(rgbaRadio, 1, 2)
        gr_lay.addWidget(topXradio, 2, 2)
        monoRadio.setChecked(True)

        # gr_lay.addWidget(widget=pw1, row=0, column=0,) #  rowSpan=2)
        gr_lay.addWidget(pw1, 0, 0, 3, 1)  # Row 1, Column 1, rowspan=2025widget=pw1, row=0, column=0,) #  rowSpan=2)

        # self.setCentralWidget(gr_lay)
        # self.setWindowTitle('pyqtgraph example: Interactive color bar')
        self.setWindowTitle(f"pyqtgraph MAXWELL WHEEL: {args.url}")
        self.resize(900, 800)
        self.show()

    def mouse_clicked(self, mouseClickEvent):
        # mouseClickEvent is a pyqtgraph.GraphicsScene.mouseEvents.MouseClickEvent
        # print('clicked 0x{:x}, event: {}'.format(id(self), mouseClickEvent))
        scene_coords = mouseClickEvent.scenePos()

        # mouse_point = self.pw1.vb.mapSceneToView(scene_coords)
        mouse_point = self.pw1.plotItem.vb.mapSceneToView(scene_coords)
        # mouse_point = self.pw1.mapFromScene(scene_coords)

        print(f'clicked plot X: {mouse_point.x()}, Y: {mouse_point.y()}, event: {mouseClickEvent}')

    def getData(self, date, start, end):
        RANGE = f'start:{date:s}{start:s}Z, stop:{date:s}{end:s}Z'
        query_iflx = (f'from(bucket:"ardu-rasp") |> range({RANGE}) '
                      '|> filter(fn: (r) => r._measurement == "imu_data" ) '        
                      '|> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")')
        imu_df = self.query_api.query_data_frame(query=query_iflx)
        imu_df.head()
        return imu_df





mkQApp("ColorBarItem Example")
main_window = MainWindow()

## Start Qt event loop
if __name__ == '__main__':
    pg.exec()

