#include <sys/socket.h>  
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <linux/mroute.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>

#include "utils.h"

using namespace std;

#define SEND_BUF_SIZE (128*1024) 
#define RECV_BUF_SIZE (128*1024) 

#define SO_SEND_BUF_SIZE_MAX (256*1024)
#define SO_SEND_BUF_SIZE_MIN (48*1024)
#define SO_RECV_BUF_SIZE_MAX (256*1024)
#define SO_RECV_BUF_SIZE_MIN (48*1024)

#define IP_IGMP_HEADER_LEN      24 /* MIN + Router Alert */

#define INADDR_ALLRPTS_GROUP            ((in_addr_t)0xe0000016) /* 224.0.0.22, IGMPv3 */
#define INADDR_ANY_N                    (uint32_t)0x00000000 
#define INADDR_ALL_PIM_ROUTERS          (uint32_t)0xe000000D          /* 224.0.0.13 */

#define DEFAULT_METRIC           1       /* default subnet/tunnel metric     */
#define DEFAULT_THRESHOLD        1       /* default subnet/tunnel threshold  */
#define MAX_INET_BUF_LEN 19
	
#define DEFAULT_REG_RATE_LIMIT  0             /* default register_vif rate limit  */
#define DEFAULT_PHY_RATE_LIMIT  0             /* default phyint rate limit  */

#define UCAST_DEFAULT_ROUTE_METRIC     1024
#define UCAST_DEFAULT_ROUTE_DISTANCE   101

#define PIM_HELLO_DR_PRIO_DEFAULT       1

#define NO_VIF              ((vifi_t)MAXVIFS)  /* An invalid vif index */
#define NBRTYPE                 uint32_t
#define VIFF_DISABLED           0x000200       /* administratively disable */
#define VIFF_DOWN               0x000100       /* kernel state of interface */
#define VIFF_POINT_TO_POINT     0x100000       /* point-to-point link       */
#define VIFF_DR                 0x040000       /* designated router         */
#define VIFF_QUERIER            0x000400       /* I am the subnet's querier */
#define VIFF_NONBRS             0x080000       /* no neighbor on vif        */
#define VIFF_REXMIT_PRUNES      0x004000       /* retransmit prunes         */
#define VIFF_IGMPV1             0x002000       /* Act as an IGMPv1 Router   */
#define VIFF_IGMPV2             0x800000       /* Act as an IGMPv2 Router   */

#define MINTTL                  1 
#define NBRM_CLRALL(m)          ((m).lo = (m).hi = 0)
#define RANDOM()                (uint32_t)random()

#ifndef strlcpy
size_t  strlcpy(char *dst, const char *src, size_t siz);
#endif

char   *igmp_recv_buf; /* input packet buffer               */
char   *igmp_send_buf; /* output packet buffer              */
int     igmp_socket = 0; /* socket for all network I/O        */

uint32_t allhosts_group; /* allhosts  addr in net order       */
uint32_t allrouters_group; /* All-Routers addr in net order     */
uint32_t allreports_group; /* All IGMP routers in net order     */
uint32_t allpimrouters_group; /* ALL_PIM_ROUTERS address in net order */

uint32_t        default_route_metric   = UCAST_DEFAULT_ROUTE_METRIC;
uint32_t        default_route_distance = UCAST_DEFAULT_ROUTE_DISTANCE;

struct vif_acl {
	struct vif_acl  *acl_next;      /* next acl member         */
	uint32_t         acl_addr; /* Group address           */
	uint32_t         acl_mask; /* Group addr. mask        */
};

extern void config_vifs_from_kernel(void);
extern void config_vifs_from_file();
extern void zero_vif(struct uvif *v, int t);

extern char s1[MAX_INET_BUF_LEN];
extern char s2[MAX_INET_BUF_LEN];
extern char s3[MAX_INET_BUF_LEN];

