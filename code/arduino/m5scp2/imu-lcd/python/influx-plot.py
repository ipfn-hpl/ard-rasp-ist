
"""


export INFLUXDB_TOKEN="****"

2024-10-24T09:42:44.264163902Z  2024-10-24T09:42:55.264163902Z
"""

# import influxdb_client
import os
# import time
from influxdb_client import InfluxDBClient
# , WritePrecision
# from influxdb_client.client.write_api import SYNCHRONOUS
# import pandas as pd
import matplotlib.pyplot as plt

plt.close("all")
# import matplotlib

token = os.environ.get("INFLUXDB_TOKEN")
org = "IPFN-IST"
url = "http://kane584.tecnico.ulisboa.pt:8086"

client = InfluxDBClient(url=url, token=token, org=org)

query_api = client.query_api()

# query = """from(bucket: "ardu-rasp")
# |> range(start: -1m)
# |> filter(fn: (r) => r._measurement == "measurement1")"""

query_fmt = """from(bucket:"ardu-rasp") 
|> range(start:{}, stop:{})
|> filter(fn: (r) => r._measurement == "imu_data"
and  (r._field == "accel_x" or r._field == "accel_y" or r._field == "accel_z"))
|> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
"""

start = '2024-10-24T09:42:00Z'
stop = '2024-10-24T09:42:55Z'
query = query_fmt.format(start, stop)
# tables = query_api.query(query, org="IPFN-IST")

# for table in tables:
#    for record in table.records:
#        print(record)

imu_stats = client.query_api().query_data_frame(org=org, query=query)
# imu_stats.to_pickle('imu_stirling.pkl')
# pd.display(imu_stats.head())
# i mu_stats.drop(columns=['result', 'table', 'start', 'stop'])
df = imu_stats.drop(columns=['result', 'table',
                             '_start', '_stop', '_measurement'])
ax1 = df.plot.scatter(x='_time',
                      y='accel_x',
                      c='DarkBlue')
ax2 = df.plot.scatter(x='_time',
                      y='accel_y',
                      c='Green', ax=ax1)
ax3 = df.plot.scatter(x='_time',
                      y='accel_z',
                      c='Red', ax=ax1)
# imu_stats.head()
plt.show()
