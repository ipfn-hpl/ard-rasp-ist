#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""


export INFLUXDB_TOKEN="****"

"""

import os
# import time
from influxdb_client import InfluxDBClient
# , WritePrecision
# from influxdb_client.client.write_api import SYNCHRONOUS
# import pandas as pd
import matplotlib.pyplot as plt

plt.close("all")
org = "7ffb1a7998038e38"
# define INFLUXDB_BUCKET "ardu-rasp"
token = os.environ.get("INFLUXDB_TOKEN")
# org = "IPFN-IST"
url = "http://kane584.tecnico.ulisboa.pt:8086"

client = InfluxDBClient(url=url, token=token, org=org)

query_api = client.query_api()

# query = """from(bucket: "ardu-rasp")
# |> range(start: -1m)
# |> filter(fn: (r) => r._measurement == "measurement1")"""

query_fmt = """from(bucket:"ardu-rasp") 
|> range(start:{}, stop:{})
|> filter(fn: (r) => r._measurement == "imu_data"
and  (r._field == "gyro.x" or r._field == "gyro.y" or r._field == "gyro.z"))
|> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
"""


start = '2025-03-10T10:21:00Z'
stop = '2025-03-10T10:22:00Z'
query = query_fmt.format(start, stop)

# for table in tables:
#    for record in table.records:
#        print(record)

imu_stats = client.query_api().query_data_frame(org=org, query=query)
# imu_stats.to_pickle('imu_stirling.pkl')
# pd.display(imu_stats.head())
# i mu_stats.drop(columns=['result', 'table', 'start', 'stop'])
df = imu_stats.drop(columns=['result', 'table',
                             '_start', '_stop', '_measurement'])
ax1 = df.plot.line(x='_time',
                   y='gyro.x',
                   c='DarkBlue', style='-o')
ax2 = df.plot.line(x='_time',
                   y='gyro.y',
                   c='Green', ax=ax1)
ax3 = df.plot.line(x='_time',
                   y='gyro.z',
                   c='Red', ax=ax1)
# imu_stats.head()
plt.show()