struct listaddr {
	struct listaddr *al_next;           /* link to next addr, MUST BE FIRST */
	uint32_t         al_addr; /* local group or neighbor address  */
	struct listaddr *al_sources;        /* link to sources */
	uint32_t         al_timer; /* for timing out group or neighbor */
	time_t           al_ctime; /* entry creation time              */
	union {
		uint32_t     alu_genid; /* generation id for neighbor       */
		uint32_t     alu_reporter; /* a host which reported membership */
	} al_alu;
	uint8_t          al_pv; /* router protocol version          */
	uint8_t          al_mv; /* router mrouted version           */
	uint8_t          al_old; /* time since heard old report      */
	uint8_t          al_index; /* neighbor index                   */
	uint32_t         al_timerid; /* timer for group membership       */
	uint32_t         al_query; /* timer for repeated leave query   */
	uint16_t         al_flags; /* flags related to this neighbor   */
	u_long           al_versiontimer; /* timer for version switch    */
};

#define NBRTYPE                 uint32_t

typedef struct {
	NBRTYPE hi;
	NBRTYPE lo;
} nbrbitmap_t;

struct uvif {
	u_int            uv_flags; /* VIFF_ flags defined below            */
	uint8_t          uv_metric; /* cost of this vif                     */
	uint8_t          uv_admetric; /* advertised cost of this vif          */
	uint8_t          uv_threshold; /* min ttl required to forward on vif   */
	u_int            uv_rate_limit; /* rate limit on this vif               */
	int              uv_mtu; /* Initially interface MTU, then PMTU   */
	uint32_t         uv_lcl_addr; /* local address of this vif            */
	uint32_t         uv_rmt_addr; /* remote end-point addr (tunnels only) */
	uint32_t         uv_dst_addr; /* destination for DVMRP/PIM messages   */
	uint32_t         uv_subnet; /* subnet number         (phyints only) */
	uint32_t         uv_subnetmask; /* subnet mask           (phyints only) */
	uint32_t         uv_subnetbcast; /* subnet broadcast addr (phyints only) */
	char             uv_name[IFNAMSIZ]; /* interface name                   */
	//	struct listaddr *uv_groups;     /* list of local groups  (phyints only) */
		struct listaddr *uv_sources; /* list of local sources */
	struct listaddr *uv_dvmrp_neighbors; /* list of neighboring routers     */
	nbrbitmap_t      uv_nbrmap; /* bitmap of active neighboring routers */
	struct listaddr *uv_querier;    /* IGMP querier on vif                  */
	int              uv_igmpv1_warn; /* To rate-limit IGMPv1 warnings        */
	int              uv_prune_lifetime; /* Prune lifetime or 0 for default  */
	struct vif_acl  *uv_acl;        /* access control list of groups        */
	int              uv_leaf_timer; /* time until this vif is considrd leaf */
	struct phaddr   *uv_addrs;      /* Additional subnets on this vif       */
	struct vif_filter *uv_filter;   /* Route filters on this vif            */
	uint16_t        uv_hello_timer; /* Timer for sending PIM hello msgs     */
	uint32_t        uv_dr_prio; /* PIM Hello DR Priority                */
	uint32_t        uv_genid; /* Random PIM Hello Generation ID       */
	uint16_t        uv_gq_timer; /* Group Query timer                    */
	uint16_t        uv_jp_timer; /* The Join/Prune timer                 */
	int             uv_local_pref; /* default local preference for assert  */
	int             uv_local_metric; /* default local metric for assert      */
	struct pim_nbr_entry *uv_pim_neighbors; /* list of PIM neighbor routers */
#ifdef __linux__
	int             uv_ifindex; /* because RTNETLINK returns only index */
#endif /* __linux__ */
};

extern struct uvif     uvifs[MAXVIFS]; /* array of all virtual interfaces          */
extern vifi_t          numvifs; /* Number of vifs in use                    */
extern bool            vifs_down; /* 1=>some interfaces are down              */
int             phys_vif; /* An enabled vif                           */
vifi_t          reg_vif_num; /* really virtual interface for registers   */
int             udp_socket = 0; /* Since the honkin' kernel doesn't support
                                 * ioctls on raw IP sockets, we need a UDP
                                 * socket as well as our IGMP (raw) socket. */
