import paho.mqtt.client as mqtt
import time
import argparse
from tinydb import TinyDB, Query
from tinyrecord import transaction
import logging
import sys
import json
import threading
import ssl
from random import randint


CA_ROOT_CERT_FILE = "cheater-certificate/AmazonRootCA1.pem"
THING_CERT_FILE = "cheater-certificate/..."
THING_PRIVATE_KEY = "cheater-certificate/..."

# init args parser
parser = argparse.ArgumentParser(description="Process some integers.")
parser.add_argument(
    "MQTT_broker", metavar="MQTT_broker", type=str, help="Address of the MQTT broker"
)
args = parser.parse_args()

logFormatter = logging.Formatter(
    "%(asctime)s [%(threadName)-12.12s] [%(levelname)-5.5s]  %(message)s"
)
logger = logging.getLogger()
fileHandler = logging.FileHandler("{0}/{1}.log".format("log", f"cheater"))
fileHandler.setFormatter(logFormatter)
logger.addHandler(fileHandler)
consoleHandler = logging.StreamHandler(sys.stdout)
consoleHandler.setFormatter(logFormatter)
logger.addHandler(consoleHandler)
logger.setLevel(logging.INFO)
db = TinyDB(f"log/cheater.json")


def on_message(client, userdata, message):
    print("test message")
    logger.info("rcvd: " + message.topic + "/" + str(message.payload.decode("utf-8")))

    if message.topic == "cheater":
        j = json.loads(message.payload.decode("utf-8"))
        logger.info(f"m: {message.payload}")
        db.insert({"entry": message.payload.decode("utf-8")})

    if message.topic == "requestPool":
        client.subscribe(f"getPool{message.payload.decode('utf-8')}")



# connecting to MQTT broker
logger.info(f"Connecting to broker at {args.MQTT_broker}")
client = mqtt.Client("Cheater")
#client.enable_logger()
client.tls_set(CA_ROOT_CERT_FILE, certfile=THING_CERT_FILE, keyfile=THING_PRIVATE_KEY)
client.connect(args.MQTT_broker, 8883)

# start receive thread
client.loop_start()

# subscribe to
# * addToPool:      endpoint for opaque payload
# * requestPool:    endpoint for opaque pool request from devices
# * measures:       endpoint for clear-data measures
client.subscribe("cheater")
client.subscribe("requestPool")


# register receive routine
client.on_message = on_message

# only on event execution
while True:
    time.sleep(1)

client.loop_stop()
