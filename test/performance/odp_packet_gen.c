/* Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/* enable usleep */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#define MAX_PKTIOS        32
#define MAX_PKTIO_NAME    255
#define RX_THREAD         1
#define TX_THREAD         2
#define MAX_VLANS         4

/* Minimum number of packets to receive in CI test */
#define MIN_RX_PACKETS_CI 800

typedef struct test_options_t {
	uint64_t gap_nsec;
	uint64_t quit;
	uint64_t update_msec;
	uint32_t num_rx;
	uint32_t num_tx;
	uint32_t num_cpu;
	uint32_t num_pktio;
	uint32_t num_pkt;
	uint32_t pkt_len;
	uint32_t hdr_len;
	uint32_t burst_size;
	uint32_t bursts;
	uint32_t num_vlan;
	uint32_t ipv4_src;
	uint32_t ipv4_dst;
	uint16_t udp_src;
	uint16_t udp_dst;
	uint32_t wait_sec;

	struct vlan_hdr {
		uint16_t tpid;
		uint16_t tci;
	} vlan[MAX_VLANS];

	struct {
		uint32_t udp_src;
		uint32_t udp_dst;
	} c_mode;

	char     pktio_name[MAX_PKTIOS][MAX_PKTIO_NAME + 1];
	char     ipv4_src_s[24];
	char     ipv4_dst_s[24];

} test_options_t;

typedef struct thread_arg_t {
	void *global;
	int tx_thr;

	/* pktout queue per pktio interface (per thread) */
	odp_pktout_queue_t pktout[MAX_PKTIOS];

} thread_arg_t;

typedef struct ODP_ALIGNED_CACHE thread_stat_t {
	uint64_t time_nsec;
	uint64_t rx_timeouts;
	uint64_t rx_packets;
	uint64_t rx_bytes;

	uint64_t tx_timeouts;
	uint64_t tx_packets;
	uint64_t tx_drops;

	int      thread_type;

	struct {
		uint64_t rx_packets;
		uint64_t tx_packets;

	} pktio[MAX_PKTIOS];

} thread_stat_t;

typedef struct test_global_t {
	test_options_t test_options;
	odp_atomic_u32_t exit_test;
	odp_barrier_t barrier;
	odp_cpumask_t cpumask;
	odp_pool_t pool;
	uint64_t drained;
	odph_thread_t thread_tbl[ODP_THREAD_COUNT_MAX];
	thread_stat_t stat[ODP_THREAD_COUNT_MAX];
	thread_arg_t thread_arg[ODP_THREAD_COUNT_MAX];

	struct {
		odph_ethaddr_t eth_src;
		odph_ethaddr_t eth_dst;
		odp_pktio_t pktio;
		odp_pktout_queue_t pktout[ODP_THREAD_COUNT_MAX];
		int started;

	} pktio[MAX_PKTIOS];

} test_global_t;

static test_global_t *test_global;

static void print_usage(void)
{
	printf("\n"
	       "ODP packet generator\n"
	       "\n"
	       "Usage: odp_packet_gen [options]\n"
	       "\n"
	       "  Mandatory:\n"
	       "  -i, --interface <name>  Packet IO interfaces. Comma-separated list of\n"
	       "                          interface names (no spaces) e.g. eth0,eth1.\n"
	       "                          At least one interface is required.\n"
	       "\n"
	       "  Optional:\n"
	       "  -e, --eth_dst <mac>       Destination MAC address. Comma-separated list of\n"
	       "                            addresses (no spaces), one address per packet IO\n"
	       "                            interface e.g. AA:BB:CC:DD:EE:FF,11:22:33:44:55:66\n"
	       "                            Default per interface: 02:00:00:A0:B0:CX, where X = 0,1,...\n"
	       "  -v, --vlan <tpid:tci>     VLAN configuration. Comma-separated list of VLAN TPID:TCI\n"
	       "                            values in hexadecimal, starting from the outer most VLAN.\n"
	       "                            For example:\n"
	       "                            VLAN 200 (decimal):          8100:c8\n"
	       "                            Double tagged VLANs 1 and 2: 88a8:1,8100:2\n"
	       "  -r, --num_rx              Number of receive threads. Default: 1\n"
	       "  -t, --num_tx              Number of transmit threads. Default: 1\n"
	       "  -n, --num_pkt             Number of packets in the pool. Default: 1000\n"
	       "  -l, --len                 Packet length. Default: 512\n"
	       "  -b, --burst_size          Transmit burst size. Default: 8\n"
	       "  -x, --bursts              Number of bursts per one transmit round. Default: 1\n"
	       "  -g, --gap                 Gap between transmit rounds in nsec. Default: 1000000\n"
	       "                            Transmit packet rate per interface:\n"
	       "                              num_tx * burst_size * bursts * (10^9 / gap)\n"
	       "  -s, --ipv4_src            IPv4 source address. Default: 192.168.0.1\n"
	       "  -d, --ipv4_dst            IPv4 destination address. Default: 192.168.0.2\n"
	       "  -o, --udp_src             UDP source port. Default: 10000\n"
	       "  -p, --udp_dst             UDP destination port. Default: 20000\n"
	       "  -c, --c_mode <counts>     Counter mode for incrementing UDP port numbers.\n"
	       "                            Specify the number of port numbers used starting from\n"
	       "                            udp_src/udp_dst. Comma-serarated (no spaces) list of\n"
	       "                            count values: <udp_src count>,<udp_dst count>\n"
	       "                            Default value: 0,0\n"
	       "  -q, --quit                Quit after this many transmit rounds.\n"
	       "                            Default: 0 (don't quit)\n"
	       "  -u, --update_stat <msec>  Update and print statistics every <msec> milliseconds.\n"
	       "                            0: Don't print statistics periodically (default)\n"
	       "  -h, --help                This help\n"
	       "  -w, --wait <sec>          Wait up to <sec> seconds for network links to be up.\n"
	       "                            Default: 0 (don't check link status)\n"
	       "\n");
}

static int parse_vlan(const char *str, test_global_t *global)
{
	struct vlan_hdr *vlan;
	const char *start = str;
	char *end;
	int num_vlan = 0;
	intptr_t str_len = strlen(str);

	while (num_vlan < MAX_VLANS) {
		vlan = &global->test_options.vlan[num_vlan];

		/* TPID in hexadecimal */
		end = NULL;
		vlan->tpid = strtoul(start, &end, 16);
		if (end < start)
			break;

		/* Skip ':' */
		start = end + 1;
		if (start - str >= str_len)
			break;

		/* TCI in hexadecimal */
		end = NULL;
		vlan->tci = strtoul(start, &end, 16);
		if (end < start)
			break;

		num_vlan++;

		/* Skip ',' or stop at the string end */
		start = end + 1;
		if (start - str >= str_len)
			break;
	}

	return num_vlan;
}