extern int      total_interfaces; /* Number of all interfaces: including the
                                   * non-configured, but excluding the
                                   * loopback interface and the non-multicast
                                   * capable interfaces.
                                   */


static void uvif_to_vifctl(struct vifctl *vc, struct uvif *v)
{
	/* XXX: we don't support VIFF_TUNNEL; VIFF_SRCRT is obsolete */
	vc->vifc_flags           = 0;
	if (v->uv_flags & VIFF_REGISTER)
		vc->vifc_flags      |= VIFF_REGISTER;
	vc->vifc_threshold       = v->uv_threshold;
	vc->vifc_rate_limit      = v->uv_rate_limit;
	vc->vifc_lcl_addr.s_addr = v->uv_lcl_addr;
	vc->vifc_rmt_addr.s_addr = v->uv_rmt_addr;
}


/*
 * Add a virtual interface in the kernel.
 */
void k_add_vif(int socket, vifi_t vifi, struct uvif *v)
{
	struct vifctl vc;

#ifdef DRY_RUN
	return;
#endif
	
	vc.vifc_vifi = vifi;
	uvif_to_vifctl(&vc, v);

#ifndef DRY_RUN
	
	if (setsockopt(socket, IPPROTO_IP, MRT_ADD_VIF, (char *)&vc, sizeof(vc)) < 0)
	{
		perror("Failed adding VIF: ");
		fprintf(stderr,
			"Error %d: Failed adding VIF %d (MRT_ADD_VIF) for iface %s\n",
		    errno,
			vifi,
			v->uv_name);
	}

#endif
}


/*
 * Delete a virtual interface in the kernel.
 */
void k_del_vif(int socket, vifi_t vifi, struct uvif *v __attribute__((unused)))
{
	/*
	 * Unfortunately Linux MRT_DEL_VIF API differs a bit from the *BSD one.  It
	 * expects to receive a pointer to struct vifctl that corresponds to the VIF
	 * we're going to delete.  *BSD systems on the other hand exepect only the
	 * index of that VIF.
	 */
	struct vifctl vc;

	vc.vifc_vifi = vifi;
	uvif_to_vifctl(&vc, v); /* 'v' is used only on Linux systems. */

#ifndef DRY_RUN
	if (setsockopt(socket, IPPROTO_IP, MRT_DEL_VIF, (char *)&vc, sizeof(vc)) < 0)
	{
		if (errno == EADDRNOTAVAIL || errno == EINVAL)
			return;

		fprintf(stderr, "Error: %d Failed removing VIF %d (MRT_DEL_VIF)\n", errno, vifi);
	}
#endif
}

void k_join(int socket, uint32_t grp, struct uvif *v)
{
	struct ip_mreqn mreq;

	mreq.imr_ifindex          = v->uv_ifindex;
	mreq.imr_address.s_addr   = v->uv_lcl_addr;
	mreq.imr_multiaddr.s_addr = grp;

#ifndef DRY_RUN

	if (setsockopt(socket,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,
		(char *)&mreq,
		sizeof(mreq)) < 0) {
		fprintf(stderr,
			"Error %d: Cannot join group %s on interface %s (ifindex %d)\n",
			errno,
			inet_fmt(grp, s1, sizeof(s1)),
			inet_fmt(v->uv_lcl_addr, s2, sizeof(s2)),
			v->uv_ifindex);
	}
#endif
}


/*
 * Leave a multicast group on virtual interface 'v'.
 */
