#ifndef UNICAST_RECEIVE_H
#define UNICAST_RECEIVE_H

void many_to_one(my_collect_conn*, const linkaddr_t*);
void one_to_many(my_collect_conn*, const linkaddr_t*);

#endif // UNICAST_RECEIVE_H
