#ifndef SOURCE_ROUTING_H
#define SOURCE_ROUTING_H

#include <stdbool.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "net/netstack.h"
#include "core/net/linkaddr.h"

#define BEACON_INTERVAL (CLOCK_SECOND*60)
#define BEACON_FORWARD_DELAY (random_rand() * CLOCK_SECOND)

#define RSSI_REJECTION_TRESHOLD -95

/* Connection object */
struct node_state {
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
    // metric: hop count
    uint16_t hop_count;
    // sequence number of the tree protocol
    uint16_t beacon_seqn;
    // true if this node is the sink
    uint8_t sink;  // 1: is_sink, 0: not_sink
};

// receiver functions for communications channels
void bc_recv(struct broadcast_conn *conn, const linkaddr_t *sender);
void uc_recv(struct unicast_conn *c, const linkaddr_t *from);

// Callbacks structure to initialize the communication channels
struct broadcast_callbacks bc_cb = {.recv=bc_recv};
struct unicast_callbacks uc_cb = {.recv=uc_recv};

// timers Callbacks
void beacon_timer_cb(void* ptr);

// -------- UTIL FUNCTIONS --------

// sends a broadcast beacon using the current sequence number and hop count
void send_beacon(struct node_state*);
void bc_recv(struct broadcast_conn*, const linkaddr_t*);
int send_data(struct node_state*);
void uc_recv(struct unicast_conn*, const linkaddr_t*);

// -------- MESSAGE STRUCTURES --------

struct beacon_message {
    uint16_t seqn;
    uint16_t hop_count;
} __attribute__((packed));

struct data_packet_header {
    linkaddr_t source;
    uint8_t hops;
} __attribute__((packed));

#endif // SOURCE_ROUTING_H
