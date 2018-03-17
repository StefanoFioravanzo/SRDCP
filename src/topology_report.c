#include <stdbool.h>
#include <stdio.h>
#include "my_collect.h"
#include "routing_table.h"

/*
------------ TIMER Callbacks ------------
*/

/*
    Called when waiting time to forward a topology report
    has ended.
*/
void topology_report_hold_cb(void* ptr) {
    struct my_collect_conn *conn = ptr;
    if (conn->treport_hold == 1) {
        conn->treport_hold = 0;
        send_topology_report(conn, 0);
    }
}

/*
    Send a topology report, or activate
    some smart logic to handle the need to report
*/
void topology_report_timer_cb(void* ptr) {
    struct my_collect_conn *conn = ptr;
    // send_topology_report(conn, 0);
    conn->treport_hold = 1;

    ctimer_set(&conn->treport_hold_timer, TOPOLOGY_REPORT_HOLD_TIME, topology_report_hold_cb, conn);
    // ctimer_set(&conn->topology_report_timer, TOPOLOGY_REPORT_INTERVAL_RAND, topology_report_timer_cb, conn);
}

/*
------------ Topology Report Management ------------
*/

/*
    When the sink receives a topology report, it has to read the tree_connection
    structure in the packet and update the node's parent.
*/
void deliver_topology_report_to_sink(my_collect_conn* conn) {
    // remove header information
    packetbuf_hdrreduce(sizeof(enum packet_type));
    uint8_t len;
    tree_connection tc;

    memcpy(&len, packetbuf_dataptr(), sizeof(uint8_t));
    printf("Sink: received %d topology reports.", len);
    int i;
    for (i = 0; i < len; i++) {
        memcpy(&tc, packetbuf_dataptr() + sizeof(tree_connection) * i, sizeof(tree_connection));
        printf("Sink: received topology report. Updating parent of node %02x:%02x\n", tc.node.u8[0], tc.node.u8[1]);
        dict_add(&conn->routing_table, tc.node, tc.parent);
    }
    print_dict_state(&conn->routing_table);
}

bool check_address_in_topologyreport_block(my_collect_conn* conn, linkaddr_t node) {
    printf("Checking topology report address: %02x:%02x\n", node.u8[0], node.u8[1]);
    uint8_t len;
    memcpy(&len, packetbuf_dataptr() + sizeof(enum packet_type), sizeof(uint8_t));
    tree_connection tc;
    uint8_t i;
    for (i = 0; i < len; i++) {
        memcpy(&tc,
            packetbuf_dataptr() + sizeof(enum packet_type) + sizeof(uint8_t) + sizeof(tree_connection) * i,
            sizeof(tree_connection));
        if (linkaddr_cmp(&tc.node, &linkaddr_node_addr)) {
            printf("Checking top report address found: %02x:%02x\n", node.u8[0], node.u8[1]);
            return true;
        }
    }
    printf("Checking top report address NOT found: %02x:%02x\n", node.u8[0], node.u8[1]);
    return false;
}

/*
    The node sends a topology report to its parent.
    Topology report is sent when the node changes its parent or after too much
    silence from the application layer (no piggybacking available).

    This method is also used for forwarding toward the sink a topology report
    received from a node.
*/
void send_topology_report(my_collect_conn* conn, uint8_t forward) {
    // Just forward upwward a topology report coming from child node
    if (forward == 1 && conn->treport_hold == 1 && !check_address_in_topologyreport_block(conn, linkaddr_node_addr)) {
        // append to packet header the topology report
        uint8_t len;
        enum packet_type pt = topology_report;
        tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};

        memcpy(&len, packetbuf_dataptr() + sizeof(enum packet_type), sizeof(uint8_t));
        len = len + 1;

        packetbuf_hdralloc(sizeof(tree_connection));
        // Make the header and data buffer contiguous
        packetbuf_compact();
        memcpy(packetbuf_hdrptr(), &pt, sizeof(enum packet_type));
        memcpy(packetbuf_hdrptr() + sizeof(enum packet_type), &len, sizeof(uint8_t));
        memcpy(packetbuf_hdrptr() + sizeof(enum packet_type) + sizeof(uint8_t), &tc, sizeof(tree_connection));

        conn->treport_hold=0;
        // reset timer (no need to send this topology report with dedicated packet anymore)
        ctimer_stop(&conn->treport_hold_timer);
        unicast_send(&conn->uc, &conn->parent);
        return;
    }
    // else
    // Init this node's topology report and send to parent
    printf("Node %02x:%02x sending a topology report\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    enum packet_type pt = topology_report;
    tree_connection tc = {.node=linkaddr_node_addr, .parent=conn->parent};
    uint8_t len = 1;

    packetbuf_clear();
    packetbuf_set_datalen(sizeof(tree_connection));
    memcpy(packetbuf_dataptr(), &tc, sizeof(tree_connection));

    packetbuf_hdralloc(sizeof(enum packet_type) + sizeof(uint8_t));
    memcpy(packetbuf_hdrptr(), &pt, sizeof(enum packet_type));
    memcpy(packetbuf_hdrptr() + sizeof(enum packet_type), &len, sizeof(uint8_t));
    unicast_send(&conn->uc, &conn->parent);
}
