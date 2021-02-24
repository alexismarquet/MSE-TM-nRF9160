import sys
import argparse
import subprocess
import signal
import os
import time
import tinydb
import itertools
import datetime

parser = argparse.ArgumentParser(description="Process some integers.")

parser.add_argument(
    "MQTT_broker", metavar="MQTT_broker", type=str, help="Address of the MQTT broker"
)


args = parser.parse_args()

proc_list = []

try:
    os.remove("opaque.json")
except OSError as error:
    pass

try:
    os.remove("measures.json")
except OSError as error:
    pass

try:
    os.remove("log/cheater.json")
except OSError as error:
    pass


def signal_handler(sig, frame):
        for i in proc_list:
            i.terminate()

    # once terminated, check for integrity between send data and received data


print(f"starter running demo at {datetime.datetime.now()}")
start = time.time()
signal.signal(signal.SIGINT, signal_handler)
print("Press Ctrl+C to terminate")

# run AG process
proc_list.append(
    subprocess.Popen(
        ["python3", "AWS-ag.py", args.MQTT_broker],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
)
# run cheater process
proc_list.append(
    subprocess.Popen(
        ["python3", "cheater.py", args.MQTT_broker],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
)

# wait for end of demo
signal.pause()

end = time.time()
print(f"finished at {end}")
# stop processes
for i in proc_list:
    i.terminate()

dbMeasures = tinydb.TinyDB("measures.json")
dbGenerated = tinydb.TinyDB("log/cheater.json")

measures = dbMeasures.all()
generated = dbGenerated.all()

lenGen = len(generated)
lenMes = len(measures)

# print("generated")
# print(generated)

# print("measures")
# print(measures)


for g in generated:
    #    print(f"testing presence of {g}")
    if g in measures:
        #        print(f"found {g}")
        measures.remove(g)
    else:
        print(f"MISSING {g}")
# print("missing")
# print(measures)

# for missing in measures:
#    print(f"missing {missing}")

print(
    f"the devices generated {lenGen} measures in {int(time.time()-start)} seconds, and {lenMes} measures ended up in the database"
)
