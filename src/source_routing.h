#ifndef SOURCE_ROUTING_H
#define SOURCE_ROUTING_H

#include <stdbool.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "net/netstack.h"
#include "core/net/linkaddr.h"

#define MAX_NODES 30
#define MAX_PATH_LENGTH 10

#define BEACON_INTERVAL (CLOCK_SECOND*60)
#define BEACON_FORWARD_DELAY (random_rand() * CLOCK_SECOND)
// time after which the node has to have completed the parent communication
// rand() % (max_number + 1 - minimum_number) + minimum_number
#define PARENT_REFRESH_RATE (rand() % (60 + 1 - 30) + 30)

#define RSSI_REJECTION_TRESHOLD -95

enum packet_type {
    data_packet = 0,
    pyggiback = 1,
    topology_report = 2
};

/* Connection object */
typedef struct node_state {
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
    uint16_t hop_count;
    // sequence number of the tree protocol
    uint16_t beacon_seqn;
    // true if this node is the sink
    uint8_t sink;  // 1: is_sink, 0: not_sink
    // tree table (used only in the sink)
    // TODO: can we put this in some structure usd only by the sink?
    TreeDict routing_table;
} node_state;

// receiver functions for communications channels
void bc_recv(struct broadcast_conn*, const linkaddr_t*);
void uc_recv(struct unicast_conn*, const linkaddr_t*);

// Callbacks structure to initialize the communication channels
struct broadcast_callbacks bc_cb = {.recv=bc_recv};
struct unicast_callbacks uc_cb = {.recv=uc_recv};

// timers Callbacks
void beacon_timer_cb(void*);
void dedicated_topology_report_timer_cb(void*);

// -------- UTIL FUNCTIONS --------

// sends a broadcast beacon using the current sequence number and hop count
void send_beacon(node_state*);
void bc_recv(struct broadcast_conn*, const linkaddr_t*);
int send_data(node_state*);
void uc_recv(struct unicast_conn*, const linkaddr_t*);
void send_topology_report()

// -------- MESSAGE STRUCTURES --------

struct beacon_message {
    uint16_t seqn;
    uint16_t hop_count;
} __attribute__((packed));
typedef struct beacon_message beacon_message;

struct data_packet_header {
    packet_type type;
    linkaddr_t source;
    uint8_t hops;
    uint8_t pyggi_len;  // 0 in case there is no piggybacking
} __attribute__((packed));
typedef struct data_packet_header data_packet_header;

struct tree_connection {
    linkaddr_t node;
    linkaddr_t parent;
};
typedef struct tree_connection tree_connection;

struct topology_report_header {
    packet_type type;
    // number of
    uint8_t data_len;
} __attribute__((packed));
typedef struct topology_report_header topology_report_header;

#endif // SOURCE_ROUTING_H
