#!/usr/bin/env python3
"""
"""

import argparse
import serial
import influxdb_client, os, time
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

token = os.environ.get("INFLUXDB_TOKEN")
ORG = "IPFN-IST"
URL = "http://epics.ipfn.tecnico.ulisboa.pt:8086"

client = influxdb_client.InfluxDBClient(url=URL, token=token, org=ORG)

bucket="ardu-rasp"

DEVICE = '/dev/ttyUSB0'
BAUDRATE = 57600
SAMPLE_PERIOD = 1000  # Sample Period in millisec


"""
write_api = client.write_api(write_options=SYNCHRONOUS)

for value in range(5):
    point = (
            Point("measurement1")
            .tag("experiment", "calorimetry")
            .field("field1", value)
            )
    write_api.write(bucket=bucket, org="IPFN-IST", record=point)
    time.sleep(1) # separate points by 1 second
        p_str = f'{probe:d}'
        if self.simul:
            return 20.0 + probe * 10.0 + 4.0 * np.random.random_sample()
        else:
"""
def get_temp(serialCon, probe):
    p_str = f'{probe:d}'
    command = b'MEAS:SCAL:TEMP:DS' + p_str.encode('utf-8') + b'?\n'
    print(command, end='')
    serialCon.write(command)
    line = serialCon.readline()
    return float(line)

def main(args):
    serialCon = serial.Serial(DEVICE, BAUDRATE, timeout=1)
    serialCon.reset_input_buffer()
    write_api = client.write_api(write_options=SYNCHRONOUS)
    # for value in range(args.seconds):
    try:
        while True:
            try:
                temp0 = get_temp(serialCon, 0)
                print(f" {temp0:.2f}")
                temp1 = get_temp(serialCon, 1)
                print(temp1)
                point = (
                    Point("ds18b20Sensors")
                    .tag("experiment", "calorimetry")
                    .field("tempRed", temp0)
                    .field("tempBlue", temp1)
                    )
                write_api.write(bucket=bucket, org="IPFN-IST", record=point)
            except influxdb_client.rest.ApiException:
                print('Influx "unauthorized access"')
                print('Forgot to set Influx Token?')
                exit()
            except ValueError:
                print('... Error SCPI reading!')
            time.sleep(1)

    except KeyboardInterrupt:
        print('Loop interrupted!')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description='Script to extract temperature from arduino and sendo to InfluxDB')
    parser.add_argument('-s', '--seconds', type=int, help='Seconds to run', default=10)

    args = parser.parse_args()
    main(args)
# vim: syntax=python ts=4 sw=4 sts=4 sr et
