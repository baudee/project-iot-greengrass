import json
import sys
import time
import traceback
import datetime

from awsiot.greengrasscoreipc.clientv2 import GreengrassCoreIPCClientV2

DEVICES_TOPIC = 'devices/+/data'
ACCELERATOR_NAME = 'Ursy-Accelerator1'
DETECTOR_NAME = 'Ursy-Distance1'
DEVICES_NAME = [ACCELERATOR_NAME, DETECTOR_NAME]

CORE_NAME = 'GreengrassCore-Ursy'
PUBLISH_DATA_TOPIC = 'cores/' + CORE_NAME + '/devices/+/data'
PUBLISH_HEALTH_TOPIC = 'cores/' + CORE_NAME + '/devices/+/health'

TIMEOUT = 10
QOS = 0

# Init variables
health_map = {
    ACCELERATOR_NAME: None,
    DETECTOR_NAME: None,
}

data_map = {
    ACCELERATOR_NAME: [],
    DETECTOR_NAME: [],
}


def on_received_message(event):
    try:
        binary_message = event.binary_message

        topic = binary_message.context.topic
        device_name = topic.split('/')[1]

        payload = binary_message.message.decode('utf-8')

        health_map[device_name] = int(datetime.datetime.now().timestamp())
        data_map[device_name].append(payload)
    except:
        traceback.print_exc()


def publish_data(device_name):
    # publish health status
    health_topic = PUBLISH_HEALTH_TOPIC.replace('+', device_name)

    if health_map[device_name] is not None:
        message_dict = { "status" : health_map[device_name]}
        ipc_client.publish_to_iot_core(
            topic_name=health_topic,
            qos=QOS,
            payload=json.dumps(message_dict).encode('utf-8')
        )

        health_map[device_name] = None

    # Publish data
    data_topic = PUBLISH_DATA_TOPIC.replace('+', device_name)
    
    message_dict = {}

    if len(data_map[device_name]) > 0:
        if device_name == ACCELERATOR_NAME:
            avg_x = 0
            avg_y = 0
            avg_z = 0
            avg_temp = 0
            avg_dist = 0
            dist_count = 0
            for str_data in data_map[device_name]:
                data = json.loads(str_data)
                avg_x += data['x']
                avg_y += data['y']
                avg_z += data['z']
                avg_temp += data['temperature']
                if "distance" in data:
                    avg_dist += data['distance']
                    dist_count += 1
                    
            avg_x /= len(data_map[device_name])
            avg_y /= len(data_map[device_name])
            avg_z /= len(data_map[device_name])
            avg_temp /= len(data_map[device_name])

            message_dict = { "x" : avg_x, "y" : avg_y, "z" : avg_z, "temperature" : avg_temp}
            if dist_count > 0:
                avg_dist /= dist_count
                message_dict["distance"] = avg_dist
                
            ipc_client.publish_to_iot_core(
                topic_name=data_topic,
                qos=QOS,
                payload=json.dumps(message_dict).encode('utf-8')
            )
        elif device_name == DETECTOR_NAME:
            avg_dist = 0
            dist_count = 0
            for str_data in data_map[device_name]:
                data = json.loads(str_data)
                if "distance" in data:
                    avg_dist += data['distance']
                    dist_count += 1
                    
            if dist_count > 0:
                avg_dist /= dist_count
                message_dict["distance"] = avg_dist
                
            if message_dict:
                ipc_client.publish_to_iot_core(
                    topic_name=data_topic,
                    qos=QOS,
                    payload=json.dumps(message_dict).encode('utf-8')
                )

        data_map[device_name] = []


if __name__ == '__main__':
    try:
        ipc_client = GreengrassCoreIPCClientV2()

        _, operation = ipc_client.subscribe_to_topic(
            topic=DEVICES_TOPIC, on_stream_event=on_received_message)

        try:
            while True:
                time.sleep(60)

                for device_name in DEVICES_NAME:
                    publish_data(device_name)

        except InterruptedError:
            print('Subscribe interrupted.')

        operation.close()
    except Exception:
        print('Exception occurred when using IPC.', file=sys.stderr)
        traceback.print_exc()
        exit(1)
