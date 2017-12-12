#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"

#include "my_collect.h"
#include "unicast_receive.h"

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
TODO: detect loop when building the path on the sink.
TODO: Beware of collisions mostly during bootstrap when nodes join the network
    or change the parent. May have collisions between dedicated topology reports and beacons.
    How to avoid this?
    >> Idea: Have two un synced random frames.
    The first range of time is used for sink beacons, the second for sending topology report.
    This considering instantaneous transmission time (wrt time ranges).
TODO: Test protocol both with NullRDC and ContikiMAC.
TODO: Assume a maximum path length and a max number of nodes.

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
    linkaddr_t ret = (idx == -1 ? linkaddr_null : dict->entries[idx].value);
    return ret;
}

int dict_add(TreeDict* dict, const linkaddr_t key, linkaddr_t value) {
    /*
    Adds a new entry to the Dictionary
    In case the key already exists, it replaces the value
    */
   int idx = dict_find_index(dict, key);
   linkaddr_t dst_value, dst_key;
   linkaddr_copy(&dst_value, &value);
   linkaddr_copy(&dst_key, &key);
   if (idx != -1) {  // Element already present, update its value
       dict->entries[idx].value = dst_value;
       return 0;
   }

   // first initialization
   // if (dict->len == 0) {
   //     dict->len = 1;
   //     dict->cap = 10;
   //     // alloc array of one entry
   //     // dict->
   // }

   if (dict->len == dict->cap) {
       printf("Dict len > Dict cap");
       return -1;
       // dict->cap *= 2;
       // // safe reallocation with tmp variable
       // DictEntry* tmp = realloc(dict->entries, dict->cap * sizeof(DictEntry));
       // if (!tmp) {
       //     // could not resize, handle exception
       //     printf("Could not resize entries in dictionary. ")
       //     return -1;
       // } else {
       //     dict->entries = tmp;
       // }
   }
   dict->len++;
   dict->entries[dict->len].key = dst_key;
   dict->entries[dict->len].value = dst_value;
   return 0;
}
// -------------------------------------------------------------------------------------------------

// receiver functions for communications channels
void bc_recv(struct broadcast_conn*, const linkaddr_t*);
void uc_recv(struct unicast_conn*, const linkaddr_t*);

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
        //TODO: alloc here routing table
        // no need to initialize since the first initialization is done
        // during the first entry insert.
        TreeDict rt = {.len=0, .cap=MAX_NODES};
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

void dedicated_topology_report_timer_cb(void* ptr) {
    my_collect_conn* conn = ptr;
    send_topology_report(conn, 0);  // 0: first send, no forwarding

    ctimer_set(&conn->topology_report_timer,
        PARENT_REFRESH_RATE,
        dedicated_topology_report_timer_cb,
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

    packetbuf_clear();

    // TODO: explore possibility of using data space instead of header.
    // How would I increase data allocation space for the packet? E.g. similar to packetbuf_hdralloc()
    // memcpy(packetbuf_dataptr(), &msg, sizeof(msg));
    // packetbuf_set_datalen(sizeof(msg));

    // No actual data in the packet, data present in the header

    topology_report_header hdr = {.type=topology_report};
    tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};

    packetbuf_hdralloc(sizeof(topology_report_header) + sizeof(tree_connection));
    memcpy(packetbuf_hdrptr(), &hdr, sizeof(topology_report_header));
    memcpy(packetbuf_hdrptr() + sizeof(topology_report_header), &tc, sizeof(tree_connection));

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
        printf("packet rejected due to low rssi");
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
    // update parent
    linkaddr_copy(&conn->parent, sender);

    // since we have updated the parent, send the dedicated topology report.
    send_topology_report(conn, 0);

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
    uint8_t piggy_flag = 1;  // TODO:put here a call to some function that decides if to do piggyback
    // initialize header
    data_packet_header hdr = {.type=data_packet, .source=linkaddr_node_addr,
                                    .hops=0, .piggy_len=0 };

    if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {  // in case the node has no parent
        return 0;
    }

    // allocate space for the header
    if (piggy_flag) {
        hdr.piggy_len = 1;
        // topology information to be piggybacked
        tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};

        packetbuf_hdralloc(sizeof(data_packet_header) + sizeof(tree_connection));
        memcpy(packetbuf_hdrptr(), &hdr, sizeof(data_packet_header));
        memcpy(packetbuf_hdrptr() + sizeof(data_packet_header), &tc, sizeof(tree_connection));
    } else {
        packetbuf_hdralloc(sizeof(data_packet_header));
        memcpy(packetbuf_hdrptr(), &hdr, sizeof(data_packet_header));
    }

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
    memcpy(&pt, packetbuf_hdrptr(), sizeof(int));

    switch (pt) {
        case data_packet:
            forward_upward_data(conn, sender);
            break;
        case topology_report:
            send_topology_report(conn, 1);  // 1: forwarding set to true
            break;
        case source_routing:
            forward_downward_data(conn, sender);
            break;
        default:
            printf("Packet type not recognized.");
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

    // need to build the route to the destination node.
    // NOTE: This is a mess since I cannot do dynamic memory allocation. Have to insert the path in the header backwards?
    // count the path lenght from sink to destination node
    uint8_t path_len = 0;
    linkaddr_t next;  // This will be the node in the path right after the sink.
    linkaddr_t parent;
    linkaddr_copy(&parent, dest);
    while (!linkaddr_cmp(&parent, &sink_addr)) {
        linkaddr_copy(&next, &parent);
        parent = dict_find(&conn->routing_table, &parent);
        if (linkaddr_cmp(&parent, &linkaddr_null)) {
            return 0;
        }
        path_len++;
    }

    // allocate enough space in the header for the path
    packetbuf_hdralloc(sizeof(enum packet_type)+sizeof(linkaddr_t)*path_len + sizeof(uint8_t));
    enum packet_type t = source_routing;
    memcpy(packetbuf_hdrptr(), &t, sizeof(int));
    memcpy(packetbuf_hdrptr()+sizeof(uint8_t), &path_len, sizeof(uint8_t));
    int i;
    linkaddr_copy(&parent, dest);
    // path in backward order
    for (i = path_len-1; i > 0; i--) {  // path len because to insert the Nth element I do sizeof(linkaddr_t)*(N-1)
        memcpy(packetbuf_hdrptr()+sizeof(int)+sizeof(uint8_t)+sizeof(linkaddr_t)*(i), &parent, sizeof(linkaddr_t));
        parent = dict_find(&conn->routing_table, &parent);
    }
    return unicast_send(&conn->uc, &next);
}
