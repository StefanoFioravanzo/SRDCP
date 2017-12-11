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

// in case this is a normal node, forward to parent
// in case this is the sink, collect header and call callback to sent data to application layer
void many_to_one(my_collect_conn *conn, const linkaddr_t *sender) {
    data_packet_header hdr;
    memcpy(&hdr, packetbuf_dataptr(), sizeof(data_packet_header));

    // if this is the sink
    if (conn->is_sink){
        uint8_t piggy_len = hdr.piggy_len;
        packetbuf_hdrreduce(sizeof(data_packet_header));
        if (piggy_len > 0) {  // it mens there is some piggybacking
            // read the piggyybacked information and update the tree dictionary
            tree_connection piggy_info;
            size_t i;
            for (i = 0; i < piggy_len; i++) {
                memcpy(&piggy_info, packetbuf_dataptr(), sizeof(tree_connection));
                // TODO: add piggy info to dictionary
            }
        }
        // application receive callback
        conn->callbacks->recv(&hdr.source, hdr.hops +1 );
    }else{
        uint8_t piggy_flag = 0; // TODO:put here a call to some function that decides if to do piggyback
         if (piggy_flag) {
             //TODO: update piggy_len field in the header struct
             hdr.piggy_len = hdr.piggy_len + 1;
             // alloc some more space in the header and copy the piggyback information
             packetbuf_hdralloc(sizeof(tree_connection));
             tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};
             memcpy(packetbuf_dataptr() + sizeof(data_packet_header), &tc, sizeof(tree_connection));
         }
        // update header hop count
        hdr.hops = hdr.hops + 1;
        memcpy(packetbuf_dataptr(), &hdr, sizeof(data_packet_header));
        // copy updated struct to packet header
        unicast_send(&conn->uc, &conn->parent);
    }
}

// from the sink to a single node
// initiated by sr_send() function. Unpack the header, check next destination, reduce the header, send.
void one_to_many(my_collect_conn *conn, const linkaddr_t *sender) {

    // get the lenght of the path and check the this is the correct hop
    // (that this is the node of the next address in the path)
    uint8_t path_len;
    linkaddr_t current;
    linkaddr_t next;

    memcpy(&path_len, packetbuf_hdrptr(), sizeof(uint8_t));
    memcpy(&current, packetbuf_hdrptr() + sizeof(uint8_t), sizeof(linkaddr_t));

    if (linkaddr_cmp(&current, &linkaddr_node_addr)) {
        if (path_len == 1) {
            // this is the last hop
            // read message data and print it
            uint16_t seqn;
            memcpy(&seqn, packetbuf_dataptr(), sizeof(uint16_t));
            printf("Node %02x:%02x: seqn %d from sink\n",
                    linkaddr_node_addr.u8[0],
                    linkaddr_node_addr.u8[1],
                    seqn);
        } else {
            // if the next hop is indeed this node
            // reduce header and decrease path length
            packetbuf_hdrreduce(sizeof(linkaddr_t));
            path_len = path_len - 1;
            memcpy(packetbuf_hdrptr(), &path_len, sizeof(uint8_t));

            memcpy(&next, packetbuf_hdrptr()+sizeof(uint8_t), sizeof(linkaddr_t));
            unicast_send(&conn->uc, &next);
        }
    } else {
        // TODO: error
    }
}
