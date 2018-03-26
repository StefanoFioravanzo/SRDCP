#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "my_collect.h"

// Callbacks structure to initialize the communication channels
struct broadcast_callbacks bc_cb = {.recv=bc_recv};
struct unicast_callbacks uc_cb = {.recv=uc_recv};

void my_collect_open(struct my_collect_conn* state, uint16_t channels,
                    bool is_sink, const struct my_collect_callbacks* callbacks)
{
    // initialize the node state structure
    linkaddr_copy(&state->parent, &linkaddr_null);  // no parent
    state->hop_count = 65535;  // max int: node not connected
    state->beacon_seqn = 0;
    state->callbacks = callbacks;

    if (is_sink) {
        state->sink = 1;
    } else {
        state->sink = 0;
    }

    // open the connection primitives
    broadcast_open(&state->bc, channels, &bc_cb);
    unicast_open(&state->uc, channels + 1, &uc_cb);

    // in case this is the sink, start the beacon propagation process
    if (state->sink) {
        state->hop_count=0;
        // start a timer to send the beacon. Pass to the timer the state object
        ctimer_set(&state->beacon_timer, CLOCK_SECOND, beacon_timer_cb, state);
    }
}

/*
------------ TIMER Callbacks ------------
*/

void beacon_timer_cb(void* ptr) {
    struct my_collect_conn* state = ptr;
    send_beacon(state);
    // in case this is the sink, schedule next broadcast in random time
    if (state->sink) {
        ctimer_set(&state->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, state);
    }
}

/*
------------ SEND and RECEIVE functions ------------
*/

void send_beacon(struct my_collect_conn* state) {
    // init beacon message
    struct beacon_msg beacon = {.seqn = state->beacon_seqn, .hop_count = state->hop_count};

    packetbuf_clear();
    packetbuf_copyfrom(&beacon, sizeof(beacon));
    printf("sending beacon: seqn %d metric %d\n", state->beacon_seqn, state->hop_count);
    broadcast_send(&state->bc);
}

void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
    struct beacon_msg beacon;
    int8_t rssi;

    // Get the pointer to the overall structure my_my_collect_conn from its field bc
    struct my_collect_conn* state = (struct my_collect_conn*)(((uint8_t*)bc_conn) -
                                    offsetof(struct my_collect_conn, bc));

    if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
        printf("broadcast of wrong size\n");
        return;
    }

    memcpy(&beacon, packetbuf_dataptr(), sizeof(struct beacon_msg));
    rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    printf("recv beacon from %02x:%02x seqn %u metric %u rssi %d\n",
        sender->u8[0], sender->u8[1],
        beacon.seqn, beacon.hop_count, rssi);

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
                state->hop_count, beacon.hop_count);
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
    printf("Schedule retransmission");
    ctimer_set(&state->beacon_timer, BEACON_FORWARD_DELAY, beacon_timer_cb, state);
}

int my_collect_send(struct my_collect_conn *state) {
    // initialize header
    struct data_packet_header hdr = {.source=linkaddr_node_addr, .hops=0};

    if (linkaddr_cmp(&state->parent, &linkaddr_null)) {  // in case the node has no parent
        return 0;
    }

    // allocate space for the header
    packetbuf_hdralloc(sizeof(struct data_packet_header));
    memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct data_packet_header));

    return unicast_send(&state->uc, &state->parent);
}

void uc_recv(struct unicast_conn *conn, const linkaddr_t *sender) {
    // Get the pointer to the overall structure my_my_collect_conn from its field uc
    struct my_collect_conn* state = (struct my_collect_conn*)(((uint8_t*)conn) -
                                    offsetof(struct my_collect_conn, uc));

    struct data_packet_header hdr;

    if (packetbuf_datalen() < sizeof(struct data_packet_header)) {
        printf("too short unicast packet %d\n", packetbuf_datalen());
        return;
    }

    memcpy(&hdr, packetbuf_dataptr(), sizeof(struct data_packet_header));
     // if this is the sink
     if (state->sink){
         packetbuf_hdrreduce(sizeof(struct data_packet_header));
         state->callbacks->recv(&hdr.source, hdr.hops+1 );
     }else{
         // update struct
         hdr.hops = hdr.hops + 1;
         memcpy(packetbuf_dataptr(), &hdr, sizeof(struct data_packet_header));
         // copy updated struct to packet header
         unicast_send(&state->uc, &state->parent);
     }
}
