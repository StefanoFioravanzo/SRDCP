#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"

#include "my_collect.h"

/*
Data collection > multi-hop source routing protocol

Traffic patterns:

- topology protocol: build tree with beacons.   V

- data packets:
    - upward: from node to sink     V
    - downward: from sink to node

- Build the routing table:
	- piggybacking: attach header in data_collection
	- topology reports: ad-hoc report packets.     V

TODO: node sends topology report after joining the network (or changing the parent?)
TODO: Beware of collisions mostly during bootstrap when nodes join the network
    or change the parent. May have collisions between dedicated topology reports and beacons.
    How to avoid this?
    >> Idea: Have two un synced random frames.
    The first range of time is used for sink beacons, the second for sending topology report.
    This considering instantaneous transmission time (wrt time ranges).
TODO: Test protocol both with NullRDC and ContikiMAC.


THINK ABOUT THE TIMING. HOW TO BALANCE PIGGYBACKING AND TOPOLOGY REPORTS. USE A VERY BASIC IMPLEMENTATION TO HAVE SOMETHING FUNCTIONAL.

- Compute average application data rate. (Have an adaptive algortihm).
- if updated parent:
    - default: send dedicated topology report (reset timer)
    - adaptive: In case average application data rate is ~30s, don't send dedicated topology report.
- When sending upward data:
    - if

*/

// -------------------------------------------------------------------------------------------------
//                                      DICT IMPLEMENTATION
// -------------------------------------------------------------------------------------------------

int dict_find_index(TreeDict* dict, const linkaddr_t key) {
    int i;
    for (i = 0; i < dict->len; i++) {
        DictEntry tmp = dict->entries[i];
        if (linkaddr_cmp(&(tmp.key), &key) != 0) {
            return i;
        }
    }
    return -1;
}

linkaddr_t dict_find(TreeDict* dict, const linkaddr_t *key) {
    int idx = dict_find_index(dict, *key);
    linkaddr_t ret;
    if (idx == -1) {
        linkaddr_copy(&ret, &linkaddr_null);
    } else {
        linkaddr_copy(&ret, &dict->entries[idx].value);
    }
    return ret;
}

int dict_add(TreeDict* dict, const linkaddr_t key, linkaddr_t value) {
    /*
    Adds a new entry to the Dictionary
    In case the key already exists, it replaces the value
    */
    if (dict->len == MAX_NODES) {
        printf("Dictionary is full. MAX_NODES cap reached.");
        return -1;
    }
   int idx = dict_find_index(dict, key);
   linkaddr_t dst_value, dst_key;
   linkaddr_copy(&dst_value, &value);
   linkaddr_copy(&dst_key, &key);
   if (idx != -1) {  // Element already present, update its value
       dict->entries[idx].value = dst_value;
       return 0;
   }
   dict->len++;
   dict->entries[dict->len].key = dst_key;
   dict->entries[dict->len].value = dst_value;
   return 0;
}

// -------------------------------------------------------------------------------------------------
//                                      ROUTING TABLE MANAGEMENT
// -------------------------------------------------------------------------------------------------

/*
    Initialize the path by setting each entry to linkaddr_null
*/
void init_routing_path(my_collect_conn* conn) {
    int i = 0;
    linkaddr_t* path_ptr = conn->routing_table.tree_path;
    while(i < MAX_PATH_LENGTH) {
        linkaddr_copy(path_ptr, &linkaddr_null);
        path_ptr++;
        i++;
    }
}

/*
    Check if target is already in the path.
    len: number of linkaddr_t elements already present in the path.
*/
int already_in_route(my_collect_conn* conn, uint8_t len, linkaddr_t* target) {
    int i;
    for (i = 0; i < len; i++) {
        if (linkaddr_cmp(&conn->routing_table.tree_path[i], target)) {
            return true;
        }
    }
    return false;
}

