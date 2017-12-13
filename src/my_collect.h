#ifndef MY_COLLECT_H
#define MY_COLLECT_H

#include <stdbool.h>
#include <stdlib.h>
// #include "contiki.h"
// #include "net/rime/rime.h"
// #include "net/netstack.h"
// #include "core/net/linkaddr.h"

#define MAX_NODES 30
#define MAX_PATH_LENGTH 10

#define BEACON_INTERVAL (CLOCK_SECOND*60)
#define BEACON_FORWARD_DELAY (random_rand() * CLOCK_SECOND)
// time after which the node has to have completed the parent communication
// rand() % (max_number + 1 - minimum_number) + minimum_number
#define PARENT_REFRESH_RATE (rand() % (60 + 1 - 30) + 30)

#define RSSI_REJECTION_TRESHOLD -95

static const linkaddr_t sink_addr = {{0x01, 0x00}}; // node 1 will be our sink

enum packet_type {
    data_packet = 0,
    topology_report = 2,
    source_routing = 3
};

// --------------------------------------------------------------------
//                              DICT STRUCTS
// --------------------------------------------------------------------

typedef struct DictEntry {
    linkaddr_t key;  // the address of the node
    linkaddr_t value;  // the address of the parent
} DictEntry;

typedef struct TreeDict {
    int len;
    int cap;
    DictEntry entries[MAX_NODES];
    linkaddr_t tree_path[MAX_PATH_LENGTH];
} TreeDict;

// --------------------------------------------------------------------

/* Connection object */
typedef struct my_collect_conn {
    // broadcast connection object
    struct broadcast_conn bc;
    // unicast connection object
    struct unicast_conn uc;
    // callback to ??
    const struct my_collect_callbacks* callbacks;
    // address of parent node
    linkaddr_t parent;
    // global timer
    struct ctimer beacon_timer;
    struct ctimer topology_report_timer;
    // metric: hop count
    uint16_t metric;
    // sequence number of the tree protocol
    uint16_t beacon_seqn;
    // true if this node is the sink
    uint8_t is_sink;  // 1: is_sink, 0: not_sink
    // tree table (used only in the sink)
    // TODO: can we put this in some structure usd only by the sink?
    TreeDict routing_table;
} my_collect_conn;

struct my_collect_callbacks {
  void (* recv)(const linkaddr_t *originator, uint8_t hops);
  void (* sr_recv)(struct my_collect_conn *ptr, uint8_t hops);
};

// timers Callbacks
void beacon_timer_cb(void*);
void dedicated_topology_report_timer_cb(void*);

// -------- UTIL FUNCTIONS --------

void my_collect_open(my_collect_conn*, uint16_t, bool, const struct my_collect_callbacks*);

// sends a broadcast beacon using the current sequence number and hop count
void send_beacon(my_collect_conn*);
void bc_recv(struct broadcast_conn*, const linkaddr_t*);
int my_collect_send(my_collect_conn*);
void uc_recv(struct unicast_conn*, const linkaddr_t*);
void send_topology_report(my_collect_conn*, uint8_t);

/*
 Source routing send function:
 Params:
    c: pointer to the collection connection structure
    dest: pointer to the destination address
 Returns non-zero if the packet could be sent, zero otherwise.
*/
int sr_send(struct my_collect_conn*, const linkaddr_t*);


// -------- MESSAGE STRUCTURES --------

struct beacon_message {
    uint16_t seqn;
    uint16_t metric;
} __attribute__((packed));
typedef struct beacon_message beacon_message;

struct upward_data_packet_header {
    enum packet_type type;
    linkaddr_t source;
    uint8_t hops;
    uint8_t piggy_len;  // 0 in case there is no piggybacking
} __attribute__((packed));
typedef struct upward_data_packet_header upward_data_packet_header;

struct downward_data_packet_header {
    enum packet_type type;
    uint8_t hops;
    uint8_t path_len;
} __attribute__((packed));
typedef struct downward_data_packet_header downward_data_packet_header;

struct tree_connection {
    linkaddr_t node;
    linkaddr_t parent;
} __attribute__((packed));
typedef struct tree_connection tree_connection;

struct topology_report_header {
    enum packet_type type;
    // number of
    // uint8_t data_len;
} __attribute__((packed));
typedef struct topology_report_header topology_report_header;

#endif // MY_COLLECT_H
