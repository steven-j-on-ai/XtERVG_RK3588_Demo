#include "ip.h"

#define BUFFER_SIZE 8192

extern int g_is_online;

void parse_netlink_message(struct nlmsghdr *nlh)
{
	struct ifaddrmsg *ifa;
	struct rtattr *rth;
	int rtl;
	char ip[INET_ADDRSTRLEN];

	ifa = (struct ifaddrmsg *)NLMSG_DATA(nlh);

	rth = IFA_RTA(ifa);
	rtl = IFA_PAYLOAD(nlh);
	
	for (; RTA_OK(rth, rtl); rth = RTA_NEXT(rth, rtl)) {
		if (rth->rta_type == IFA_LOCAL) {
			inet_ntop(AF_INET, RTA_DATA(rth), ip, sizeof(ip));
			fprintf(stderr, "Interface index %d changed, new IP address: %s\n", ifa->ifa_index, ip);

			if(g_is_online) {
				continue;
			}
			fprintf(stderr, "start_msg_client: before ....................\n");
			start_msg_client();
			fprintf(stderr, "start_msg_client: after ................... \n");
		}
	}
}

void *check_ip(void *arg)
{
	int sock_fd;
	struct sockaddr_nl addr;
	struct nlmsghdr *nlh;
	char buffer[BUFFER_SIZE];

	// 创建netlink套接字
	sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock_fd < 0) {
		perror("socket");
		return 0;
	}

	// 初始化sockaddr_nl结构体
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	addr.nl_groups = RTMGRP_IPV4_IFADDR;

	// 绑定套接字
	if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(sock_fd);
		return 0;
	}

	// 监听IP地址变化
	while (1) {
		int len = recv(sock_fd, buffer, sizeof(buffer), 0);
		if (len < 0) {
			perror("recv");
			continue;
		}
		for (nlh = (struct nlmsghdr *)buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
			if (nlh->nlmsg_type == NLMSG_DONE) {
				break;
			}

			if (nlh->nlmsg_type == RTM_NEWADDR) {
				parse_netlink_message(nlh);
			}
		}
	}

	close(sock_fd);
	pthread_exit(NULL);
	return 0;
}

int start_check_ip_thread(void)
{
	pthread_t pid;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&pid, &attr, check_ip, NULL) != 0) {
		fprintf(stderr, "[start_check_ip_thread] pthread_create check_ip failed.\n");
		return -1;
	}

	usleep(100 * 1000);
	pthread_attr_destroy(&attr);
	return 0;
}