void k_leave(int socket, uint32_t grp, struct uvif *v)
{
	struct ip_mreqn mreq;

	mreq.imr_ifindex          = v->uv_ifindex;
	mreq.imr_address.s_addr   = v->uv_lcl_addr;
	mreq.imr_multiaddr.s_addr = grp;

#ifndef DRY_RUN
	
	if (setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
		fprintf(stderr,
			"Error %d: Cannot leave group %s on interface %s (ifindex %d)\n",
			errno,
			inet_fmt(grp, s1, sizeof(s1)),
			inet_fmt(v->uv_lcl_addr, s2, sizeof(s2)),
			v->uv_ifindex);
	}
#endif
}
/*
 * Initialize the vif and add to the kernel. The vif can be either
 * physical, register or tunnel (tunnels will be used in the future
 * when this code becomes PIM multicast boarder router.
 */
static void start_vif(vifi_t vifi)
{
	struct uvif *v;

	v   = &uvifs[vifi];
	/* Initialy no router on any vif */
	if (v->uv_flags & VIFF_REGISTER)
		v->uv_flags = v->uv_flags & ~VIFF_DOWN;
	else {
		v->uv_flags = (v->uv_flags | VIFF_DR | VIFF_NONBRS) & ~VIFF_DOWN;

		/* https://tools.ietf.org/html/draft-ietf-pim-hello-genid-01 */
		 v->uv_genid = RANDOM();
	}

	/* Tell kernel to add, i.e. start this vif */
	k_add_vif(igmp_socket, vifi, &uvifs[vifi]);
	fprintf(stderr, "VIF #%u: now in service, interface %s UP\n", vifi, v->uv_name);

	if (!(v->uv_flags & VIFF_REGISTER)) {
		/* Join the PIM multicast group on the interface. */
		// k_join(pim_socket, allpimrouters_group, v);

		/* Join the ALL-ROUTERS multicast group on the interface.  This
		 * allows mtrace requests to loop back if they are run on the
		 * multicast router. */
		k_join(igmp_socket, allrouters_group, v);

		/* Join INADDR_ALLRPTS_GROUP to support IGMPv3 membership reports */
		k_join(igmp_socket, allreports_group, v);

		/* Until neighbors are discovered, assume responsibility for sending
		 * periodic group membership queries to the subnet.  Send the first
		 * query. */
		v->uv_flags |= VIFF_QUERIER;
		// query_groups(vifi);

		/* Send a probe via the new vif to look for neighbors. */
		// send_pim_hello(v, pim_timer_hello_holdtime);
	}
	//	else {
	//		struct ifreq ifr;
	//
	//		memset(&ifr, 0, sizeof(struct ifreq));
	//
	//		if (mrt_table_id != 0) {
	//			fprintf(stderr, "Initializing pimreg%u\n", mrt_table_id);
	//			fprintf(stderr, ifr.ifr_name, IFNAMSIZ, "pimreg%u\n", mrt_table_id);
	//		}
	//		else {
	//			fprintf(stderr, "Initializing pimreg\n");
	//			strlcpy(ifr.ifr_name, "pimreg", IFNAMSIZ);
	//		}
	//
	//		if (ioctl(udp_socket, SIOGIFINDEX, (char *) &ifr) < 0) {
	//			fprintf(stderr, "Error:%d ioctl SIOGIFINDEX for %s\n", errno, ifr.ifr_name);
	//			/* Not reached */
	//			return;
	//		}
	//
	//		v->uv_ifindex = ifr.ifr_ifindex;
	//	}
}


/*
 * Stop a vif (either physical interface, tunnel or
 * register.) If we are running only PIM we don't have tunnels.
 */
