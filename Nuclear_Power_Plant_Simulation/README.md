# second-assignment-2021
**CORE:**

The idea is to build an adaptive sensor network which is aware of a new node coming in the network. Each new node is based on a NodeMCU (esp8266).
Since we have only one NodeMCU available we have to simulate the presence of two nodes. Use the first assignment as starting point. It means that the hardware configuration may be exactly the same. To simulate the presence of two nodes we have to virtually separate the sensors in two groups. Let's suppose we have a temperature/humidity (TH) and a light sensor (LS). The TH sensor will be virtually associated to the NodeMCU1 and the LS sensor to the NodeMCU2. We have to consider one of the NodeMCU as master and the other as slave. The master node is already registered in the sensor network while the slave is to be dinamically added to the sensor network. The sensor network communications should be handled by using MQTT. The master node may act as root user of the sensor network: it may collect the messages coming from all the nodes in the network (in this case one) and it may log these messages on the suitable database.


**ADD-ONS**

Set up a remote control of the monitoring system (start, stop, etc.) through a web page. All the sensed values and alerting events must be logged into a database (MySQL or InfluxDB).


**INGREDIENTS:**

- Micro of your choice
- Sensor or actuators of your choice.

**EXPECTED DELIVERABLES:**

- Github code upload at: [Classroom link](https://classroom.github.com/g/4pPRvAZ0)
- Powerpoint presentation of 5 mins
