#ifndef IP_H
#define IP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <arpa/inet.h>

int start_msg_client(void);
int start_check_ip_thread(void);

#endif // IP_H