static void stop_vif(vifi_t vifi)
{
	struct uvif *v;
	struct listaddr *a, *b;
	// pim_nbr_entry_t *n, *next;
	struct vif_acl *acl;

	/*
	 * TODO: make sure that the kernel viftable is
	 * consistent with the daemon table
	 */
	v = &uvifs[vifi];
	if (!(v->uv_flags & VIFF_REGISTER)) {
		// k_leave(pim_socket, allpimrouters_group, v);
		k_leave(igmp_socket, allrouters_group, v);
		k_leave(igmp_socket, allreports_group, v);
	}
	
	k_del_vif(igmp_socket, vifi, v);

	v->uv_flags = (v->uv_flags & ~VIFF_DR & ~VIFF_QUERIER & ~VIFF_NONBRS) | VIFF_DOWN;
	if (!(v->uv_flags & VIFF_REGISTER)) {
		//			for (n = v->uv_pim_neighbors; n; n = next) {
		//				next = n->next; /* Free the space for each neighbour */
		//				// delete_pim_nbr(n);
		//			}
		//			v->uv_pim_neighbors = NULL;
	}

	/* TODO: currently not used */
   /* The Access Control List (list with the scoped addresses) */
	while (v->uv_acl) {
		acl = v->uv_acl;
		v->uv_acl = acl->acl_next;
		free(acl);
	}

	vifs_down = true;
	fprintf(stderr, "Interface %s goes down; VIF #%u out of service\n", v->uv_name, vifi);
}

static void start_all_vifs(void)
{
	vifi_t vifi;
	struct uvif *v;
	u_int action;

	/* Start first the NON-REGISTER vifs */
	for (action = 0; ; action = VIFF_REGISTER) {
		for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
			/* If starting non-registers but the vif is a register or if starting
			 * registers, but the interface is not a register, then just continue. */
			if ((v->uv_flags & VIFF_REGISTER) ^ action)
				continue;

			/* Start vif if not DISABLED or DOWN */
			if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN)) {
				if (v->uv_flags & VIFF_DISABLED)
					fprintf(stderr, "Interface %s is DISABLED; VIF #%u out of service\n", v->uv_name, vifi);
				else
					fprintf(stderr, "Interface %s is DOWN; VIF #%u out of service\n", v->uv_name, vifi);
			}
			else {
				start_vif(vifi);
			}
		}

		if (action == VIFF_REGISTER)
			break;   /* We are done */
	}
}


/*
 * stop all vifs
 */
void stop_all_vifs(void)
{
	vifi_t vifi;
	struct uvif *v;

	for (vifi = 0; vifi < numvifs; vifi++) {
		v = &uvifs[vifi];
		if (!(v->uv_flags & VIFF_DOWN))
			stop_vif(vifi);
	}
}



void init_vifs(void)
{
	vifi_t vifi;
	struct uvif *v;
	int enabled_vifs;

	numvifs    = 0;
	reg_vif_num = NO_VIF;
	vifs_down = false;
	total_interfaces = 0;

	/* Configure the vifs based on the interface configuration of the the kernel and
	 * the contents of the configuration file.  (Open a UDP socket for ioctl use in
	 * the config procedures if the kernel can't handle IOCTL's on the IGMP socket.) */
#ifdef IOCTL_OK_ON_RAW_SOCKET
	udp_socket = igmp_socket;
#else
	if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		fprintf(stderr, "UDP socket Error: %d\n", errno);
#endif

	/* Clean up all vifs */
	for (vifi = 0, v = uvifs; vifi < MAXVIFS; ++vifi, ++v)
		zero_vif(v, false);

	config_vifs_from_kernel();

	//	if (!do_vifs) {
	//		for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v)
	//			v->uv_flags |= VIFF_DISABLED;
	//	}

		config_vifs_from_file();

	/*
	 * Quit if there are fewer than two enabled vifs.
	 */
	enabled_vifs    = 0;
	phys_vif        = -1;

	for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
		/* Initialize the outgoing timeout for each vif.  Currently use a fixed time
		 * 'PIM_JOIN_PRUNE_HOLDTIME'.  Later, may add a configurable array to feed
		 * these parameters, or compute them as function of the i/f bandwidth and the
		 * overall connectivity...etc. */
		// SET_TIMER(v->uv_jp_timer, PIM_JOIN_PRUNE_HOLDTIME);
		if(v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | VIFF_REGISTER | VIFF_TUNNEL))
			continue;

		if (phys_vif == -1)
			phys_vif = vifi;

		enabled_vifs++;
	}

	if (enabled_vifs < 1) /* XXX: TODO: */
	fprintf(stderr, "Cannot forward: %s\n", enabled_vifs == 0 ? "no enabled vifs" : "only one enabled vif");

	//	k_init_pim(igmp_socket); /* Call to kernel to initialize structures */

		/* Add a dummy virtual interface to support Registers in the kernel.
		 * In order for this to work, the kernel has to have been modified
		 * with the PIM patches to ip_mroute.{c,h} and ip.c
		 */
		 //	init_reg_vif();

		 	start_all_vifs();
}