static int parse_options(int argc, char *argv[], test_global_t *global)
{
	int opt, i, len, str_len, long_index, udp_port;
	unsigned long int count;
	uint32_t min_packets, num_tx_pkt;
	char *name, *str, *end;
	test_options_t *test_options = &global->test_options;
	int ret = 0;
	int help = 0;
	uint8_t default_eth_dst[6] = {0x02, 0x00, 0x00, 0xa0, 0xb0, 0xc0};

	static const struct option longopts[] = {
		{"interface",   required_argument, NULL, 'i'},
		{"eth_dst",     required_argument, NULL, 'e'},
		{"num_rx",      required_argument, NULL, 'r'},
		{"num_tx",      required_argument, NULL, 't'},
		{"num_pkt",     required_argument, NULL, 'n'},
		{"len",         required_argument, NULL, 'l'},
		{"burst_size",  required_argument, NULL, 'b'},
		{"bursts",      required_argument, NULL, 'x'},
		{"gap",         required_argument, NULL, 'g'},
		{"vlan",        required_argument, NULL, 'v'},
		{"ipv4_src",    required_argument, NULL, 's'},
		{"ipv4_dst",    required_argument, NULL, 'd'},
		{"udp_src",     required_argument, NULL, 'o'},
		{"udp_dst",     required_argument, NULL, 'p'},
		{"c_mode",      required_argument, NULL, 'c'},
		{"quit",        required_argument, NULL, 'q'},
		{"wait",        required_argument, NULL, 'w'},
		{"update_stat", required_argument, NULL, 'u'},
		{"help",        no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	static const char *shortopts = "+i:e:r:t:n:l:b:x:g:v:s:d:o:p:c:q:u:w:h";

	test_options->num_pktio  = 0;
	test_options->num_rx     = 1;
	test_options->num_tx     = 1;
	test_options->num_pkt    = 1000;
	test_options->pkt_len    = 512;
	test_options->burst_size = 8;
	test_options->bursts     = 1;
	test_options->gap_nsec   = 1000000;
	test_options->num_vlan   = 0;
	strncpy(test_options->ipv4_src_s, "192.168.0.1",
		sizeof(test_options->ipv4_src_s) - 1);
	strncpy(test_options->ipv4_dst_s, "192.168.0.2",
		sizeof(test_options->ipv4_dst_s) - 1);
	odph_ipv4_addr_parse(&test_options->ipv4_src, test_options->ipv4_src_s);
	odph_ipv4_addr_parse(&test_options->ipv4_dst, test_options->ipv4_dst_s);
	test_options->udp_src = 10000;
	test_options->udp_dst = 20000;
	test_options->c_mode.udp_src = 0;
	test_options->c_mode.udp_dst = 0;
	test_options->quit = 0;
	test_options->update_msec = 0;
	test_options->wait_sec = 0;

	for (i = 0; i < MAX_PKTIOS; i++) {
		memcpy(global->pktio[i].eth_dst.addr, default_eth_dst, 6);
		global->pktio[i].eth_dst.addr[5] += i;
	}

	while (1) {
		opt = getopt_long(argc, argv, shortopts, longopts, &long_index);

		if (opt == -1)
			break;

		switch (opt) {
		case 'i':
			i = 0;
			str = optarg;
			str_len = strlen(str);

			while (str_len > 0) {
				len = strcspn(str, ",");
				str_len -= len + 1;

				if (i == MAX_PKTIOS) {
					printf("Error: Too many interfaces\n");
					ret = -1;
					break;
				}

				if (len > MAX_PKTIO_NAME) {
					printf("Error: Too long interface name %s\n",
					       str);
					ret = -1;
					break;
				}

				name = test_options->pktio_name[i];
				memcpy(name, str, len);
				str += len + 1;
				i++;
			}

			test_options->num_pktio = i;

			break;
		case 'e':
			i = 0;
			str = optarg;
			str_len = strlen(str);

			while (str_len > 0) {
				odph_ethaddr_t *dst = &global->pktio[i].eth_dst;

				len = strcspn(str, ",");
				str_len -= len + 1;

				if (i == MAX_PKTIOS) {
					printf("Error: Too many MAC addresses\n");
					ret = -1;
					break;
				}

				if (odph_eth_addr_parse(dst, str)) {
					printf("Error: Bad MAC address: %s\n",
					       str);
					ret = -1;
					break;
				}

				str += len + 1;
				i++;
			}
			break;
		case 'o':
			udp_port = atoi(optarg);
			if (udp_port < 0 || udp_port > UINT16_MAX) {
				printf("Error: Bad UDP source port: %d\n", udp_port);
				ret = -1;
				break;
			}
			test_options->udp_src = udp_port;
			break;
		case 'p':
			udp_port = atoi(optarg);
			if (udp_port < 0 || udp_port > UINT16_MAX) {
				printf("Error: Bad UDP destination port: %d\n", udp_port);
				ret = -1;
				break;
			}
			test_options->udp_dst = udp_port;
			break;
		case 'r':
			test_options->num_rx = atoi(optarg);
			break;
		case 't':
			test_options->num_tx = atoi(optarg);
			break;
		case 'n':
			test_options->num_pkt = atoi(optarg);
			break;
		case 'l':
			test_options->pkt_len = atoi(optarg);
			break;
		case 'b':
			test_options->burst_size = atoi(optarg);
			break;
		case 'x':
			test_options->bursts = atoi(optarg);
			break;
		case 'g':
			test_options->gap_nsec = atoll(optarg);
			break;
		case 'v':
			test_options->num_vlan = parse_vlan(optarg, global);
			if (test_options->num_vlan == 0) {
				printf("Error: Did not find any VLANs\n");
				ret = -1;
			}
			break;
		case 's':
			if (odph_ipv4_addr_parse(&test_options->ipv4_src,
						 optarg)) {
				printf("Error: Bad IPv4 source address: %s\n",
				       optarg);
				ret = -1;
			}
			strncpy(test_options->ipv4_src_s, optarg,
				sizeof(test_options->ipv4_src_s) - 1);
			break;
		case 'd':
			if (odph_ipv4_addr_parse(&test_options->ipv4_dst,
						 optarg)) {
				printf("Error: Bad IPv4 destination address: %s\n",
				       optarg);
				ret = -1;
			}
			strncpy(test_options->ipv4_dst_s, optarg,
				sizeof(test_options->ipv4_dst_s) - 1);
			break;
		case 'c':
			count = strtoul(optarg, &end, 0);
			test_options->c_mode.udp_src = count;

			end++;
			count = strtoul(end, NULL, 0);
			test_options->c_mode.udp_dst = count;
			break;
		case 'q':
			test_options->quit = atoll(optarg);
			break;
		case 'u':
			test_options->update_msec = atoll(optarg);
			break;
		case 'w':
			test_options->wait_sec = atoi(optarg);
			break;
		case 'h':
			/* fall through */
		default:
			help = 1;
			print_usage();
			ret = -1;
			break;
		}
	}

	if (help == 0 && test_options->num_pktio == 0) {
		printf("Error: At least one packet IO interface is needed.\n");
		printf("       Use -i <name> to specify interfaces.\n");
		ret = -1;
	}

	if (test_options->num_rx < 1 || test_options->num_tx < 1) {
		printf("Error: At least one rx and tx thread needed.\n");
		ret = -1;
	}

	test_options->num_cpu = test_options->num_rx + test_options->num_tx;

	/* Pool needs to have enough packets for all tx side bursts and
	 * one rx side burst */
	num_tx_pkt  = test_options->burst_size * test_options->bursts;
	min_packets = (test_options->num_pktio * test_options->num_tx *
		      num_tx_pkt) +
		      (test_options->num_pktio * test_options->num_rx *
		      test_options->burst_size);

	if (test_options->num_pkt < min_packets) {
		printf("Error: Pool needs to have at least %u packets\n",
		       min_packets);
		ret = -1;
	}

	if (test_options->gap_nsec) {
		double gap_hz = 1000000000.0 / test_options->gap_nsec;

		if (gap_hz > (double)odp_time_local_res()) {
			printf("\nWARNING: Burst gap exceeds time counter resolution "
			       "%" PRIu64 "\n\n", odp_time_local_res());
		}
	}

	if (test_options->c_mode.udp_dst &&
	    num_tx_pkt % test_options->c_mode.udp_dst)
		printf("\nWARNING: Transmit packet count is not evenly divisible by UDP destination port count.\n\n");

	if (test_options->c_mode.udp_src &&
	    num_tx_pkt % test_options->c_mode.udp_src)
		printf("\nWARNING: Transmit packet count is not evenly divisible by UDP source port count.\n\n");

	test_options->hdr_len = ODPH_ETHHDR_LEN +
				(test_options->num_vlan * ODPH_VLANHDR_LEN) +
				ODPH_IPV4HDR_LEN + ODPH_UDPHDR_LEN;

	if (test_options->hdr_len >= test_options->pkt_len) {
		printf("Error: Headers do not fit into packet length %u\n",
		       test_options->pkt_len);
		ret = -1;
	}

	return ret;
}

static int set_num_cpu(test_global_t *global)
{
	int ret;
	test_options_t *test_options = &global->test_options;
	int num_cpu = test_options->num_cpu;

	/* One thread used for the main thread */
	if (num_cpu > ODP_THREAD_COUNT_MAX - 1) {
		printf("Error: Too many threads. API supports max %i.\n",
		       ODP_THREAD_COUNT_MAX - 1);
		return -1;
	}

	ret = odp_cpumask_default_worker(&global->cpumask, num_cpu);

	if (ret != num_cpu) {
		int cpu;

		/* Normally we want to use only worker threads */
		if (ret > 1) {
			printf("Error: Too many workers. Maximum supported %i.\n",
			       ret);
			return -1;
		}

		/* When number of workers is very limited (e.g. ODP project CI),
		 * we try to use any CPUs available. */
		ret = odp_cpumask_all_available(&global->cpumask);
		if (ret < num_cpu) {
			printf("Error: Not enough CPUs. Maximum supported %i.\n",
			       ret);
			return -1;
		}

		/* Remove extra CPUs from the mask */
		cpu = odp_cpumask_first(&global->cpumask);
		while (ret > num_cpu) {
			odp_cpumask_clr(&global->cpumask, cpu);
			cpu = odp_cpumask_first(&global->cpumask);
			ret--;
		}
	}

	odp_barrier_init(&global->barrier, num_cpu + 1);

	return 0;
}

static int open_pktios(test_global_t *global)
{
	odp_pool_capability_t pool_capa;
	odp_pktio_capability_t pktio_capa;
	odp_pool_param_t  pool_param;
	odp_pool_t pool;
	odp_pktio_param_t pktio_param;
	odp_pktio_t pktio;
	odp_pktio_config_t pktio_config;
	odp_pktin_queue_param_t pktin_param;
	odp_pktout_queue_param_t pktout_param;
	char *name;
	uint32_t i, seg_len;
	int j;
	test_options_t *test_options = &global->test_options;
	uint32_t num_rx = test_options->num_rx;
	int num_tx = test_options->num_tx;
	uint32_t num_pktio = test_options->num_pktio;
	uint32_t num_pkt = test_options->num_pkt;
	uint32_t pkt_len = test_options->pkt_len;
	odp_pktout_queue_t pktout[num_tx];

	printf("\nODP packet generator\n");
	printf("  quit test after     %" PRIu64 " rounds\n",
	       test_options->quit);
	printf("  num rx threads      %u\n", num_rx);
	printf("  num tx threads      %i\n", num_tx);
	printf("  num packets         %u\n", num_pkt);
	printf("  packet length       %u bytes\n", pkt_len);
	printf("  tx burst size       %u\n", test_options->burst_size);
	printf("  tx bursts           %u\n", test_options->bursts);
	printf("  tx burst gap        %" PRIu64 " nsec\n",
	       test_options->gap_nsec);
	printf("  clock resolution    %" PRIu64 " Hz\n", odp_time_local_res());
	for (i = 0; i < test_options->num_vlan; i++) {
		printf("  VLAN[%i]             %x:%x\n", i,
		       test_options->vlan[i].tpid, test_options->vlan[i].tci);
	}
	printf("  IPv4 source         %s\n", test_options->ipv4_src_s);
	printf("  IPv4 destination    %s\n", test_options->ipv4_dst_s);
	printf("  UDP source          %u\n", test_options->udp_src);
	printf("  UDP destination     %u\n", test_options->udp_dst);
	printf("  UDP src count       %u\n", test_options->c_mode.udp_src);
	printf("  UDP dst count       %u\n", test_options->c_mode.udp_dst);
	printf("  num pktio           %u\n", num_pktio);

	printf("  interfaces names:   ");
	for (i = 0; i < num_pktio; i++) {
		if (i > 0)
			printf("                      ");
		printf("%s\n", test_options->pktio_name[i]);
	}

	printf("  destination MACs:   ");
	for (i = 0; i < num_pktio; i++) {
		uint8_t *eth_dst = global->pktio[i].eth_dst.addr;

		if (i > 0)
			printf("                      ");
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
		       eth_dst[0], eth_dst[1], eth_dst[2],
		       eth_dst[3], eth_dst[4], eth_dst[5]);
	}
	printf("\n");

	global->pool = ODP_POOL_INVALID;

	if (odp_pool_capability(&pool_capa)) {
		printf("Error: Pool capability failed.\n");
		return -1;
	}

	if (pool_capa.pkt.max_num &&
	    num_pkt > pool_capa.pkt.max_num) {
		printf("Error: Too many packets. Max %u supported.\n",
		       pool_capa.pkt.max_num);
		return -1;
	}

	if (pool_capa.pkt.max_len &&
	    pkt_len > pool_capa.pkt.max_len) {
		printf("Error: Too large packets. Max %u supported length.\n",
		       pool_capa.pkt.max_len);
		return -1;
	}

	seg_len = test_options->hdr_len;
	if (pool_capa.pkt.max_seg_len &&
	    seg_len > pool_capa.pkt.max_seg_len) {
		printf("Error: Max segment length is too small %u\n",
		       pool_capa.pkt.max_seg_len);
		return -1;
	}

	/* Create pool */
	odp_pool_param_init(&pool_param);
	pool_param.type        = ODP_POOL_PACKET;
	pool_param.pkt.num     = num_pkt;
	pool_param.pkt.len     = pkt_len;
	pool_param.pkt.seg_len = seg_len;

	pool = odp_pool_create("packet gen pool", &pool_param);

	if (pool == ODP_POOL_INVALID) {
		printf("Error: Pool create failed.\n");
		return -1;
	}

	global->pool = pool;

	odp_pktio_param_init(&pktio_param);
	pktio_param.in_mode  = ODP_PKTIN_MODE_SCHED;
	pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;

	for (i = 0; i < num_pktio; i++)
		global->pktio[i].pktio = ODP_PKTIO_INVALID;

	/* Open and configure interfaces */
	for (i = 0; i < num_pktio; i++) {
		name  = test_options->pktio_name[i];
		pktio = odp_pktio_open(name, pool, &pktio_param);

		if (pktio == ODP_PKTIO_INVALID) {
			printf("Error (%s): Pktio open failed.\n", name);
			return -1;
		}

		global->pktio[i].pktio = pktio;

		odp_pktio_print(pktio);

		if (odp_pktio_capability(pktio, &pktio_capa)) {
			printf("Error (%s): Pktio capability failed.\n", name);
			return -1;
		}

		if (num_rx > pktio_capa.max_input_queues) {
			printf("Error (%s): Too many RX threads. Interface supports max %u input queues.\n",
			       name, pktio_capa.max_input_queues);
			return -1;
		}

		if (num_tx > (int)pktio_capa.max_output_queues) {
			printf("Error (%s): Too many TX threads. Interface supports max %u output queues.\n",
			       name, pktio_capa.max_output_queues);
			return -1;
		}

		if (odp_pktio_mac_addr(pktio,
				       &global->pktio[i].eth_src.addr,
				       ODPH_ETHADDR_LEN) != ODPH_ETHADDR_LEN) {
			printf("Error (%s): MAC address read failed.\n", name);
			return -1;
		}

		odp_pktio_config_init(&pktio_config);
		pktio_config.parser.layer = ODP_PROTO_LAYER_ALL;

		odp_pktio_config(pktio, &pktio_config);

		odp_pktin_queue_param_init(&pktin_param);

		pktin_param.queue_param.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
		pktin_param.queue_param.sched.sync  = ODP_SCHED_SYNC_PARALLEL;
		pktin_param.queue_param.sched.group = ODP_SCHED_GROUP_ALL;
		pktin_param.hash_enable = 1;
		pktin_param.hash_proto.proto.ipv4_udp = 1;
		pktin_param.num_queues = num_rx;

		if (odp_pktin_queue_config(pktio, &pktin_param)) {
			printf("Error (%s): Pktin config failed.\n", name);
			return -1;
		}

		odp_pktout_queue_param_init(&pktout_param);
		pktout_param.op_mode = ODP_PKTIO_OP_MT_UNSAFE;
		pktout_param.num_queues = num_tx;

		if (odp_pktout_queue_config(pktio, &pktout_param)) {
			printf("Error (%s): Pktout config failed.\n", name);
			return -1;
		}

		if (odp_pktout_queue(pktio, pktout, num_tx) != num_tx) {
			printf("Error (%s): Pktout queue request failed.\n",
			       name);
			return -1;
		}

		for (j = 0; j < num_tx; j++)
			global->pktio[i].pktout[j] = pktout[j];
	}

	return 0;
}

static int print_link_info(odp_pktio_t pktio)
{
	odp_pktio_link_info_t info;

	if (odp_pktio_link_info(pktio, &info)) {
		printf("Error: Pktio link info failed.\n");
		return -1;
	}

	printf("  autoneg     %s\n",
	       (info.autoneg == ODP_PKTIO_LINK_AUTONEG_ON ? "on" :
	       (info.autoneg == ODP_PKTIO_LINK_AUTONEG_OFF ? "off" : "unknown")));
	printf("  duplex      %s\n",
	       (info.duplex == ODP_PKTIO_LINK_DUPLEX_HALF ? "half" :
	       (info.duplex == ODP_PKTIO_LINK_DUPLEX_FULL ? "full" : "unknown")));
	printf("  media       %s\n", info.media);
	printf("  pause_rx    %s\n",
	       (info.pause_rx == ODP_PKTIO_LINK_PAUSE_ON ? "on" :
	       (info.pause_rx == ODP_PKTIO_LINK_PAUSE_OFF ? "off" : "unknown")));
	printf("  pause_tx    %s\n",
	       (info.pause_tx == ODP_PKTIO_LINK_PAUSE_ON ? "on" :
	       (info.pause_tx == ODP_PKTIO_LINK_PAUSE_OFF ? "off" : "unknown")));
	printf("  speed(Mbit/s) %" PRIu32 "\n\n", info.speed);

	return 0;
}
static int start_pktios(test_global_t *global)
{
	uint32_t i;
	test_options_t *test_options = &global->test_options;
	uint32_t num_pktio = test_options->num_pktio;
	uint32_t link_wait = 0;

	for (i = 0; i < num_pktio; i++) {
		if (odp_pktio_start(global->pktio[i].pktio)) {
			printf("Error (%s): Pktio start failed.\n",
			       test_options->pktio_name[i]);

			return -1;
		}

		global->pktio[i].started = 1;
	}

	if (!test_options->wait_sec)
		return 0;

	/* Wait until all links are up */
	for (i = 0; i < num_pktio; i++) {
		while (1) {
			odp_pktio_t pktio = global->pktio[i].pktio;

			if (odp_pktio_link_status(pktio) == ODP_PKTIO_LINK_STATUS_UP) {
				printf("pktio:%s\n", test_options->pktio_name[i]);
				if (print_link_info(pktio)) {
					printf("Error (%s): Printing link info failed.\n",
					       test_options->pktio_name[i]);
					return -1;
				}
				break;
			}
			link_wait++;
			if (link_wait > test_options->wait_sec) {
				printf("Error (%s): Pktio link down.\n",
				       test_options->pktio_name[i]);
				return -1;
			}
			odp_time_wait_ns(ODP_TIME_SEC_IN_NS);
		}
	}
	return 0;
}

static int stop_pktios(test_global_t *global)
{
	uint32_t i;
	odp_pktio_t pktio;
	int ret = 0;
	test_options_t *test_options = &global->test_options;
	uint32_t num_pktio = test_options->num_pktio;

	for (i = 0; i < num_pktio; i++) {
		pktio = global->pktio[i].pktio;

		if (pktio == ODP_PKTIO_INVALID || global->pktio[i].started == 0)
			continue;

		if (odp_pktio_stop(pktio)) {
			printf("Error (%s): Pktio stop failed.\n",
			       test_options->pktio_name[i]);
			ret = -1;
		}
	}

	return ret;
}

static int close_pktios(test_global_t *global)
{
	uint32_t i;
	odp_pktio_t pktio;
	test_options_t *test_options = &global->test_options;
	uint32_t num_pktio = test_options->num_pktio;
	int ret = 0;

	for (i = 0; i < num_pktio; i++) {
		pktio = global->pktio[i].pktio;

		if (pktio == ODP_PKTIO_INVALID)
			continue;

		if (odp_pktio_close(pktio)) {
			printf("Error (%s): Pktio close failed.\n",
			       test_options->pktio_name[i]);
			ret = -1;
		}
	}

	if (global->pool != ODP_POOL_INVALID &&
	    odp_pool_destroy(global->pool)) {
		printf("Error: Pool destroy failed.\n");
		ret = -1;
	}

	return ret;
}

static int rx_thread(void *arg)
{
	int i, thr, num;
	uint32_t exit_test;
	uint64_t bytes;
	odp_time_t t1, t2, exit_time;
	odp_packet_t pkt;
	thread_arg_t *thread_arg = arg;
	test_global_t *global = thread_arg->global;
	int periodic_stat = global->test_options.update_msec ? 1 : 0;
	uint64_t rx_timeouts = 0;
	uint64_t rx_packets = 0;
	uint64_t rx_bytes = 0;
	uint64_t nsec = 0;
	int ret = 0;
	int clock_started = 0;
	int exit_timer_started = 0;
	int paused = 0;
	int max_num = 32;
	odp_event_t ev[max_num];

	thr = odp_thread_id();
	global->stat[thr].thread_type = RX_THREAD;

	/* Start all workers at the same time */
	odp_barrier_wait(&global->barrier);

	while (1) {
		num = odp_schedule_multi_no_wait(NULL, ev, max_num);

		exit_test = odp_atomic_load_u32(&global->exit_test);
		if (exit_test) {
			/* Wait 1 second for possible in flight packets sent by the tx threads */
			if (exit_timer_started == 0) {
				exit_time = odp_time_local();
				t2 = exit_time;
				exit_timer_started = 1;
			} else if (odp_time_diff_ns(odp_time_local(), exit_time) >
				   ODP_TIME_SEC_IN_NS) {
				if (paused == 0) {
					odp_schedule_pause();
					paused = 1;
				} else if (num == 0) {
					/* Exit schedule loop after schedule paused and no more
					 * packets received */
					break;
				}
			}
			/* Use last received packet as stop time and don't increase rx_timeouts
			 * counter since tx threads have already been stopped */
			if (num)
				t2 = odp_time_local();
			else
				continue;
		}

		if (num == 0) {
			rx_timeouts++;
			continue;
		}

		if (!clock_started) {
			t1 = odp_time_local();
			clock_started = 1;
		}

		bytes = 0;
		for (i = 0; i < num; i++) {
			pkt = odp_packet_from_event(ev[i]);
			bytes += odp_packet_len(pkt);
		}

		rx_packets += num;
		rx_bytes   += bytes;

		if (odp_unlikely(periodic_stat)) {
			/* All packets from the same queue are from the same pktio interface */
			int index = odp_packet_input_index(odp_packet_from_event(ev[0]));

			if (index >= 0)
				global->stat[thr].pktio[index].rx_packets += num;
		}

		odp_event_free_multi(ev, num);
	}

	if (clock_started)
		nsec = odp_time_diff_ns(t2, t1);

	/* Update stats*/
	global->stat[thr].time_nsec   = nsec;
	global->stat[thr].rx_timeouts = rx_timeouts;
	global->stat[thr].rx_packets  = rx_packets;
	global->stat[thr].rx_bytes    = rx_bytes;

	return ret;
}

static void drain_queues(test_global_t *global)
{
	odp_event_t ev;
	uint64_t wait_time = odp_schedule_wait_time(100 * ODP_TIME_MSEC_IN_NS);

	while ((ev = odp_schedule(NULL, wait_time)) != ODP_EVENT_INVALID) {
		global->drained++;
		odp_event_free(ev);
	}
}

static int init_packets(test_global_t *global, int pktio,
			odp_packet_t packet[], uint32_t num, uint16_t seq)
{
	odp_packet_t pkt;
	uint32_t i, j, pkt_len, seg_len, payload_len, l2_len;
	void *data;
	uint8_t *u8;
	odph_ethhdr_t *eth;
	odph_vlanhdr_t *vlan;
	odph_ipv4hdr_t *ip;
	odph_udphdr_t *udp;
	uint16_t tpid;
	test_options_t *test_options = &global->test_options;
	uint32_t num_vlan = test_options->num_vlan;
	uint32_t hdr_len = test_options->hdr_len;
	uint16_t udp_src = test_options->udp_src;
	uint16_t udp_dst = test_options->udp_dst;
	uint32_t udp_src_cnt = 0;
	uint32_t udp_dst_cnt = 0;

	if (num_vlan > MAX_VLANS)
		num_vlan = MAX_VLANS;

	for (i = 0; i < num; i++) {
		pkt = packet[i];
		pkt_len = odp_packet_len(pkt);
		seg_len = odp_packet_seg_len(pkt);
		data = odp_packet_data(pkt);
		payload_len = pkt_len - hdr_len;

		if (seg_len < hdr_len) {
			printf("Error: First segment too short %u\n", seg_len);
			return -1;
		}

		/* Ethernet */
		eth = data;
		memcpy(eth->dst.addr, global->pktio[pktio].eth_dst.addr, 6);
		memcpy(eth->src.addr, global->pktio[pktio].eth_src.addr, 6);
		eth->type = odp_cpu_to_be_16(ODPH_ETHTYPE_IPV4);
		l2_len = ODPH_ETHHDR_LEN;

		/* VLAN(s) */
		if (num_vlan) {
			tpid = test_options->vlan[0].tpid;
			eth->type = odp_cpu_to_be_16(tpid);
		}

		for (j = 0; j < num_vlan; j++) {
			vlan = (odph_vlanhdr_t *)((uint8_t *)data + l2_len);
			vlan->tci = odp_cpu_to_be_16(test_options->vlan[j].tci);
			if (j < num_vlan - 1) {
				tpid = test_options->vlan[j + 1].tpid;
				vlan->type = odp_cpu_to_be_16(tpid);
			}

			l2_len += ODPH_VLANHDR_LEN;
		}

		if (num_vlan)
			vlan->type = odp_cpu_to_be_16(ODPH_ETHTYPE_IPV4);

		/* IPv4 */
		ip = (odph_ipv4hdr_t *)((uint8_t *)data + l2_len);
		memset(ip, 0, ODPH_IPV4HDR_LEN);
		ip->ver_ihl = ODPH_IPV4 << 4 | ODPH_IPV4HDR_IHL_MIN;
		ip->tot_len = odp_cpu_to_be_16(pkt_len - l2_len);
		ip->id = odp_cpu_to_be_16(seq + i);
		ip->ttl = 64;
		ip->proto = ODPH_IPPROTO_UDP;
		ip->src_addr = odp_cpu_to_be_32(test_options->ipv4_src);
		ip->dst_addr = odp_cpu_to_be_32(test_options->ipv4_dst);
		ip->chksum = ~odp_chksum_ones_comp16(ip, ODPH_IPV4HDR_LEN);

		/* UDP */
		udp = (odph_udphdr_t *)((uint8_t *)data + l2_len +
		      ODPH_IPV4HDR_LEN);
		memset(udp, 0, ODPH_UDPHDR_LEN);
		udp->src_port = odp_cpu_to_be_16(udp_src);
		udp->dst_port = odp_cpu_to_be_16(udp_dst);
		udp->length   = odp_cpu_to_be_16(payload_len + ODPH_UDPHDR_LEN);
		udp->chksum   = 0;

		/* Init UDP payload until the end of the first segment */
		u8  = data;
		u8 += hdr_len;
		for (j = 0; j < seg_len - hdr_len; j++)
			u8[j] = j;

		/* Insert UDP checksum */
		odp_packet_l3_offset_set(pkt, l2_len);
		odp_packet_l4_offset_set(pkt, l2_len + ODPH_IPV4HDR_LEN);
		odp_packet_has_eth_set(pkt, 1);
		odp_packet_has_ipv4_set(pkt, 1);
		odp_packet_has_udp_set(pkt, 1);
		udp->chksum = odph_ipv4_udp_chksum(pkt);

		/* Increment port numbers */
		if (test_options->c_mode.udp_src) {
			udp_src_cnt++;
			if (udp_src_cnt < test_options->c_mode.udp_src) {
				udp_src++;
			} else {
				udp_src = test_options->udp_src;
				udp_src_cnt = 0;
			}
		}
		if (test_options->c_mode.udp_dst) {
			udp_dst_cnt++;
			if (udp_dst_cnt < test_options->c_mode.udp_dst) {
				udp_dst++;
			} else {
				udp_dst = test_options->udp_dst;
				udp_dst_cnt = 0;
			}
		}
	}

	return 0;
}

static inline int send_burst(odp_pktout_queue_t pktout, odp_packet_t pkt[],
			     int burst_size) {
	int i, sent;
	int num = burst_size;
	odp_packet_t pkt_ref[burst_size];

	for (i = 0; i < burst_size; i++) {
		pkt_ref[i] = odp_packet_ref_static(pkt[i]);

		if (pkt_ref[i] == ODP_PACKET_INVALID) {
			num = i;
			break;
		}
	}

	if (odp_unlikely(num == 0))
		return 0;

	sent = odp_pktout_send(pktout, pkt_ref, num);

	if (odp_unlikely(sent < 0))
		sent = 0;

	if (odp_unlikely(sent != num)) {
		uint32_t num_drop = num - sent;

		odp_packet_free_multi(&pkt_ref[sent], num_drop);
	}

	return sent;
}

static int tx_thread(void *arg)
{
	int i, thr, tx_thr, num_alloc;
	uint32_t exit_test;
	odp_time_t t1, t2, next_tmo;
	uint64_t diff_ns, t1_nsec;
	thread_arg_t *thread_arg = arg;
	test_global_t *global = thread_arg->global;
	test_options_t *test_options = &global->test_options;
	int periodic_stat = test_options->update_msec ? 1 : 0;
	odp_pool_t pool = global->pool;
	uint64_t gap_nsec = test_options->gap_nsec;
	uint64_t quit = test_options->quit;
	uint64_t tx_timeouts = 0;
	uint64_t tx_packets = 0;
	uint64_t tx_drops = 0;
	int ret = 0;
	int burst_size = test_options->burst_size;
	int bursts = test_options->bursts;
	uint32_t num_tx = test_options->num_tx;
	uint32_t pkt_len = test_options->pkt_len;
	int num_pktio = test_options->num_pktio;
	int num_pkt = num_pktio * bursts * burst_size;
	odp_pktout_queue_t pktout[num_pktio];
	odp_packet_t pkt[num_pkt];

	thr = odp_thread_id();
	tx_thr = thread_arg->tx_thr;
	global->stat[thr].thread_type = TX_THREAD;

	num_alloc = odp_packet_alloc_multi(pool, pkt_len, pkt, num_pkt);

	if (num_alloc != num_pkt) {
		printf("Error: alloc of %u packets failed (%i)\n",
		       num_pkt, thr);
		ret = -1;
	}

	/* Initialize packets per pktio interface */
	for (i = 0; ret == 0 && i < num_pktio; i++) {
		int f = i * bursts * burst_size;

		pktout[i] = thread_arg->pktout[i];
		if (init_packets(global, i, &pkt[f], bursts * burst_size, f)) {
			ret = -1;
			break;
		}
	}

	/* Start all workers at the same time */
	odp_barrier_wait(&global->barrier);

	t1 = odp_time_local();

	/* Start TX burst at different per thread offset */
	t1_nsec = odp_time_to_ns(t1) + gap_nsec + (tx_thr * gap_nsec / num_tx);

	while (ret == 0) {
		exit_test = odp_atomic_load_u32(&global->exit_test);
		if (exit_test)
			break;

		if (quit && tx_timeouts >= quit) {
			odp_atomic_inc_u32(&global->exit_test);
			break;
		}

		if (gap_nsec) {
			uint64_t nsec = t1_nsec + tx_timeouts * gap_nsec;

			next_tmo = odp_time_local_from_ns(nsec);
			odp_time_wait_until(next_tmo);
		}
		tx_timeouts++;

		/* Send bursts to each pktio */
		for (i = 0; i < num_pktio; i++) {
			int sent, j;
			int first = i * bursts * burst_size;

			for (j = 0; j < bursts; j++) {
				sent = send_burst(pktout[i],
						  &pkt[first + j * burst_size],
						  burst_size);

				if (odp_unlikely(sent < 0)) {
					ret = -1;
					tx_drops += burst_size;
					break;
				}

				tx_packets += sent;
				if (odp_unlikely(sent < burst_size))
					tx_drops += burst_size - sent;

				if (odp_unlikely(periodic_stat))
					global->stat[thr].pktio[i].tx_packets += sent;

			}
		}
	}

	t2 = odp_time_local();
	diff_ns = odp_time_diff_ns(t2, t1);

	if (num_alloc > 0)
		odp_packet_free_multi(pkt, num_alloc);

	/* Update stats */
	global->stat[thr].time_nsec   = diff_ns;
	global->stat[thr].tx_timeouts = tx_timeouts;
	global->stat[thr].tx_packets  = tx_packets;
	global->stat[thr].tx_drops    = tx_drops;

	return ret;
}

static int start_workers(test_global_t *global, odp_instance_t instance)
{
	odph_thread_common_param_t thr_common;
	int i, j, ret, tx_thr;
	test_options_t *test_options = &global->test_options;
	int num_pktio = test_options->num_pktio;
	int num_rx  = test_options->num_rx;
	int num_cpu = test_options->num_cpu;
	odph_thread_param_t thr_param[num_cpu];

	memset(global->thread_tbl, 0, sizeof(global->thread_tbl));
	memset(thr_param, 0, sizeof(thr_param));
	memset(&thr_common, 0, sizeof(thr_common));

	thr_common.instance = instance;
	thr_common.cpumask  = &global->cpumask;

	/* Receive threads */
	for (i = 0; i < num_rx; i++) {
		thr_param[i].start    = rx_thread;
		thr_param[i].arg      = &global->thread_arg[i];
		thr_param[i].thr_type = ODP_THREAD_WORKER;
	}

	/* Transmit threads */
	tx_thr = 0;
	for (i = num_rx; i < num_cpu; i++) {
		for (j = 0; j < num_pktio; j++) {
			odp_pktout_queue_t pktout;

			global->thread_arg[i].tx_thr = tx_thr;

			/* Dedicate a pktout queue per pktio interface
			 * (per TX thread) */
			pktout = global->pktio[j].pktout[tx_thr];
			global->thread_arg[i].pktout[j] = pktout;
		}

		thr_param[i].start    = tx_thread;
		thr_param[i].arg      = &global->thread_arg[i];
		thr_param[i].thr_type = ODP_THREAD_WORKER;
		tx_thr++;
	}

	ret = odph_thread_create(global->thread_tbl, &thr_common, thr_param,
				 num_cpu);

	if (ret != num_cpu) {
		printf("Error: thread create failed %i\n", ret);
		return -1;
	}

	return 0;
}

static void print_periodic_stat(test_global_t *global, uint64_t nsec)
{
	int i, j;
	int num_pktio = global->test_options.num_pktio;
	double sec  = nsec / 1000000000.0;
	uint64_t num_tx[num_pktio];
	uint64_t num_rx[num_pktio];

	for (i = 0; i < num_pktio; i++) {
		num_tx[i] = 0;
		num_rx[i] = 0;

		for (j = 0; j < ODP_THREAD_COUNT_MAX; j++) {
			if (global->stat[j].thread_type == RX_THREAD)
				num_rx[i] += global->stat[j].pktio[i].rx_packets;
			else if (global->stat[j].thread_type == TX_THREAD)
				num_tx[i] += global->stat[j].pktio[i].tx_packets;
		}
	}

	printf("  TX: %12.6fs", sec);
	for (i = 0; i < num_pktio; i++)
		printf(" %10" PRIu64 "", num_tx[i]);

	printf("\n  RX: %12.6fs", sec);
	for (i = 0; i < num_pktio; i++)
		printf(" %10" PRIu64 "", num_rx[i]);

	printf("\n");
}

static void periodic_print_loop(test_global_t *global)
{
	odp_time_t t1, t2;
	uint64_t nsec;
	int i;
	int num_pktio = global->test_options.num_pktio;

	printf("\n\nPackets per interface\n");
	printf("  Dir          Time");
	for (i = 0; i < num_pktio; i++)
		printf(" %10i", i);

	printf("\n  -----------------");
	for (i = 0; i < num_pktio; i++)
		printf("-----------");

	printf("\n");

	t1 = odp_time_local();
	while (odp_atomic_load_u32(&global->exit_test) == 0) {
		usleep(1000 * global->test_options.update_msec);
		t2 = odp_time_local();
		nsec = odp_time_diff_ns(t2, t1);
		print_periodic_stat(global, nsec);
	}
}

static int print_final_stat(test_global_t *global)
{
	int i, num_thr;
	double rx_pkt_ave, rx_mbit_per_sec, tx_mbit_per_sec;
	test_options_t *test_options = &global->test_options;
	int num_rx = test_options->num_rx;
	int num_tx = test_options->num_tx;
	uint32_t pkt_len = test_options->pkt_len;
	uint64_t rx_nsec_sum = 0;
	uint64_t rx_pkt_sum = 0;
	uint64_t rx_byte_sum = 0;
	uint64_t rx_tmo_sum = 0;
	uint64_t tx_nsec_sum = 0;
	uint64_t tx_pkt_sum = 0;
	uint64_t tx_drop_sum = 0;
	uint64_t tx_tmo_sum = 0;
	double rx_pkt_per_sec = 0.0;
	double rx_byte_per_sec = 0.0;
	double rx_pkt_len = 0.0;
	double rx_sec = 0.0;
	double tx_pkt_per_sec = 0.0;
	double tx_sec = 0.0;

	printf("\nRESULTS PER THREAD\n");
	printf("  rx thread:\n");
	printf("           1          2          3          4          5          6          7          8\n");
	printf("  ---------------------------------------------------------------------------------------\n");
	printf("  ");

	num_thr = 0;
	for (i = 0; i < ODP_THREAD_COUNT_MAX; i++) {
		if (global->stat[i].thread_type != RX_THREAD)
			continue;

		if (num_thr && (num_thr % 8) == 0)
			printf("\n  ");

		printf("%10" PRIu64 " ", global->stat[i].rx_packets);
		num_thr++;
	}

	printf("\n\n");

	printf("  tx thread:\n");
	printf("           1          2          3          4          5          6          7          8\n");
	printf("  ---------------------------------------------------------------------------------------\n");
	printf("  ");

	num_thr = 0;
	for (i = 0; i < ODP_THREAD_COUNT_MAX; i++) {
		if (global->stat[i].thread_type != TX_THREAD)
			continue;

		if (num_thr && (num_thr % 8) == 0)
			printf("\n  ");

		printf("%10" PRIu64 " ", global->stat[i].tx_packets);
		num_thr++;
	}

	printf("\n\n");

	for (i = 0; i < ODP_THREAD_COUNT_MAX; i++) {
		if (global->stat[i].thread_type == RX_THREAD) {
			rx_tmo_sum  += global->stat[i].rx_timeouts;
			rx_pkt_sum  += global->stat[i].rx_packets;
			rx_byte_sum += global->stat[i].rx_bytes;
			rx_nsec_sum += global->stat[i].time_nsec;

		} else if (global->stat[i].thread_type == TX_THREAD) {
			tx_tmo_sum  += global->stat[i].tx_timeouts;
			tx_pkt_sum  += global->stat[i].tx_packets;
			tx_drop_sum += global->stat[i].tx_drops;
			tx_nsec_sum += global->stat[i].time_nsec;
		}
	}

	rx_pkt_ave = (double)rx_pkt_sum / num_rx;
	rx_sec = rx_nsec_sum / 1000000000.0;
	tx_sec = tx_nsec_sum / 1000000000.0;

	/* Packets and bytes per thread per sec */
	if (rx_nsec_sum) {
		rx_pkt_per_sec = (1000000000.0 * (double)rx_pkt_sum) /
				 (double)rx_nsec_sum;

		rx_byte_per_sec  = 1000000000.0;
		rx_byte_per_sec *= (rx_byte_sum + 24 * rx_pkt_sum);
		rx_byte_per_sec /= (double)rx_nsec_sum;
	}

	if (tx_nsec_sum) {
		tx_pkt_per_sec = (1000000000.0 * (double)tx_pkt_sum) /
				 (double)tx_nsec_sum;
	}

	/* Total Mbit/s */
	rx_mbit_per_sec = (num_rx * 8 * rx_byte_per_sec) / 1000000.0;
	tx_mbit_per_sec = (num_tx * 8 * tx_pkt_per_sec * (pkt_len + 24))
			  / 1000000.0;

	if (rx_pkt_sum)
		rx_pkt_len = (double)rx_byte_sum / rx_pkt_sum;

	printf("TOTAL (%i rx and %i tx threads)\n", num_rx, num_tx);
	printf("  rx timeouts:                %" PRIu64 "\n", rx_tmo_sum);
	printf("  rx time spent (sec):        %.3f\n", rx_sec);
	printf("  rx packets:                 %" PRIu64 "\n", rx_pkt_sum);
	printf("  rx packets drained:         %" PRIu64 "\n", global->drained);
	printf("  rx packets per thr:         %.1f\n", rx_pkt_ave);
	printf("  rx packets per thr per sec: %.1f\n", rx_pkt_per_sec);
	printf("  rx packets per sec:         %.1f\n", num_rx * rx_pkt_per_sec);
	printf("  rx ave packet len:          %.1f\n", rx_pkt_len);
	printf("  rx Mbit/s:                  %.1f\n", rx_mbit_per_sec);
	printf("\n");
	printf("  tx timeouts:                %" PRIu64 "\n", tx_tmo_sum);
	printf("  tx time spent (sec):        %.3f\n", tx_sec);
	printf("  tx packets:                 %" PRIu64 "\n", tx_pkt_sum);
	printf("  tx dropped packets:         %" PRIu64 "\n", tx_drop_sum);
	printf("  tx packets per thr per sec: %.1f\n", tx_pkt_per_sec);
	printf("  tx packets per sec:         %.1f\n", num_tx * tx_pkt_per_sec);
	printf("  tx Mbit/s:                  %.1f\n", tx_mbit_per_sec);
	printf("\n");

	if (rx_pkt_sum < MIN_RX_PACKETS_CI)
		return -1;

	return 0;
}

static void sig_handler(int signo)
{
	(void)signo;

	if (test_global == NULL)
		return;

	odp_atomic_add_u32(&test_global->exit_test, 1);
}

int main(int argc, char **argv)
{
	odp_instance_t instance;
	odp_init_t init;
	test_global_t *global;
	odp_shm_t shm;
	int i;
	int ret = 0;

	signal(SIGINT, sig_handler);

	/* List features not to be used */
	odp_init_param_init(&init);
	init.not_used.feat.cls      = 1;
	init.not_used.feat.compress = 1;
	init.not_used.feat.crypto   = 1;
	init.not_used.feat.ipsec    = 1;
	init.not_used.feat.timer    = 1;
	init.not_used.feat.tm       = 1;

	/* Init ODP before calling anything else */
	if (odp_init_global(&instance, &init, NULL)) {
		printf("Error: Global init failed.\n");
		return -1;
	}

	/* Init this thread */
	if (odp_init_local(instance, ODP_THREAD_CONTROL)) {
		printf("Error: Local init failed.\n");
		return -1;
	}

	shm = odp_shm_reserve("packet_gen_global", sizeof(test_global_t),
			      ODP_CACHE_LINE_SIZE, 0);

	if (shm == ODP_SHM_INVALID) {
		printf("Error: SHM reserve failed.\n");
		return -1;
	}

	global = odp_shm_addr(shm);
	test_global = global;

	memset(global, 0, sizeof(test_global_t));
	odp_atomic_init_u32(&global->exit_test, 0);

	for (i = 0; i < ODP_THREAD_COUNT_MAX; i++)
		global->thread_arg[i].global = global;

	if (parse_options(argc, argv, global)) {
		ret = -1;
		goto term;
	}

	odp_schedule_config(NULL);

	if (set_num_cpu(global)) {
		ret = -1;
		goto term;
	}

	if (open_pktios(global)) {
		ret = -1;
		goto term;
	}

	if (start_pktios(global)) {
		ret = -1;
		goto term;
	}

	/* Start worker threads */
	start_workers(global, instance);

	/* Wait until workers have started. */
	odp_barrier_wait(&global->barrier);

	/* Periodic statistics printing */
	if (global->test_options.update_msec)
		periodic_print_loop(global);

	/* Wait workers to exit */
	odph_thread_join(global->thread_tbl,
			 global->test_options.num_cpu);

	if (stop_pktios(global))
		ret = -1;

	drain_queues(global);

	if (close_pktios(global))
		ret = -1;

	if (print_final_stat(global))
		ret = -2;

term:
	if (odp_shm_free(shm)) {
		printf("Error: SHM free failed.\n");
		return -1;
	}

	if (odp_term_local()) {
		printf("Error: term local failed.\n");
		return -1;
	}

	if (odp_term_global(instance)) {
		printf("Error: term global failed.\n");
		return -1;
	}

	return ret;
}
