/* Copyright 2000-2004 Joakim Axelsson (gozem@linux.nu)
 *                     Patrick Schaaf (bof@bof.de)
 *                     Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify   
 * it under the terms of the GNU General Public License as published by   
 * the Free Software Foundation; either version 2 of the License, or      
 * (at your option) any later version.                                    
 *                                                                         
 * This program is distributed in the hope that it will be useful,        
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
 * GNU General Public License for more details.                           
 *                                                                         
 * You should have received a copy of the GNU General Public License      
 * along with this program; if not, write to the Free Software            
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <asm/bitops.h>

#include <linux/netfilter_ipv4/ip_set_ipmap.h>
#include "ipset.h"

#define BUFLEN 30;

#define OPT_CREATE_FROM    0x01U
#define OPT_CREATE_TO      0x02U
#define OPT_CREATE_NETWORK 0x04U
#define OPT_CREATE_NETMASK 0x08U

#define OPT_ADDDEL_IP      0x01U

/* Initialize the create. */
void create_init(void *data)
{
	struct ip_set_req_ipmap_create *mydata =
	    (struct ip_set_req_ipmap_create *) data;

	DP("create INIT");
	mydata->netmask = 0xFFFFFFFF;
}

/* Function which parses command options; returns true if it ate an option */
int create_parse(int c, char *argv[], void *data, unsigned *flags)
{
	struct ip_set_req_ipmap_create *mydata =
	    (struct ip_set_req_ipmap_create *) data;
	unsigned int bits;

	DP("create_parse");

	switch (c) {
	case '1':
		parse_ip(optarg, &mydata->from);

		*flags |= OPT_CREATE_FROM;

		DP("--from %x (%s)", mydata->from,
		   ip_tostring(mydata->from, 0));

		break;

	case '2':
		parse_ip(optarg, &mydata->to);

		*flags |= OPT_CREATE_TO;

		DP("--to %x (%s)", mydata->to, ip_tostring(mydata->to, 0));

		break;

	case '3':
		parse_ipandmask(optarg, &mydata->from, &mydata->to);

		/* Make to the last of from + mask */
		mydata->to = mydata->from | ~(mydata->to);

		*flags |= OPT_CREATE_NETWORK;

		DP("--network from %x (%s)", mydata->from,
		   ip_tostring(mydata->from, 0));
		DP("--network to   %x (%s)", mydata->to,
		   ip_tostring(mydata->to, 0));

		break;

	case '4':
		if (string_to_number(optarg, 0, 32, &bits))
			exit_error(PARAMETER_PROBLEM, 
				  "Invalid netmask `%s' specified", optarg);
		
		if (bits != 0)
			mydata->netmask = 0xFFFFFFFF << (32 - bits);

		*flags |= OPT_CREATE_NETMASK;

		DP("--netmask %x", mydata->netmask);
		
		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
void create_final(void *data, unsigned int flags)
{
	struct ip_set_req_ipmap_create *mydata =
	    (struct ip_set_req_ipmap_create *) data;

	if (flags == 0)
		exit_error(PARAMETER_PROBLEM,
			   "Need to specify --from and --to, or --network\n");

	if (flags & OPT_CREATE_NETWORK) {
		/* --network */
		if ((flags & OPT_CREATE_FROM) || (flags & OPT_CREATE_TO))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --from or --to with --network\n");
	} else {
		/* --from --to */
		if ((flags & OPT_CREATE_FROM) == 0
		    || (flags & OPT_CREATE_TO) == 0)
			exit_error(PARAMETER_PROBLEM,
				   "Need to specify both --from and --to\n");
	}

	DP("from : %x to: %x  diff: %d", mydata->from, mydata->to,
	   mydata->to - mydata->from);

	if (mydata->from > mydata->to)
		exit_error(PARAMETER_PROBLEM,
			   "From can't be lower than to.\n", MAX_RANGE);

	if (mydata->to - mydata->from > MAX_RANGE)
		exit_error(PARAMETER_PROBLEM,
			   "Range to large. Max is %d IPs in range\n",
			   MAX_RANGE);
	if (flags & OPT_CREATE_NETMASK) {
		unsigned int mask_bits, netmask_bits;
		ip_set_ip_t mask;
		
		if ((mydata->from & mydata->netmask) != mydata->from)
			exit_error(PARAMETER_PROBLEM,
				   "%s is not a network address according to netmask %d\n",
				   ip_tostring(mydata->from, 0),
				   mask_to_bits(mydata->netmask));
		
		mask = range_to_mask(mydata->from, mydata->to, &mask_bits);
		if (!mask)
			exit_error(PARAMETER_PROBLEM,
				   "%s-%s is not a full network\n",
				   ip_tostring(mydata->from, 0),
				   ip_tostring(mydata->to, 0));

		netmask_bits = mask_to_bits(mydata->netmask);
		
		if (netmask_bits <= mask_bits)
			exit_error(PARAMETER_PROBLEM,
				   "%d netmask specifies larger or equal netblock than %s-%s\n",
				   netmask_bits,
				   ip_tostring(mydata->from, 0),
				   ip_tostring(mydata->to, 0));
	}
}

/* Create commandline options */
static struct option create_opts[] = {
	{"from", 1, 0, '1'},
	{"to", 1, 0, '2'},
	{"network", 1, 0, '3'},
	{"netmask", 1, 0, '4'},
	{0}
};

/* Add, del, test parser */
ip_set_ip_t adt_parser(int cmd, const char *optarg,
		       void *data, const void *setdata)
{
	struct ip_set_req_ipmap *mydata =
	    (struct ip_set_req_ipmap *) data;
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) setdata;

	DP("ipmap: %p %p", data, setdata);

	parse_ip(optarg, &mydata->ip);

	DP("from %s", ip_tostring(mysetdata->first_ip, 0));
	DP("to   %s", ip_tostring(mysetdata->last_ip, 0));
	DP("ip   %s", ip_tostring(mydata->ip, 0));

	if (mydata->ip < mysetdata->first_ip ||
	    mydata->ip > mysetdata->last_ip)
		exit_error(PARAMETER_PROBLEM, "IP '%s' is out of range\n",
			   ip_tostring(mydata->ip, 0));

	return mydata->ip;	
}

ip_set_ip_t getipbyid(const void *setdata, ip_set_ip_t id)
{
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) setdata;

	return (mysetdata->first_ip + id * mysetdata->hosts);
}