void k_hdr_include(int socket, int val)
{
#ifdef IP_HDRINCL
	if (setsockopt(socket, IPPROTO_IP, IP_HDRINCL, (char *)&val, sizeof(val)) < 0)
		fprintf(stderr, 
			"Error %d Failed %d IP_HDRINCL on socket %d\n",
			errno,
			val,
			socket);
#endif
}

/*
 * Set the socket sending buffer. `bufsize` is the preferred size,
 * `minsize` is the smallest acceptable size.
 */
void k_set_sndbuf(int socket, int bufsize, int minsize)
{
	int delta = bufsize / 2;
	int iter = 0;

	/*
	 * Set the socket buffer.  If we can't set it as large as we
	 * want, search around to try to find the highest acceptable
	 * value.  The highest acceptable value being smaller than
	 * minsize is a fatal error.
	 */
	if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize, sizeof(bufsize)) < 0) {
		bufsize -= delta;
		while (1) {
			iter++;
			if (delta > 1)
				delta /= 2;

			if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize, sizeof(bufsize)) < 0) {
				bufsize -= delta;
			}
			else {
				if (delta < 1024)
					break;
				bufsize += delta;
			}
		}

		if (bufsize < minsize) {
			fprintf(stderr, "OS-allowed send buffer size %u < app min %u\n", bufsize, minsize);
			/*NOTREACHED*/
		}
	}
	
	fprintf(stderr, "Got %d byte send buffer size in %d iterations\n", bufsize, iter);
}


void k_set_rcvbuf(int socket, int bufsize, int minsize)
{
	int delta = bufsize / 2;
	int iter = 0;

	/*
	 * Set the socket buffer.  If we can't set it as large as we
	 * want, search around to try to find the highest acceptable
	 * value.  The highest acceptable value being smaller than
	 * minsize is a fatal error.
	 */
	if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize, sizeof(bufsize)) < 0) {
		bufsize -= delta;
		while (1) {
			iter++;
			if (delta > 1)
				delta /= 2;

			if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize, sizeof(bufsize)) < 0) {
				bufsize -= delta;
			}
			else {
				if (delta < 1024)
					break;
				bufsize += delta;
			}
		}

		if (bufsize < minsize) {
			fprintf(stderr, "OS-allowed recv buffer size %u < app min %u\n", bufsize, minsize);
		}
	}
	
	fprintf(stderr, "Got %d byte recv buffer size in %d iterations\n",
		bufsize,
		iter);
}

void k_set_loop(int socket, int flag)
{
	uint8_t loop;

	loop = flag;
	if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loop, sizeof(loop)) < 0)
		fprintf(stderr, "Error: %d Failed %d IP_MULTICAST_LOOP on socket %d\n",
			errno,
			flag,
			socket);
}

/*
 * Set the default TTL for the multicast packets outgoing from this
 * socket.
 * TODO: Does it affect the unicast packets?
 */
void k_set_ttl(int socket __attribute__((unused)), int t)
{
	uint8_t ttl = t;

	if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0)
		fprintf(stderr, "Error %d: Failed setting IP_MULTICAST_TTL %u on socket %d\n", errno, ttl, socket);
}

