#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "source_routing.h"
#include "dictionary.c"

void connection_open(node_state* state, uint16_t channels,
                    bool is_sink, const struct custom_conn_callbacks* callbacks)
{
    // initialize the node state structure
    linkaddr_copy(&state->parent, &linkaddr_null);  // no parent
    state->hop_count = 65535;  // max int: node not connected
    state->beacon_seqn = 0;
    conn->callbacks = callbacks;

    if (is_sink) {
        state->sink = 1;
    } else {
        state->sink = 0;
    }

    // open the connection primitives
    broadcast_open(&state->bc, channels, &bc_cb);
    unicast_open(&state->uc, channels + 1, &uc_cb);

    // in case this is the sink, start the beacon propagation process
    if (state->is_sink) {
        state->metric=0

        // initialize routing table
        //TODO: alloc here routing table


        // start a timer to send the beacon. Pass to the timer the state object
        ctimer_set(&state->beacon_timer, CLOCK_SECOND, beacon_timer_cb, state)
    }
}

/*
------------ TIMER Callbacks ------------
*/

void beacon_timer_cb(void* ptr) {
    node_state* state = ptr;
    send_beacon(state)
    // in case this is the sink, schedule next broadcast in random time
    if (state->sink) {
        ctimer_set(&state->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, state);
    }
}

/*
------------ SEND and RECEIVE functions ------------
*/

void send_beacon(node_state* state) {
    // init beacon message
    beacon_msg beacon = {.seqn = state->beacon_seqn, .hop_count = state->hop_count};

    packetbuf_clear();
    packetbuf_copyfrom(&beacon, sizeof(beacon));
    printf("sending beacon: seqn %d metric %d\n", state->beacon_seqn, state->hop_count);
    broadcast_send(&state->bc);
}

void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
    beacon_msg beacon;
    int8_t rssi;

    // Get the pointer to the overall structure my_collect_conn from its field bc
    node_state* state = (node_state*)(((uint8_t*)bc_conn) -
                                    offsetof(node_state, bc));

    if (packetbuf_datalen() != sizeof(beacon_msg)) {
        printf("broadcast of wrong size\n");
        return;
    }

    memcpy(&beacon, packetbuf_dataptr(), sizeof(beacon_msg));
    rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    printf("recv beacon from %02x:%02x seqn %u metric %u rssi %d\n",
        sender->u8[0], sender->u8[1],
        beacon.seqn, beacon.metric, rssi);

    if (rssi < RSSI_REJECTION_TRESHOLD) {
        printf("packet reject due to low rssi");
        return;
    }

    // check received sequence number
    if (state->beacon_seqn < beacon.seqn){
        // new tree
        state->beacon_seqn = beacon.seqn;

    }else{
        if (state->hop_count < beacon.hop_count) {
            // current hop count is better
            printf("return. state->hop_count: %u, beacon.hop_count: %u\n",
                state->metric, beacon.metric);
            return;
      }
    }
    // update metric
    state->hop_count = beacon.hop_count + 1;
    // update parent
    linkaddr_copy(&state->parent, sender);

    // Retransmit the beacon since the metric has been updated.
    // Introduce small random delay with the BEACON_FORWARD_DELAY to avoid synch
    // issues when multiple nodes broadcast at the same time.
    ctimer_set(&state->beacon_timer, BEACON_FORWARD_DELAY, beacon_timer_cb, state);
}

int send_data(node_state *state) {
    // TODO: decide whether to send pyggiback or not.
    uint8_t piggy_flag = 0 // TODO:put here a call to some function that decides if to do piggyback
    // initialize header
    data_packet_header hdr = {.type=packet_type.pyggiback, .source=linkaddr_node_addr,
                                    .hops=0, .pyggi_len=0 };
    if (piggy_flag) {
        hdr.pyggi_len = 1;
    }

    // set current pyggibacking structure
    tree_connection tc = {.node=linkaddr_node_addr, .parent=state->parent};

    if (linkaddr_cmp(&state->parent, &linkaddr_null)) {  // in case the node has no parent
        return 0;
    }

    // allocate space for the header
    if (piggy_flag) {
        packetbuf_hdralloc(sizeof(data_packet_header) + sizeof(tree_connection));
        memcpy(packetbuf_hdrptr(), &hdr, sizeof(data_packet_header));
        memcpy(packetbuf_hdrptr() + sizeof(data_packet_header), &tc, sizeof(tree_connection));
    } else {
        packetbuf_hdralloc(sizeof(data_packet_header));
        memcpy(packetbuf_hdrptr(), &hdr, sizeof(data_packet_header));
    }

    return unicast_send(&state->uc, &state->parent);
}

void uc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
    // Get the pointer to the overall structure my_collect_conn from its field uc
    node_state* state = (node_state*)(((uint8_t*)uc_conn) -
                                    offsetof(node_state, uc));

    data_packet_header hdr;

    //TODO:?
    // if (packetbuf_datalen() < sizeof(struct data_packet_header)) {
    //     printf("too short unicast packet %d\n", packetbuf_datalen());
    //     return;
    // }

    memcpy(&hdr, packetbuf_dataptr(), sizeof(data_packet_header));
     // if this is the sink
     if (conn->sink){
         uint8_t piggy_len = &hdr->pyggi_len;
         packetbuf_hdrreduce(sizeof(data_packet_header));
         if (pyggi_len > 0) {  // it mens there is some piggybacking
             // read the piggyybacked information and update the tree dictionary
             tree_connection piggy_info;
             for (size_t i = 0; i < pyggi_len; i++) {
                 memcpy(&piggy_info, packetbuf_dataptr(), sizeof(tree_connection));
                 // TODO: add piggy info to dictionary
             }
         }
         // application receive callback
         conn->callbacks->recv(&hdr.source, hdr.hops +1 );
     }else{
         uint8_t piggy_flag = 0 // TODO:put here a call to some function that decides if to do piggyback
          if (piggy_flag) {
              // alloc some more space in the header and copy the piggyback information
              packetbuf_hdralloc(sizeof(tree_connection));
              tree_connection tc = {.node=linkaddr_node_addr, .parent=state->parent};
              memcpy(packetbuf_dataptr() + sizeof(data_packet_header), &tc, sizeof(tree_connection));
          }
         // update header hop count
         hdr.hops = hdr.hops + 1;
         memcpy(packetbuf_dataptr(), &hdr, sizeof(data_packet_header));
         // copy updated struct to packet header
         unicast_send(&state->uc, &state->parent);
     }
}
