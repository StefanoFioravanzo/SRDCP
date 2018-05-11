## Protocol Specification

#### Topology Construction

In order to provide bidirectional communication between the nodes and the sink, the routing protocol needs to build an appropriate topology, meaning that there must be a way for the nodes to discover their neighbors and decide whether to "link" with them.

An appropriate topology structure for this scenario is a tree-like structure, with the Sink node as root.

There are many approaches to maintain topology information throughout the network. In our case the Sink will store a routing table containing all the topology information of all the nodes (node-parent links), and every node will store just its parent.

**Topology construction protocol**

The protocol to construct the topology is as follows:

1. Sink: broadcast a `beacon` message, containing sequence number `i` and `metric = 0`
2. Node: receive beacon
	- If `i` is greater than local sequence number `k`, accept the beacon. Go to step **3**
	- Check if the signal strength is sufficient and beacon metric is greater than previous one
	- If both conditions true, go to step **3**
	- Otherwise return
3. Node: Register sender as parent
4. Node: forward beacon with sequence number `i` and `metric + 1`

The Sink will periodically re-initiate the protocol with sequence number `i+1` to rebuild or fix the topology, accounting for any message loss during the previous runs or any change in the nodes present in the network.

The `metric` used here is just the hop count, initialized by the sink to `0`. More complex and more performant metrics could be use, but implementing them goes out of the scope of this project.

#### Data Collection

The data collection protocol simply requires every node to be able to send some data to the sink. Having already built a spanning tree of all the nodes in the network with the Sink node as root, collecting this data is just a matter of sending a packet to the parent and forward it until it reaches the Sink.

Once the sink received the packet, it can read the original sender form the packet header and send the data to the application layer.

#### Issues

- Loss of packets (time synch)
- Possible loop formation: explain why and also a possible solution

#### Source Routing

To allow the Sink to send data to arbitrary destinations in the network, it has to store some topology information to build a routing path. Indeed, while constructing the spanning tree, the only topology information is stored in the nodes as a node-parent link. 

The Sink needs some way to retrieve this information from all the participating nodes to construct a global view of the network topology. In order to build the routing table, the Sink needs to collect the parent address of each node in the network. 

The are two ways to do it:

- Dedicated **Topology Report** messages: dedicated messages sent with the purpose of informing the sink of topology information (node - parent information).
- **Piggyback** information in Data Collection packets: Topology information is piggybacked (i.e. added to the header) of data collection packets. This reduces the number of topology reports needed thus limiting the packet traffic of the communication channel. When the sink receives a data collection packet, it checks the header for any topology information.

The Sink stores the topology information inside a routing table in the form of `node -> parent`, when it needs to send data to a specific node, it can compute a path backwards using the information in the routing table.

>**Note**: While computing the path, the Sink needs to check for any loop in the routing table. These loops could form because the routing table is not a precise snapshot of the current network topology, but is dependent on the transmission of topology report messages or piggybacked information, which take some time to propagate.

Once the sink has determined a viable path, it has to include all the involved nodes' addresses inside the header of the packet. Once a node receives a source routing packet the following happens:

1. Extract the first address in the packet header, if it matches the current node's address accept the packet, otherwise stop
2. Reduce the size of the header removing the current node's address
3. In case this was the last address in the packet header, deliver the data inside the body of the packet to the application layer. Otherwise go to step **3**
4. Forward the packet to the next node's address in the header

#### Avoiding Network Congestion

**Scheduling**

With all these messages wandering around it's easy to experience collisions and loose many packets. Timing is of utmost importance, the scheduling of the topology report messages with respect to the broadcast beacons sent by the sink has to be chosen carefully in order to reduce collisions as much as possible. Refer to section *Message Scheduling* of the [Implementation Guide](Implementation.md) for mode details on how this issue is addressed.

**Avoiding useless messages**

The sending rate of topology report messages must be chosen carefully as well. If the rate is too high, the might be too many packets in the air that would result in frequent packets collisions. To avoid this issue, one node could wait some time before sending a topology report (after a topology change - e.g. a parent change after a beacon broadcast). If the node receives a child's topology report or a data collection message, it can either concatenate its topology information in the incoming topology report message, or extend the header of the data collection message and piggyback its topology information.

In this way one less packet would be sent, decreasing the probability of collisions. Refer to section *Collision Avoidance Strategies* of the [Implementation Guide](Implementation.md) for mode details on how this issue is addressed.