/*
    Search for a path from the sink to the destination node, going backwards
    from the destiantion throught the parents. If not proper path is found returns 0,
    otherwise the path length.
    The linkddr_t addresses on the nodes in the path are written to the tree_path
    array in the conn object.
*/
int find_route(my_collect_conn* conn, const linkaddr_t *dest) {
    init_routing_path(conn);

    uint8_t path_len = 0;
    linkaddr_t parent;
    linkaddr_copy(&parent, dest);
    do {
        // copy into path the fist entry (dest node)
        memcpy(&conn->routing_table.tree_path[path_len], &parent, sizeof(linkaddr_t));
        parent = dict_find(&conn->routing_table, &parent);
        // abort in case a node has not parent or the path presents a loop
        if (linkaddr_cmp(&parent, &linkaddr_null) ||
            already_in_route(conn, path_len, &parent))
        {
            return 0;
        }
        path_len++;
    } while (!linkaddr_cmp(&parent, &sink_addr) && path_len < 10);

    if (path_len > 10) {
        // path too long
        return 0;
    }
    return path_len;
}

// -------------------------------------------------------------------------------------------------

bool piggybacking_operation_allowed(my_collect_conn* conn) {
    if (conn->traffic_control.piggy_sent >= PIGGYBACK_PACKETS_SENT) {
        return false;
    }
    return true;
}

// -------------------------------------------------------------------------------------------------

// Callbacks structure to initialize the communication channels
struct broadcast_callbacks bc_cb = {.recv=bc_recv};
struct unicast_callbacks uc_cb = {.recv=uc_recv};

void my_collect_open(my_collect_conn* conn, uint16_t channels,
                    bool is_sink, const struct my_collect_callbacks* callbacks)
{
    // initialize the conn structure
    linkaddr_copy(&conn->parent, &linkaddr_null);  // no parent
    conn->metric = 65535;  // max int: node not connected
    conn->beacon_seqn = 0;
    conn->callbacks = callbacks;
    conn->traffic_control.packet_rate = 0;
    conn->traffic_control.packet_counter = 0;
    conn->traffic_control.piggy_sent = 0;
    // ctimer_set(&conn->traffic_control.packet_rate_timer,
    //     CLOCK_SECOND,
    //     traffic_control_timer_cb,
    //     conn);

    if (is_sink) {
        conn->is_sink = 1;
    } else {
        conn->is_sink = 0;
    }

    // open the connection primitives
    broadcast_open(&conn->bc, channels, &bc_cb);
    unicast_open(&conn->uc, channels + 1, &uc_cb);

    // in case this is the sink, start the beacon propagation process
    if (conn->is_sink) {
        conn->metric = 0;

        // initialize routing table
        TreeDict rt;
        // TreeDict rt = {.len=0, .cap=MAX_NODES};
        conn->routing_table = rt;

        // start a timer to send the beacon. Pass to the timer the conn object
        ctimer_set(&conn->beacon_timer, CLOCK_SECOND, beacon_timer_cb, conn);
    }
}

/*
------------ TIMER Callbacks ------------
*/

void beacon_timer_cb(void* ptr) {
    my_collect_conn* conn = ptr;
    send_beacon(conn);
    // in case this is the sink, schedule next broadcast in random time
    if (conn->is_sink) {
        ctimer_set(&conn->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, conn);
    }
}

void traffic_control_timer_cb(void* ptr) {
    my_collect_conn* conn = ptr;
    send_topology_report(conn, 0);  // 0: first send, no forwarding

    ctimer_set(&conn->traffic_control_timer,
        PARENT_REFRESH_RATE,
        traffic_control_timer_cb,
        conn);
}

void packet_rate_timer_cb(void* ptr) {
    my_collect_conn* conn = ptr;

    conn->traffic_control.packet_rate = conn->traffic_control.packet_counter;
    conn->traffic_control.packet_counter = 0;

    ctimer_set(&conn->traffic_control.packet_rate_timer,
        PACKET_RATE_INTERVAL,
        traffic_control_timer_cb,
        conn);
}

/*
------------ SEND and RECEIVE functions ------------
*/

/*
    The node sends a topology report to its parent.
    Topology report is sent when the node changes its parent or after too much
    silence from the application layer (no piggybacking available).

    This method is also used for forwarding toward the sink a topology report
    received from a node.
    TODO: explore the possibility of appending this node's topology report (if needed)
    during forwarding to reduce packet traffic.
*/
void send_topology_report(my_collect_conn* conn, uint8_t forward) {
    if (forward == 1) {
            // TODO: improve performance by appending topology report to forwarding packet
            unicast_send(&conn->uc, &conn->parent);
            return;
    }
    // else
    printf("Node %02x:%02x sending a topology report\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    packetbuf_clear();

    // TODO: explore possibility of using data space instead of header.
    // How would I increase data allocation space for the packet? E.g. similar to packetbuf_hdralloc()
    // memcpy(packetbuf_dataptr(), &msg, sizeof(msg));
    // packetbuf_set_datalen(sizeof(msg));

    // No actual data in the packet, data present in the header

    // topology_report_header hdr = {.type=topology_report};
    enum packet_type pt = topology_report;
    tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};

    packetbuf_hdralloc(sizeof(int) + sizeof(tree_connection));
    memcpy(packetbuf_hdrptr(), &pt, sizeof(int));
    memcpy(packetbuf_hdrptr() + sizeof(int), &tc, sizeof(tree_connection));

    unicast_send(&conn->uc, &conn->parent);
}

