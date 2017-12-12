#ifndef UNICAST_RECEIVE_H
#define UNICAST_RECEIVE_H

void forward_upward_data(my_collect_conn*, const linkaddr_t*);
void forward_downward_data(my_collect_conn*, const linkaddr_t*);

#endif // UNICAST_RECEIVE_H
