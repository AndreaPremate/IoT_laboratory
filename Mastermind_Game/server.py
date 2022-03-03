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

username={'old': "", 'new': ""}
matches={}

@app.route('/', methods=["GET", "POST"])
def index():
    if request.method=="POST":
        if request.form.get('new_username')!="" and request.form.get('old_username')!="":
            username['new']=request.form.get('new_username')
            username['old']=request.form.get('old_username')
            mqtt_json = json.dumps(username)
            print(mqtt_json)
            topic = 'db3ap/'+ str(username['old']) + '/username'
            mqtt.publish(topic, mqtt_json)
    return render_template('index.html', matches=matches)

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
    room= jsonMqtt['room']
    user= jsonMqtt['user']
    combo= jsonMqtt['comb']  
    combinazione=""
    operation= jsonMqtt['operation']
    if operation != 'finish' and operation != 'abbandono':
        for i in combo:
            combinazione+= str(i) + " "
    
    if room not in matches:
        matches[room]={}
    if user not in matches[room]:
        matches[room][user]={}
    if operation == 'finish':
        del matches[room]
    elif operation != 'set':
        if 'combos' in matches[room][user]:
            if operation!='abbandono':
                matches[room][user]['combos'].append(combinazione)
                matches[room][user]['green'].append(operation[0])
                matches[room][user]['yellow'].append(operation[1])
                matches[room][user]['red'].append(operation[2])
            else:
                matches[room][user]['combos'].append('abbandono')
                matches[room][user]['green'].append(0)
                matches[room][user]['yellow'].append(0)
                matches[room][user]['red'].append(4)
        else:
            if operation!='abbandono':
                matches[room][user]['combos']=[]
                matches[room][user]['green']=[]
                matches[room][user]['yellow']=[]
                matches[room][user]['red']=[]
                matches[room][user]['combos'].append(combinazione)
                matches[room][user]['green'].append(operation[0])
                matches[room][user]['yellow'].append(operation[1])
                matches[room][user]['red'].append(operation[2])
            else:
                matches[room][user]['combos'].append('abbandono')
                matches[room][user]['green'].append(0)
                matches[room][user]['yellow'].append(0)
                matches[room][user]['red'].append(4)
    
    
    print(matches)


    #    values[node]= jsonMqtt[node]
    #    if values[node][0]=='yellow':
    #        alert['yellow']=True
    #print(values)
    #socketio.emit('my response', {'values': values})
    #requests.put('http://127.0.0.1:5000')
    

@mqtt.on_log()
def handle_logging(client, userdata, level, buf):
    # print(level, buf)
    pass

@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    mqtt.subscribe('db3ap/mastermind/web')

if __name__ == '__main__':
    socketio.run(app, host='127.0.0.1', port=5000, use_reloader=False, debug=True)