void init_igmp()
{	
	igmp_recv_buf = (char *) calloc(1, RECV_BUF_SIZE);
	igmp_send_buf = (char *) calloc(1, SEND_BUF_SIZE);

	if ((igmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0)
		fprintf(stderr, "Failed creating IGMP socket in init_igmp: %d\n", errno);

	k_hdr_include(igmp_socket, true); /* include IP header when sending */
	k_set_sndbuf(igmp_socket, SO_SEND_BUF_SIZE_MAX, SO_SEND_BUF_SIZE_MIN); /* lots of output buffering        */
	k_set_rcvbuf(igmp_socket, SO_RECV_BUF_SIZE_MAX, SO_RECV_BUF_SIZE_MIN); /* lots of input buffering        */
	k_set_ttl(igmp_socket, MINTTL); /* restrict multicasts to one hop */
	k_set_loop(igmp_socket, false); /* disable multicast loopback     */

	//	ip	       = (struct ip *)igmp_send_buf;
	//	memset(ip, 0, IP_IGMP_HEADER_LEN);
	//	
	//	ip->ip_v   = IPVERSION;
	//	ip->ip_hl  = IP_IGMP_HEADER_LEN >> 2;
	//	ip->ip_tos = 0xc0; /* Internet Control   */
	//	ip->ip_id  = 0; /* let kernel fill in */
	//	ip->ip_off = 0;
	//	ip->ip_ttl = MAXTTL; /* applies to unicasts only */
	//	ip->ip_p   = IPPROTO_IGMP;
	//	ip->ip_sum = 0; /* let kernel fill in */

	allhosts_group   = htonl(INADDR_ALLHOSTS_GROUP);
	allrouters_group = htonl(INADDR_ALLRTRS_GROUP);
	allreports_group = htonl(INADDR_ALLRPTS_GROUP);
	allpimrouters_group = htonl(INADDR_ALL_PIM_ROUTERS);
}

void shutdown()
{
	stop_all_vifs();

	free(igmp_recv_buf);
	free(igmp_send_buf);

	close(igmp_socket);
}

static void sig_handler(int sig)
{
	fprintf(stderr, "shutting down\n");
	shutdown();
	
	exit(0);
}

void init_signal_handlers()
{
	struct sigaction sa;

	sa.sa_handler = sig_handler;
	sa.sa_flags = 0; /* Interrupt system calls */
	
	sigemptyset(&sa.sa_mask);

	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

}

int main(int argc, char *argv[])
{
	struct timeval tv;
	fd_set rfds;
	int nfds, n;

	int ipdatalen, iphdrlen, igmpdatalen;
	uint32_t src, dst, group;
	struct ip *ip;
	struct igmp *igmp;
	ssize_t recvlen;
	socklen_t dummy = 0;

	init_signal_handlers();
	
	init_igmp();
	init_vifs();

	FD_ZERO(&rfds);
	FD_SET(igmp_socket, &rfds);

	nfds = igmp_socket + 1;
	
	while (1)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if ((n = select(nfds, &rfds, NULL, NULL, &tv)) < 0) {
			if (errno != EINTR) /* SIGALRM is expected */
			printf("select failed: %d", errno);
		}
		
		if (n > 0) {

			while ((recvlen = recvfrom(igmp_socket, igmp_recv_buf, RECV_BUF_SIZE, 0, NULL, &dummy)) < 0) {
				if (errno == EINTR)
					continue;         

				fprintf(stderr, "Failed recvfrom() in igmp_read: %d\n", errno);
			}
			
			ip  = (struct ip *)igmp_recv_buf;
			src = ip->ip_src.s_addr;
			dst = ip->ip_dst.s_addr;


			iphdrlen  = ip->ip_hl << 2;
			ipdatalen = recvlen - iphdrlen;

			igmp        = (struct igmp *)(igmp_recv_buf + iphdrlen);
			group       = igmp->igmp_group.s_addr;
			
			fprintf(stderr, "igmp type %d, group %s\n", igmp->igmp_type, inet_fmt(group, s1, sizeof(s1)));
		}
	}
	
	// shutdown();
		
	return 0;
}