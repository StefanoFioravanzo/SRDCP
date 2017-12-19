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
   dict->len++;
   linkaddr_copy(&dict->entries[dict->len].key, &key);
   linkaddr_copy(&dict->entries[dict->len].value, &value);
   return 0;
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

  if (!linkaddr_cmp(&conn->parent, sender)) {
        // update parent
        linkaddr_copy(&conn->parent, sender);
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
            // send_topology_report(conn, 1);  // 1: forwarding set to true
            break;
        case downward_data_packet:
            printf("Node %02x:%02x receivd a unicast source routing packet\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            // forward_downward_data(conn, sender);
            break;
        default:
            printf("Packet type not recognized.\n");
    }
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