ip_set_ip_t sizeid(const void *setdata)
{
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) setdata;

	return (mysetdata->sizeid);
}

void initheader(void **setdata, void *data, size_t len)
{
	struct ip_set_req_ipmap_create *header =
	    (struct ip_set_req_ipmap_create *) data;
	struct ip_set_ipmap *map;
		
	DP("ipmap: initheader() 1");

	if (len != sizeof(struct ip_set_req_ipmap_create))
		exit_error(OTHER_PROBLEM,
			   "Ipmap: incorrect size of header. "
			   "Got %d, wanted %d.", len,
			   sizeof(struct ip_set_req_ipmap_create));

	*setdata = ipset_malloc(sizeof(struct ip_set_ipmap));

	DP("ipmap: initheader() 2");
	
	map = (struct ip_set_ipmap *) *setdata;
	
	map->first_ip = header->from;
	map->last_ip = header->to;
	map->netmask = header->netmask;

	if (map->netmask == 0xFFFFFFFF) {
		map->hosts = 1;
		map->sizeid = map->last_ip - map->first_ip + 1;
	} else {
		unsigned int mask_bits, netmask_bits;
		ip_set_ip_t mask;
	
		mask = range_to_mask(header->from, header->to, &mask_bits);
		netmask_bits = mask_to_bits(header->netmask);

		DP("bits: %i %i", mask_bits, netmask_bits);
		map->hosts = 2 << (32 - netmask_bits - 1);
		map->sizeid = 2 << (netmask_bits - mask_bits - 1);
	}

	DP("%i %i", map->hosts, map->sizeid );

}

void initmembers(void *setdata, void *data, size_t len)
{
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) setdata;
	size_t size;

	DP("ipmap: initmembers()");

	/* Check so we get the right amount of memberdata */
	size = bitmap_bytes(0, mysetdata->sizeid - 1);

	if (len != size)
		exit_error(OTHER_PROBLEM,
			   "Ipmap: incorrect size of members. "
			   "Got %d, wanted %d.", len, size);

	mysetdata->members = data;
}

void killmembers(void **setdata)
{
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) *setdata;

	DP("ipmap: killmembers()");

	if (mysetdata->members != NULL)
		ipset_free(&mysetdata->members);
		
	ipset_free(setdata);
}

void printheader(const void *setdata, unsigned options)
{
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) setdata;

	printf(" from: %s", ip_tostring(mysetdata->first_ip, options));
	printf(" to: %s", ip_tostring(mysetdata->last_ip, options));
	if (mysetdata->netmask == 0xFFFFFFFF)
		printf("\n");
	else
		printf(" netmask: %d\n", mask_to_bits(mysetdata->netmask));
}

void printips_sorted(const void *setdata, unsigned options)
{
	struct ip_set_ipmap *mysetdata =
	    (struct ip_set_ipmap *) setdata;
	ip_set_ip_t id;

	for (id = 0; id < mysetdata->sizeid; id++)
		if (test_bit(id, mysetdata->members))
			printf("%s\n", ip_tostring(mysetdata->first_ip + id * mysetdata->hosts,
						   options));
}

void saveheader(const void *setdata)
{
	return;
}

/* Print save for an IP */
void saveips(const void *setdata)
{
	return;
}

void usage(void)
{
	printf
	    ("-N set ipmap --from IP --to IP [--netmask CIDR-netmask]\n"
	     "-N set ipmap --network IP/mask [--netmask CIDR-netmask]\n"
	     "-A set IP\n"
	     "-D set IP\n"
	     "-T set IP\n");
}

static struct settype settype_ipmap = {
	.typename = SETTYPE_NAME,
	.typecode = IPSET_TYPE_IP,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_ipmap_create),
	.create_init = &create_init,
	.create_parse = &create_parse,
	.create_final = &create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.req_size = sizeof(struct ip_set_req_ipmap),
	.adt_parser = &adt_parser,

	/* Get an IP address by id */
	.getipbyid = &getipbyid,
	.sizeid = &sizeid,

	/* Printing */
	.initheader = &initheader,
	.initmembers = &initmembers,
	.killmembers = &killmembers,
	.printheader = &printheader,
	.printips = &printips_sorted,	/* We only have sorted version */
	.printips_sorted = &printips_sorted,
	.saveheader = &saveheader,
	.saveips = &saveips,
	.usage = &usage,
	.hint = NULL,
};

void _init(void)
{
	settype_register(&settype_ipmap);

}
