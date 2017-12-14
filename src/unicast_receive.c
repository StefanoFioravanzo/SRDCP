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
    Forwarding or collection of data sent from one node to the sink.

        - Sink: Counts the number of topology informations with piggy_len present in the header.
        Then calls the callback to the application layer to retrieve the packet data.
        - Node: A node just forwards upwards the message. In case it wants to piggyback
        some topology information (its parent) it increases header size and writes in the header
        its parent.
*/
void forward_upward_data(my_collect_conn *conn, const linkaddr_t *sender) {
    upward_data_packet_header hdr;
    memcpy(&hdr, packetbuf_dataptr(), sizeof(upward_data_packet_header));

    // if this is the sink
    if (conn->is_sink){
        uint8_t piggy_len = hdr.piggy_len;
        packetbuf_hdrreduce(sizeof(upward_data_packet_header));
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
        uint8_t piggy_flag = 1; // TODO:put here a call to some function that decides if to do piggyback
         if (piggy_flag) {
             hdr.piggy_len = hdr.piggy_len + 1;
             // alloc some more space in the header and copy the piggyback information
             packetbuf_hdralloc(sizeof(tree_connection));
             tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};
             memcpy(packetbuf_dataptr() + sizeof(upward_data_packet_header), &tc, sizeof(tree_connection));
         }
        // update header hop count
        hdr.hops = hdr.hops + 1;
        memcpy(packetbuf_dataptr(), &hdr, sizeof(upward_data_packet_header));
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

    memcpy(&hdr, packetbuf_hdrptr(), sizeof(downward_data_packet_header));
    // Get first address in path
    memcpy(&addr, packetbuf_hdrptr() + sizeof(downward_data_packet_header), sizeof(linkaddr_t));

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
            memcpy(packetbuf_hdrptr(), &hdr, sizeof(downward_data_packet_header));
            // get next addr in path
            memcpy(&addr, packetbuf_hdrptr()+sizeof(downward_data_packet_header), sizeof(linkaddr_t));
            unicast_send(&conn->uc, &addr);
        }
    } else {
        printf("ERROR: Node %02x:%02x received sr message. Was meant for node %02x:%02x\n",
            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], addr.u8[0], addr.u8[1]);
    }
}
