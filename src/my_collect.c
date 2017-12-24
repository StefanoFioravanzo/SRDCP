#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "my_collect.h"

/*--------------------------------------------------------------------------------------*/
/* Callback structures */
struct broadcast_callbacks bc_cb = {.recv=bc_recv};
struct unicast_callbacks uc_cb = {.recv=uc_recv};

// -------------------------------------------------------------------------------------------------
//                                      DICT IMPLEMENTATION
// -------------------------------------------------------------------------------------------------

void print_dict_state(TreeDict* dict) {
    int i;
    for (i = 0; i < dict->len; i++) {
        printf("\tDictEntry %d: node %02x:%02x - parent %02x:%02x\n",
            i,
            dict->entries[i].key.u8[0],
            dict->entries[i].key.u8[1],
            dict->entries[i].value.u8[0],
            dict->entries[i].value.u8[1]);
    }
}

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
   if (idx != -1) {  // Element already present, update its value
       linkaddr_copy(&dict->entries[idx].value, &value);
       return 0;
   }
   linkaddr_copy(&dict->entries[dict->len].key, &key);
   linkaddr_copy(&dict->entries[dict->len].value, &value);
   dict->len++;
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

void print_route(my_collect_conn* conn, uint8_t route_len) {
    uint8_t i;
    printf("Sink route to node:\n");
    for (i = 0; i < route_len; i++) {
        printf("\t%d: %02x:%02x\n",
            i,
            conn->routing_table.tree_path[i].u8[0],
            conn->routing_table.tree_path[i].u8[1]);
    }
}

// -------------------------------------------------------------------------------------------------

void my_collect_open(struct my_collect_conn* conn, uint16_t channels,
                     bool is_sink, const struct my_collect_callbacks *callbacks)
{
    // initialise the connector structure
    linkaddr_copy(&conn->parent, &linkaddr_null);
    conn->metric = 65535; // the max metric (means that the node is not connected yet)
    conn->beacon_seqn = 0;
    conn->callbacks = callbacks;

    if (is_sink) {
        conn->is_sink = 1;
    }else {
        conn->is_sink = 0;
    }

    // open the underlying primitives
    broadcast_open(&conn->bc, channels,     &bc_cb);
    unicast_open  (&conn->uc, channels + 1, &uc_cb);

    if (is_sink) {
        conn->metric = 0;
        conn->routing_table.len = 0;
        // we pass the connection object conn to the timer callback
        ctimer_set(&conn->beacon_timer, CLOCK_SECOND, beacon_timer_cb, conn);
    }
}

/*
------------ TIMER Callbacks ------------
*/

void topology_report_timer_cb(void* ptr) {
    struct my_collect_conn *conn = ptr;
    send_topology_report(conn, 0);

    ctimer_set(&conn->topology_report_timer, TOPOLOGY_REPORT_INTERVAL_RAND, topology_report_timer_cb, conn);
}

// Beacon timer callback
void beacon_timer_cb(void* ptr) {
    struct my_collect_conn *conn = ptr;
    send_beacon(conn);
    if (conn->is_sink == 1) {
      // we pass the connection object conn to the timer callback
      ctimer_set(&conn->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, conn);
    }
}

/*
------------ SEND and RECEIVE functions ------------
*/

/*
    When the sink receives a topology report, it has to read the tree_connection
    structure in the packet and update the node's parent.
*/
void deliver_topology_report_to_sink(my_collect_conn* conn) {
    // remove header information
    packetbuf_hdrreduce(sizeof(enum packet_type));
    tree_connection tc;
    memcpy(&tc, packetbuf_dataptr(), sizeof(tree_connection));

    printf("Sink: received topology report. Updating parent of node %02x:%02x\n", tc.node.u8[0], tc.node.u8[1]);
    dict_add(&conn->routing_table, tc.node, tc.parent);
    print_dict_state(&conn->routing_table);
}

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
    enum packet_type pt = topology_report;
    tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};

    packetbuf_clear();
    packetbuf_set_datalen(sizeof(tree_connection));
    memcpy(packetbuf_dataptr(), &tc, sizeof(tree_connection));

    packetbuf_hdralloc(sizeof(enum packet_type));
    memcpy(packetbuf_hdrptr(), &pt, sizeof(enum packet_type));
    unicast_send(&conn->uc, &conn->parent);
}