/*
    The node sends a beacon in broadcast to everyone.
*/
void send_beacon(my_collect_conn* conn) {
    // init beacon message
    beacon_message beacon = {.seqn = conn->beacon_seqn, .metric = conn->metric};

    packetbuf_clear();
    packetbuf_copyfrom(&beacon, sizeof(beacon));
    printf("sending beacon: seqn %d metric %d\n", conn->beacon_seqn, conn->metric);
    broadcast_send(&conn->bc);
}

/*
    Receviing BEACON during topology construction (many-to-one).
    The sink is sending the beacon periodically and the nodes forward it to everyone.
    Each node updates its parent based on metric.

    When node updates its parent it sends a dedidated topology report.
*/
void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
    beacon_message beacon;
    int8_t rssi;

    // Get the pointer to the overall structure my_collect_conn from its field bc
    my_collect_conn* conn = (my_collect_conn*)(((uint8_t*)bc_conn) -
                                    offsetof(my_collect_conn, bc));

    if (packetbuf_datalen() != sizeof(beacon_message)) {
        printf("broadcast of wrong size\n");
        return;
    }

    memcpy(&beacon, packetbuf_dataptr(), sizeof(beacon_message));
    rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    printf("recv beacon from %02x:%02x seqn %u metric %u rssi %d\n",
        sender->u8[0], sender->u8[1],
        beacon.seqn, beacon.metric, rssi);

    if (rssi < RSSI_REJECTION_TRESHOLD) {
        printf("packet rejected due to low rssi\n");
        return;
    }

    // check received sequence number
    if (conn->beacon_seqn < beacon.seqn){
        // new tree
        conn->beacon_seqn = beacon.seqn;

    }else{
        if (conn->metric < beacon.metric) {
            // current hop count is better
            printf("return. conn->metric: %u, beacon.metric: %u\n",
                conn->metric, beacon.metric);
            return;
      }
    }
    // update metric (hop count)
    conn->metric = beacon.metric + 1;

    if (!linkaddr_cmp(&conn->parent, sender)) {
        // update parent
        linkaddr_copy(&conn->parent, sender);
        // since we have updated the parent, send the dedicated topology report.
        if (conn->traffic_control.packet_rate < DEDICATED_REPORT_SUPPRESSION_THRESHOLD) {
            // send the packet just if the application data rate is too low ()
            send_topology_report(conn, 0);
        }
    }

    conn->traffic_control.piggy_sent = 0;

    // Retransmit the beacon since the metric has been updated.
    // Introduce small random delay with the BEACON_FORWARD_DELAY to avoid synch
    // issues when multiple nodes broadcast at the same time.
    ctimer_set(&conn->beacon_timer, BEACON_FORWARD_DELAY, beacon_timer_cb, conn);
}

/*
    Data collection protocol. Here the node sends some data to its parent to reach the sink.
    If it is necessary the node atatches routing information (its parent's address) into
    the header (piggybacking).
    The data posrtion of the packetbuf is already filled by the application layer.
*/
int my_collect_send(my_collect_conn *conn) {
    uint8_t piggy_flag = piggybacking_operation_allowed(conn);
    // initialize header
    enum packet_type pt = data_packet;
    upward_data_packet_header hdr = {.source=linkaddr_node_addr,
                                    .hops=0, .piggy_len=0 };

    if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {  // in case the node has no parent
        return 0;
    }

    // allocate space for the header
    if (piggy_flag) {
        conn->traffic_control.piggy_sent++;
        hdr.piggy_len = 1;
        // topology information to be piggybacked
        tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};

        packetbuf_hdralloc(sizeof(int) + sizeof(upward_data_packet_header) + sizeof(tree_connection));
        memcpy(packetbuf_hdrptr(), &pt, sizeof(int));
        memcpy(packetbuf_hdrptr() + sizeof(int), &hdr, sizeof(upward_data_packet_header));
        memcpy(packetbuf_hdrptr() + sizeof(int) + sizeof(upward_data_packet_header), &tc, sizeof(tree_connection));
    } else {
        packetbuf_hdralloc(sizeof(int) + sizeof(upward_data_packet_header));
        memcpy(packetbuf_hdrptr(), &pt, sizeof(int));
        memcpy(packetbuf_hdrptr() + sizeof(int), &hdr, sizeof(upward_data_packet_header));
    }
    // increase the packet_counter for packet rate estimation
    conn->traffic_control.packet_counter++;
    return unicast_send(&conn->uc, &conn->parent);
}

