#ifndef VIF_H
#define VIF_H

#include "defs.h"

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

#define NBRM_CLRALL(m)          ((m).lo = (m).hi = 0)

typedef struct {
	NBRTYPE hi;
	NBRTYPE lo;
} nbrbitmap_t;

struct vif_acl {
	struct vif_acl  *acl_next;      /* next acl member         */
	uint32_t         acl_addr; /* Group address           */
	uint32_t         acl_mask; /* Group addr. mask        */
};

struct phaddr {
	struct phaddr   *pa_next;
	uint32_t         pa_subnet; /* extra subnet                 */
	uint32_t         pa_subnetmask; /* netmask of extra subnet      */
	uint32_t         pa_subnetbcast; /* broadcast of extra subnet    */
};

struct vf_element {
	struct vf_element  *vfe_next;
	uint32_t            vfe_addr;
	uint32_t            vfe_mask;
	int                 vfe_flags;
#define VFEF_EXACT      0x0001
};

struct vif_filter {
	int                 vf_type;
#define VFT_ACCEPT      1
#define VFT_DENY        2
	int                 vf_flags;
#define VFF_BIDIR       1
	struct vf_element  *vf_filter;
};

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
	struct listaddr *uv_groups;     /* list of local groups  (phyints only) */
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


void init_vifs(void);
static void start_vif(vifi_t vifi);
static void stop_vif(vifi_t vifi);
static void start_all_vifs(void);
void stop_all_vifs(void);
static int init_reg_vif(void);
static int update_reg_vif(vifi_t register_vifi);
void zero_vif(struct uvif *v, int t);

#endif