/*
    The node sends a beacon in broadcast to everyone.
*/
void send_beacon(struct my_collect_conn* conn) {
    struct beacon_msg beacon = {.seqn = conn->beacon_seqn, .metric = conn->metric};

    packetbuf_clear();
    packetbuf_copyfrom(&beacon, sizeof(beacon));
    printf("my_collect: sending beacon: seqn %d metric %d\n", conn->beacon_seqn, conn->metric);
    broadcast_send(&conn->bc);
}

// Beacon receive callback
void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
    struct beacon_msg beacon;
    int8_t rssi;
    // Get the pointer to the overall structure my_collect_conn from its field bc
    struct my_collect_conn* conn = (struct my_collect_conn*)(((uint8_t*)bc_conn) -
    offsetof(struct my_collect_conn, bc));

    if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
        printf("my_collect: broadcast of wrong size\n");
        return;
    }
    memcpy(&beacon, packetbuf_dataptr(), sizeof(struct beacon_msg));
    rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    printf("my_collect: recv beacon from %02x:%02x seqn %u metric %u rssi %d\n",
      sender->u8[0], sender->u8[1],
      beacon.seqn, beacon.metric, rssi);

    if (rssi < RSSI_THRESHOLD) {
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
            printf("my_collect: return. conn->metric: %u, beacon.metric: %u\n",
                conn->metric, beacon.metric);
            return;
      }
  }
  // update metric (hop count)
  conn->metric = beacon.metric+1;
  // update parent
  //conn->parent = *sender;  // Dereference sender?
  // linkaddr_copy(&conn->parent, sender);

  // Parent update
  if (!linkaddr_cmp(&conn->parent, sender)) {
        // update parent
        linkaddr_copy(&conn->parent, sender);
        if (TOPOLOGY_REPORT) {
            // send a topology report using the timer callback (RANDOM_DELAY: small time)
            ctimer_set(&conn->topology_report_timer, RANDOM_DELAY, topology_report_timer_cb, conn);
        }
    }

  // Retransmit the beacon since the metric has been updated.
  // Introduce small random delay with the BEACON_FORWARD_DELAY to avoid synch
  // issues when multiple nodes broadcast at the same time.
  ctimer_set(&conn->beacon_timer, BEACON_FORWARD_DELAY, beacon_timer_cb, conn);
}

