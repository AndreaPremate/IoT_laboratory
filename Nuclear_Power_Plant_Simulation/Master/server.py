import eventlet
import json
from flask import Flask, render_template, redirect, url_for, request
from flask_mqtt import Mqtt
from flask_socketio import SocketIO
from flask_bootstrap import Bootstrap
import requests

eventlet.monkey_patch()

app = Flask(__name__)
app.config['SECRET'] = 'my secret key'
app.config['TEMPLATES_AUTO_RELOAD'] = True
app.config['MQTT_BROKER_URL'] = "149.132.178.180"
app.config['MQTT_BROKER_PORT'] = 1883
app.config['MQTT_CLIENT_ID'] = 'd.bellini11'
app.config['MQTT_CLEAN_SESSION'] = True
app.config['MQTT_USERNAME'] = 'dbellini3'
app.config['MQTT_PASSWORD'] = 'iot816602'
app.config['MQTT_KEEPALIVE'] = 5
app.config['MQTT_TLS_ENABLED'] = False
app.config['MQTT_LAST_WILL_TOPIC'] = 'home/lastwill'
app.config['MQTT_LAST_WILL_MESSAGE'] = 'bye'
app.config['MQTT_LAST_WILL_QOS'] = 0

mqtt = Mqtt(app)
socketio = SocketIO(app)
bootstrap = Bootstrap(app)

start= {'start': False}
alert= {'yellow':False}
values={}
threshold={'temperature': 2000, 'pressure': 50, 'rssi': -100, 'radioactivity': 15}

@app.route('/', methods=["GET", "POST"])
def index():
    if request.method=="POST":
        if 'rssi_input' in request.form.keys():
            if request.form.get('rssi_input')!="":
                threshold['rssi']= request.form.get('rssi_input')
            if request.form.get('temperature_input')!="":
                threshold['temperature'] = request.form.get('temperature_input')
            if request.form.get('pressure_input')!="":
                threshold['pressure']= request.form.get('pressure_input')
            if request.form.get('radioactivity_input')!="":
                threshold['radioactivity']= request.form.get('radioactivity_input')
            mqtt_json = json.dumps(threshold)
            print(mqtt_json)
            mqtt.publish('db3ap/web/threshold', mqtt_json)
        elif 'start' in request.form.keys():
            start['start']=True
            mqtt_json = json.dumps(start)
            mqtt.publish('db3ap/web/start', mqtt_json)
        elif 'stop' in request.form.keys(): 
            print(request.form.keys())
            start['start']=False
            mqtt_json = json.dumps(start)
            mqtt.publish('db3ap/web/start', mqtt_json)
        else:
            alert['yellow']=False
            for node in values:
                values[node][0]= 'green'
            mqtt_json = json.dumps(alert)
            mqtt.publish('db3ap/web/stopAlert', mqtt_json)
    return render_template('index.html', values= values, threshold=threshold, start=start, alert=alert)

@socketio.on('publish')
def handle_publish(json_str):
    data = json.loads(json_str)
    mqtt.publish(data['topic'], data['message'], data['qos'])


@socketio.on('subscribe')
def handle_subscribe(json_str):
    data = json.loads(json_str)
    mqtt.subscribe(data['topic'], data['qos'])


@socketio.on('unsubscribe_all')
def handle_unsubscribe_all():
    mqtt.unsubscribe_all()


@mqtt.on_message()
def handle_mqtt_message(client, userdata, message):
    data = dict(
        topic=message.topic,
        payload=message.payload.decode(),
        qos=message.qos,
    )
    jsonMqtt= json.loads(data['payload'])
    for node in jsonMqtt:
        values[node]= jsonMqtt[node]
        if values[node][0]=='yellow':
            alert['yellow']=True
    print(values)
    socketio.emit('my response', {'values': values})
    #requests.put('http://127.0.0.1:5000')
    

@mqtt.on_log()
def handle_logging(client, userdata, level, buf):
    # print(level, buf)
    pass

@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    mqtt.subscribe('db3ap/web/parameters')

if __name__ == '__main__':
    socketio.run(app, host='127.0.0.1', port=5000, use_reloader=False, debug=True)
