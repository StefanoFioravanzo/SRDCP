## SOURCE FILES STRUCTURE

#### `app.c`

Application layer which defines the type of traffic (`APP_UPWARD_TRAFFIC` and `APP_DOWNWARD_TRAFFIC`) and the network size. The application sends periodically both source routing messages and data collection messages.

It also provides two callback functions to process the received data from the two protocols:

- `recv_cb()` to process data received at the sink from the data collection protocol
- `sr_recv_cb()` to process data received at a node from the source routing protocol

#### `project-conf.h`

Contiki's configuration header. Here are defined the parameters of the contiki's network stack alongside other parameter of Contiki OS.

**TOOD: List parameters and their configuration.**

#### `my_collect.h`

All the main configuration macros regarding message scheduling and network properties are defined here, alongside with structures that defines the routing table, and all the messages types.

The most important structure is `my_collect_conn`, a connection object that stores all the persistent information of a node and is passed by reference in all main functions of the node.

#### `my_collect.c`

This file handles all the send and receive functions.

- `my_collect_open()`: called by the application layer to initialize the state of a node. It Initializes the `my_collect_conn` structure (state of the node), opens the unicast and broadcast channels and starts the timer to send beacons (in case the node is the sink).
- `beacon_timer_cb()`: callback of the beacon timer. It's task is to send a broadcast beacon, and then re-schedule the timer to send again the beacon in the future.
- `send_beacon()`: **Broadcasts** a **beacon** message, forwarding current beacon sequence number and metric.
- `bc_recv()`: general **broadcast** **receive** callback. In this application the only packet sent in broadcast is the beacon message. The function unpacks the beacon message and updates the metric and parent if required.
- `my_collect_send()`: send function of the **data collection protocol**. This function is called by the application layer to send a packet to the sink. The node sends the packet to its parent, which will forward it until it reaches the destination. This function also piggybacks (if required) the node's topology information, adding its parent to the packet's header.
- `sr_send()`: send function of the **source routing protocol** called by the application layer. The sink sends a packet to a specific node in the network. The sink needs to find a route to the node exploiting its routing table, this is done calling the method `find_route()`. The resulting path is added to the packet's header.
- `uc_recv`: general **unicast** **function**. Receives three types of packets: "topology reports", "data collection packets and "source routing packets". Based on the packet type at the beginning of the header, the function calls a specific processing function.
- `forward_upward_data()`: forwards a data collection packet.
	- At the sink: check correctness of the packet and deliver data to application layer.
	- At node: forward packet to parent node. Piggyback topology information in the header in case the protocol.
- `forward_downward_data()`: forwards a source routing packet. The node checks its address against the first one in the path (in the packet's header). In case of a match, removes its address and reduces header size accordingly.

#### `topology_report.c`

This file handles all the logic related to sending and receiving topology reports.

- `topology_report_hold_cb()`: Timer call back to handle the send event of a topology report. When a node needs to send a topology report, it waits for a defined time.(`TOPOLOGY_REPORT_HOLD_TIME` defined in `my_collect.h`) waiting to piggyback the information. If during that time no application packet is sent, so no piggybacking can be performed, then the nodes send a dedicated topology report.
- `check_topology_report_address()`:  ...
- `send_topology_report()`: Sends a dedicated topology report to the sink. This function also implements a forward functionality in the case a topology report comes from a children of the network tree and the current node is waiting to send a topology report itself (`treport=1`). In that case the node expands the header of the packet to include its own information.
- `deliver_topology_report_to_sink()`: Function called by the sink when it receives a topology report. The node reads the topology information and updated the routing table. This function is called by the `uc_recv()` unicast callback in `my_collect.c`.

#### `routing_table.c`

Sink related functions to manage the routing table and compute the path to a given node. The routing table is defined in `my_collect.h` as a `TreeDict` struct, containing an array of `DictEntry` structs, composed of a `key` and a `value` (both of type `linkaddr_t`).

- `dict_find_index()`: Returns the index of the `key` address in the routing table. `-1` in case of not match.
- `dict_find()`: Returns the value associated to a particular key.
- `dict_add()`: Adds a new entry to the routing table.
- `init_routing_path()`: Initialize the array `tree_path` which stores the routing path. This is used before computing a new path.
- `already_in_route()`: Check if the target is already present in the partial route (function used while computing a path to a node to prevent loops).
- `find_route()`: Uses the above functions to compute a path from the sink to the specified destination. In case of success it returns the path length.

#### Piggybacking

The piggyback functionality is controlled using the `PIGGYBACKING` macro defined in `my_collect.h`. In case the macro is set `1`, every nodes always piggybacks its topology information. This might not sound optimal and may lead to bit packets but the assumption is that we are dealing with a small network and the longest path length in the network is at most 10 hops. 

Topology information is stored using the `tree_connection` structure defined in `my_collect.h` which contains two `linkaddr_t` variables. `linkaddr_t` is just an array of two `u8` elements resulting in `2` bytes. So given that every `tree_connection` occupies `4` bytes and the fact that the packet buffer provided by Contiki allows for 128 bytes, there is plenty of space for up to 10 nodes to piggyback their topology information to the sink. 

The results using this technique are quite good and acceptable, a smarted protocol that piggybacks information just once in a while based on topology changes could become useful in case of more complex and dense networks that have to take into consideration also the size of the packets.

#### Message Scheduling

Knowing the message scheduling of the application layer (assumed to be fixed), the routing protocol scheduling can be optimized to limit the chance of collisions. There a few parameters that can tweak the timing behaviour of the protocol:

- `BEACON_INTERVAL`: How often the Sink initiated the broadcast of a beacon message to create the spanning connection tree
- `BEACON_FORWARD_DELAY`: A random range of time a node can wait to forward a beacon message
- `TOPOLOGY_REPORT_HOLD_TIME`: How much time a nodes waits (to piggybacK topology information) before sending a dedicated topology report

We know from the application layer that data collection packets are sent every ???, so the beacon interval can be set such that the beacons do not collide with the data collection packets.

TODO: Say something also about the timing of source routing.

The results produced with many different timing can be found in the `results/` folder of the project.

#### MAC LAYER

- nullrdc (100% PDR in both using simple impl - just piggyback)
- contikimac: say that I tried to look into the parameters of this protocol. Give reference.