// Our send function
int my_collect_send(struct my_collect_conn *conn) {
    struct upward_data_packet_header hdr = {.source=linkaddr_node_addr, .hops=0};

    enum packet_type pt = upward_data_packet;

    if (linkaddr_cmp(&conn->parent, &linkaddr_null))
        return 0; // no parent

    packetbuf_hdralloc(sizeof(enum packet_type) + sizeof(upward_data_packet_header));
    memcpy(packetbuf_hdrptr(), &pt, sizeof(enum packet_type));
    memcpy(packetbuf_hdrptr() + sizeof(enum packet_type), &hdr, sizeof(upward_data_packet_header));

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
    struct my_collect_conn* conn = (struct my_collect_conn*)(((uint8_t*)uc_conn) -
        offsetof(struct my_collect_conn, uc));

    // TODO: Need this?
    // if (packetbuf_datalen() < sizeof(upward_data_packet_header)) {
    //     printf("my_collect: too short unicast packet %d\n", packetbuf_datalen());
    //     return;
    // }

    enum packet_type pt;
    memcpy(&pt, packetbuf_dataptr(), sizeof(enum packet_type));

    printf("Node %02x:%02x received unicast packet with type %d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], pt);
    switch (pt) {
        case upward_data_packet:
            printf("Node %02x:%02x receivd a unicast data packet\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            forward_upward_data(conn, sender);
            break;
        case topology_report:
            printf("Node %02x:%02x receivd a unicast topology report\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            if (conn->is_sink) {
                deliver_topology_report_to_sink(conn);
            } else {
                if (TOPOLOGY_REPORT) {
                    send_topology_report(conn, 1);  // 1: forwarding set to true
                }
            }
            break;
        case downward_data_packet:
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
int sr_send(struct my_collect_conn* conn, const linkaddr_t* dest) {
    // the sink sends a packet to `dest`.

    if (!conn->is_sink) {
        // if this is an ordinary node
        return 0;
    }

    // populate the array present in the source_routing structure in conn.
    int path_len = find_route(conn, dest);
    // print_route(conn, path_len);
    if (path_len == 0) {
        return 0;
    }

    enum packet_type pt = downward_data_packet;
    downward_data_packet_header hdr = {.hops=0, .path_len=path_len };

    // allocate enough space in the header for the path
    packetbuf_hdralloc(sizeof(enum packet_type) + sizeof(downward_data_packet_header) + sizeof(linkaddr_t) * path_len);
    memcpy(packetbuf_hdrptr(), &pt, sizeof(enum packet_type));
    memcpy(packetbuf_hdrptr() + sizeof(enum packet_type), &hdr, sizeof(downward_data_packet_header));
    int i;
    // copy path in inverse order.
    for (i = path_len-1; i >= 0; i--) {  // path len because to insert the Nth element I do sizeof(linkaddr_t)*(N-1)
        memcpy(packetbuf_hdrptr()+sizeof(enum packet_type)+sizeof(downward_data_packet_header)+sizeof(linkaddr_t)*(path_len-(i+1)),
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
    memcpy(&hdr, packetbuf_dataptr() + sizeof(enum packet_type), sizeof(upward_data_packet_header));
    // if this is the sink
    if (conn->is_sink == 1){
        packetbuf_hdrreduce(sizeof(enum packet_type) + sizeof(upward_data_packet_header));
        conn->callbacks->recv(&hdr.source, hdr.hops +1 );
    }else{
        // copy pakcet header to local struct

        // update struct
        hdr.hops = hdr.hops+1;
        memcpy(packetbuf_dataptr() + sizeof(enum packet_type), &hdr, sizeof(upward_data_packet_header));
        // copy updated struct to packet header
        // memcpy(&hdr, packetbuf_hdrptr(), sizeof(struct collect_header));
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

    memcpy(&hdr, packetbuf_dataptr() + sizeof(enum packet_type), sizeof(downward_data_packet_header));
    // Get first address in path
    memcpy(&addr, packetbuf_dataptr() + sizeof(enum packet_type) + sizeof(downward_data_packet_header), sizeof(linkaddr_t));

    if (linkaddr_cmp(&addr, &linkaddr_node_addr)) {
        if (hdr.path_len == 1) {
            // printf("Node %02x:%02x: seqn %d from sink\n",
            //         linkaddr_node_addr.u8[0],
            //         linkaddr_node_addr.u8[1],
            //         seqn);
            packetbuf_hdrreduce(sizeof(enum packet_type) + sizeof(downward_data_packet_header) + sizeof(linkaddr_t));
            conn->callbacks->sr_recv(conn, hdr.hops +1 );
        } else {
            // if the next hop is indeed this node
            // reduce header and decrease path length
            packetbuf_hdrreduce(sizeof(linkaddr_t));
            hdr.path_len = hdr.path_len - 1;
            enum packet_type pt = downward_data_packet;
            memcpy(packetbuf_dataptr(), &pt, sizeof(enum packet_type));
            memcpy(packetbuf_dataptr() + sizeof(enum packet_type), &hdr, sizeof(downward_data_packet_header));
            // get next addr in path
            memcpy(&addr, packetbuf_dataptr()+sizeof(enum packet_type)+sizeof(downward_data_packet_header), sizeof(linkaddr_t));
            unicast_send(&conn->uc, &addr);
        }
    } else {
        printf("ERROR: Node %02x:%02x received sr message. Was meant for node %02x:%02x\n",
            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], addr.u8[0], addr.u8[1]);
    }
}
