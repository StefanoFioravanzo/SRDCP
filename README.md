# Source Routing And Data Collection for Wireless Sensor Networks

This project was developed as a final assignment of the LaboratoryOfWirelessSensorNetworks course @UniversityOfTrento during my master degree in Computer Science. 

The assignment required to implement a simple routing protocol for low power wireless devices using the Contiki open source project. [Contiki](http://www.contiki-os.org/) is an open source OS for Internet Of Things devices or, more generally, it enables building complex wireless systems for tiny low-cost, low-power microcontrollers.

We assume to have many nodes which serve as sensors or actuators and need to report some information to a central node, the Sink. Likewise, the Sink needs to be able to send messages to any node present in the network. To do this, the protocol need to build some kind of topology connecting the nodes in an optimal way, taking into considerations (messages) collision and topology changes (nodes leaving and entering the network) scenarios.

So, the routing protocol should be able of supporting two types of traffic patterns:

- *many-to-one* traffic (Data collection): The network nodes should be able to send packets to the Sink
- *one-to-many* traffic (Source routing): The Sink should be able to send packets to any node in the network


To understand the details of the implemented protocol, refer to the [Protocol.md](doc/Protocol.md) guide.

For a detailed description of the source code and main functions, refer to the [Implementation.md](doc/Implementation.md) guide.

The Cooja Network Simulator is part of the Contiki project and was built to run simulations of network software developed with Contiki. Cooja allows to simulate both large-scale networks or fully emulated hardware devices in extreme detail.

Under the `sim/` folder there are instructions to run some Cooja simulations on simple networks and how to parse the results. A summary of the simulations' results is under `results/`.

## Build

The provided `Dockerfile` will build an Ubuntu image with all the required dependences to build and run the simulations.

The `contiki/` folder contains a packages version of Contiki (???) version? 

To build and start the docker container, just run:

```
docker-compose build
docker-compose up
```

The compose file will replace the image `/code` folder with a volume pointing to the host's project directory, so any change to the source code will be reflected to the running container as well.

##### Running Cooja GUI

TODO
