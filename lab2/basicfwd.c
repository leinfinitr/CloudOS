/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

/**
 * This file is modified from emaxples/skeleton,
 * implement a DPDK application to construct and send UDP packets.
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
// #define BURST_SIZE 32
#define BURST_SIZE 1 // Modify BURST_SIZE to 1
#define PORT 777

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0)
	{
		printf("Error during getting device (port %u) info: %s\n",
			   port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++)
	{
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
										rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++)
	{
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
										rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0)
		return retval;

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
		   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
		   port,
		   addr.addr_bytes[0], addr.addr_bytes[1],
		   addr.addr_bytes[2], addr.addr_bytes[3],
		   addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __rte_noreturn void
lcore_main(void)
{
	uint16_t port;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	RTE_ETH_FOREACH_DEV(port)
	if (rte_eth_dev_socket_id(port) >= 0 &&
		rte_eth_dev_socket_id(port) !=
			(int)rte_socket_id())
		printf("WARNING, port %u is on remote NUMA node to "
			   "polling thread.\n\tPerformance will "
			   "not be optimal.\n",
			   port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
		   rte_lcore_id());

	/* Run until the application is quit or killed. */
	for (;;)
	{
		/*
		 * Receive packets on a port and forward them on the paired
		 * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
		 */
		RTE_ETH_FOREACH_DEV(port)
		{

			/* Get burst of RX packets, from first port of pair. */
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
													bufs, BURST_SIZE);

			if (unlikely(nb_rx == 0))
				continue;

			/* Send burst of TX packets, to second port of pair. */
			const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
													bufs, nb_rx);

			/* Free any unsent packets. */
			if (unlikely(nb_tx < nb_rx))
			{
				uint16_t buf;
				for (buf = nb_tx; buf < nb_rx; buf++)
					rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
}

/**
 * Send UDP packet
 */
void send_udp(struct rte_mempool *mbuf_pool)
{
	uint16_t port = 0;

	// Send a packets to physical machine
	printf("Start send packet\n");
	struct rte_mbuf *bufs[BURST_SIZE];
	int retval = rte_pktmbuf_alloc_bulk(mbuf_pool, bufs, BURST_SIZE);
	if (retval != 0)
		printf("mbufs alloc failed\n");
	construct_udp_pkt(port, bufs, 0);
	/* Send burst of TX packets. */
	const uint16_t nb_tx = rte_eth_tx_burst(port, 0,
											bufs, BURST_SIZE);

	/* Free any unsent packets. */
	if (unlikely(nb_tx < BURST_SIZE))
	{
		uint16_t buf;
		for (buf = nb_tx; buf < BURST_SIZE; buf++)
			rte_pktmbuf_free(bufs[buf]);
	}
	printf("Send finished!\n");
}

/**
 * Construct UDP packet
 */
void construct_udp_pkt(int port, struct rte_mbuf **bufs, int i)
{
	char hello_msg[64];
	memset(hello_msg, 0, 64);
	strcpy(hello_msg, "This is a message from VM.");
	char *content = rte_pktmbuf_append(bufs[i], 64);
	rte_memcpy(content, hello_msg, 64);

	printf("Finish create message.\n");

	// Construct UDP Header
	/* The length contains both udp header and the length of data. */
	unsigned int udp_pkt_len = 64 + 8;
	struct rte_udp_hdr *udp_hdr;
	udp_hdr = (struct rte_udp_hdr *)rte_pktmbuf_prepend(bufs[i], sizeof(struct rte_udp_hdr));
	udp_hdr->src_port = rte_cpu_to_be_16(PORT);
	udp_hdr->dst_port = rte_cpu_to_be_16(PORT);
	udp_hdr->dgram_len = rte_cpu_to_be_16(udp_pkt_len);
	udp_hdr->dgram_cksum = 0;
	printf("Udp header part finished.\n");

	// Construct IPv4 Header
	struct rte_ipv4_hdr *ip_hdr;
	ip_hdr = (struct rte_ipv4_hdr *)rte_pktmbuf_prepend(bufs[i], sizeof(struct rte_ipv4_hdr));
	ip_hdr->dst_addr = rte_cpu_to_be_32(RTE_IPV4(192, 168, 80, 1));
	ip_hdr->src_addr = rte_cpu_to_be_32(RTE_IPV4(192, 168, 80, 10));
	ip_hdr->version_ihl = 0x45;
	ip_hdr->total_length = rte_cpu_to_be_16(udp_pkt_len + sizeof(struct rte_ipv4_hdr));
	ip_hdr->next_proto_id = IPPROTO_UDP;
	ip_hdr->time_to_live = 0xa;
	ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
	printf("IPv4 header part finished.\n");

	// Destination MAC address
	// The physical address can be obtained by viewing the network adapter corresponding to the virtual machine on the physical machine.
	struct rte_ether_addr s_addr, d_addr;
	rte_eth_macaddr_get(port, &s_addr);
	d_addr.addr_bytes[0] = 0x00;
	d_addr.addr_bytes[1] = 0x50;
	d_addr.addr_bytes[2] = 0x56;
	d_addr.addr_bytes[3] = 0xC0;
	d_addr.addr_bytes[4] = 0x00;
	d_addr.addr_bytes[5] = 0x02;

	// Construct Ethernet Header
	struct rte_ether_hdr *ether_hdr;
	ether_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(bufs[i], sizeof(struct rte_ether_hdr));
	ether_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	// Copy
	rte_ether_addr_copy(&ether_hdr->s_addr, &s_addr);
	rte_ether_addr_copy(&ether_hdr->d_addr, &d_addr);

	printf("Ethernet header part finished.\n");
}
/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports = 1;
	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	// nb_ports = rte_eth_dev_count_avail();
	// if (nb_ports < 2 || (nb_ports & 1))
	// 	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
										MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	RTE_ETH_FOREACH_DEV(portid)
	if (port_init(portid, mbuf_pool) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
				 portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the main core only. */
	// lcore_main();
	send_udp(mbuf_pool);

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