/*
    General node's unicast receive function.
    Based on the packet type read from the first byte in the header
    a specific function is caled to handle the logic.

    Here we expect three types of packets:
        - upward traffic: from node to sink data packet passing through parents.
        - topology report: from node to sink passing through parents.
        - downward traffic: from sink to node using path computed at the sink.
*/
void uc_recv(struct unicast_conn *uc_conn, const linkaddr_t *sender) {
    // Get the pointer to the overall structure my_collect_conn from its field uc
    my_collect_conn* conn = (my_collect_conn*)(((uint8_t*)uc_conn) -
                                    offsetof(my_collect_conn, uc));

    //TODO:?
    // if (packetbuf_datalen() < sizeof(struct data_packet_header)) {
    //     printf("too short unicast packet %d\n", packetbuf_datalen());
    //     return;
    // }

    // Read packet type from first byte of the header.
    enum packet_type pt;
    memcpy(&pt, packetbuf_dataptr(), sizeof(int));

    printf("Node %02x:%02x received unicast packet with type %d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], pt);
    switch (pt) {
        case data_packet:
            printf("Node %02x:%02x receivd a unicast data packet\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            forward_upward_data(conn, sender);
            break;
        case topology_report:
            printf("Node %02x:%02x receivd a unicast topology report\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            send_topology_report(conn, 1);  // 1: forwarding set to true
            break;
        case source_routing:
            printf("Node %02x:%02x receivd a unicast source routing packet\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            forward_downward_data(conn, sender);
            break;
        default:
            printf("Packet type not recognized.\n");
    }
}

/*
    Send data from the sink to a specific node in the network.
    First, the sink has to compute the path from its routing table (avoiding loops)
    Second, the sink creates a header with all the path and sends the packet to the first node
    in the path.
*/
int sr_send(struct my_collect_conn *conn, const linkaddr_t *dest) {
    // the sink sends a packet to `dest`.

    if (!conn->is_sink) {
        // if this is an ordinary node
        return 0;
    }

    // populate the array present in the source_routing structure in conn.
    int path_len = find_route(conn, dest);
    if (path_len == 0) {
        return 0;
    }

    enum packet_type pt = source_routing;
    downward_data_packet_header hdr = {.hops=0, .path_len=path_len };

    // allocate enough space in the header for the path
    packetbuf_hdralloc(sizeof(int) + sizeof(downward_data_packet_header) + sizeof(linkaddr_t) * path_len);
    memcpy(packetbuf_hdrptr(), &pt, sizeof(int));
    memcpy(packetbuf_hdrptr() + sizeof(int), &hdr, sizeof(downward_data_packet_header));
    int i;
    // copy path in inverse order.
    for (i = path_len-1; i >= 0; i--) {  // path len because to insert the Nth element I do sizeof(linkaddr_t)*(N-1)
        memcpy(packetbuf_hdrptr()+sizeof(int)+sizeof(downward_data_packet_header)+sizeof(linkaddr_t)*(path_len-1-i),
            &conn->routing_table.tree_path[i],
            sizeof(linkaddr_t));
    }
    return unicast_send(&conn->uc, &conn->routing_table.tree_path[0]);
}

// -------------------------------------------------------------------------------------------------
//                                      UNICAST RECEIVE FUNCTIONS
// -------------------------------------------------------------------------------------------------

/*
    Forwarding or collection of data sent from one node to the sink.

        - Sink: Counts the number of topology informations with piggy_len present in the header.
        Then calls the callback to the application layer to retrieve the packet data.
        - Node: A node just forwards upwards the message. In case it wants to piggyback
        some topology information (its parent) it increases header size and writes in the header
        its parent.
*/
void forward_upward_data(my_collect_conn *conn, const linkaddr_t *sender) {
    upward_data_packet_header hdr;
    // before the header there is the packet type
    memcpy(&hdr, packetbuf_dataptr() + sizeof(int), sizeof(upward_data_packet_header));

    // if this is the sink
    if (conn->is_sink){
        uint8_t piggy_len = hdr.piggy_len;
        packetbuf_hdrreduce(sizeof(int) + sizeof(upward_data_packet_header));
        if (piggy_len > 0) {  // it mens there is some piggybacking
            // read the piggyybacked information and update the tree dictionary
            tree_connection piggy_info;
            size_t i;
            for (i = 0; i < piggy_len; i++) {
                memcpy(&piggy_info, packetbuf_dataptr(), sizeof(tree_connection));
                // TODO: add piggy info to dictionary
            }
            // remove from the packet the piggyback data. Leave just application data.
            packetbuf_hdrreduce(sizeof(tree_connection)*piggy_len);
        }
        // application receive callback
        conn->callbacks->recv(&hdr.source, hdr.hops +1 );
    }else{
        uint8_t piggy_flag = piggybacking_operation_allowed(conn);
         if (piggy_flag) {
             hdr.piggy_len = hdr.piggy_len + 1;
             // alloc some more space in the header and copy the piggyback information
             packetbuf_hdralloc(sizeof(tree_connection));
             tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};
             // TODO: Here I am doing a STRONG assumption. That the packetbuf_hdralloc is CONTIGUOUS with the data portion of the header.
             memcpy(packetbuf_hdrptr() + sizeof(int) + sizeof(upward_data_packet_header), &tc, sizeof(tree_connection));
         }
        // rewrite packet packet header
        enum packet_type pt = data_packet;
        memcpy(packetbuf_hdrptr(), &pt, sizeof(int));
        // update header hop count
        hdr.hops = hdr.hops + 1;
        memcpy(packetbuf_hdrptr() + sizeof(int), &hdr, sizeof(upward_data_packet_header));
        // copy updated struct to packet header
        unicast_send(&conn->uc, &conn->parent);
    }
}

/*
    Packet forwarding from a node to another node in the path computed by the sink.
    The node first checks that it is indeed the next hop in the path, then reduces the
    header (removing itself) and sends the packet to the next node in the path.
*/
void forward_downward_data(my_collect_conn *conn, const linkaddr_t *sender) {

    linkaddr_t addr;
    downward_data_packet_header hdr;

    memcpy(&hdr, packetbuf_dataptr() + sizeof(int), sizeof(downward_data_packet_header));
    // Get first address in path
    memcpy(&addr, packetbuf_dataptr() + sizeof(int) + sizeof(downward_data_packet_header), sizeof(linkaddr_t));

    if (linkaddr_cmp(&addr, &linkaddr_node_addr)) {
        if (hdr.path_len == 1) {
            // this is the last hop
            // read message data and print it
            // uint16_t seqn;
            // memcpy(&seqn, packetbuf_dataptr(), sizeof(uint16_t));
            // printf("Node %02x:%02x: seqn %d from sink\n",
            //         linkaddr_node_addr.u8[0],
            //         linkaddr_node_addr.u8[1],
            //         seqn);
            conn->callbacks->sr_recv(conn, hdr.hops +1 );
        } else {
            // if the next hop is indeed this node
            // reduce header and decrease path length
            packetbuf_hdrreduce(sizeof(linkaddr_t));
            hdr.path_len = hdr.path_len - 1;
            enum packet_type pt = source_routing;
            memcpy(packetbuf_dataptr(), &pt, sizeof(int));
            memcpy(packetbuf_dataptr() + sizeof(int), &hdr, sizeof(downward_data_packet_header));
            // get next addr in path
            memcpy(&addr, packetbuf_dataptr()+sizeof(int)+sizeof(downward_data_packet_header), sizeof(linkaddr_t));
            unicast_send(&conn->uc, &addr);
        }
    } else {
        printf("ERROR: Node %02x:%02x received sr message. Was meant for node %02x:%02x\n",
            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], addr.u8[0], addr.u8[1]);
    }
}
