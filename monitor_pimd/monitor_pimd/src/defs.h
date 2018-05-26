#ifndef DEFS_H
#define DEFS_H

#include <sys/socket.h>  
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <unistd.h>
	
#include <linux/mroute.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

extern int               do_vifs;
extern int               udp_socket;
extern int               total_interfaces;
extern vifi_t            numvifs; 
extern int               vifs_down;
extern int               retry_forever;

extern char      *netname(uint32_t addr, uint32_t mask);

#define C(x)    ((x) & 0xff)
#define EQUAL(s1, s2)(strcmp((s1), (s2)) == 0)
#define is_set(flag, flags)     (((flag) & (flags)) == (flag))

#define VAL_TO_MASK(x, i) {                     \
        x = htonl(~((1 << (32 - (i))) - 1));    \
    };

#ifndef __linux__
#define RANDOM()                arc4random()
#else
#define RANDOM()                (uint32_t)random()
#endif

#define PIM_TIMER_HELLO_INTERVAL          30
#define PIM_TIMER_HELLO_HOLDTIME         (3.5 * PIM_TIMER_HELLO_INTERVAL)
#define VIFF_POINT_TO_POINT               0x100000       /* point-to-point link       */
#define PIM_GROUP_PREFIX_DEFAULT_MASKLEN  16 /* The default group masklen if */
#define PIM_GROUP_PREFIX_MIN_MASKLEN       4 /* group prefix minimum length */

/* PIM messages type */
#ifndef PIM_HELLO
#define PIM_HELLO               0
#endif
#ifndef PIM_REGISTER
#define PIM_REGISTER            1
#endif
#ifndef PIM_REGISTER_STOP
#define PIM_REGISTER_STOP       2
#endif
#ifndef PIM_JOIN_PRUNE
#define PIM_JOIN_PRUNE          3
#endif
#ifndef PIM_BOOTSTRAP
#define PIM_BOOTSTRAP           4
#endif
#ifndef PIM_ASSERT
#define PIM_ASSERT              5
#endif
#ifndef PIM_GRAFT
#define PIM_GRAFT               6
#endif
#ifndef PIM_GRAFT_ACK
#define PIM_GRAFT_ACK           7
#endif
#ifndef PIM_CAND_RP_ADV
#define PIM_CAND_RP_ADV         8
#endif

#define PIM_V2_HELLO            PIM_HELLO
#define PIM_V2_REGISTER         PIM_REGISTER
#define PIM_V2_REGISTER_STOP    PIM_REGISTER_STOP
#define PIM_V2_JOIN_PRUNE       PIM_JOIN_PRUNE
#define PIM_V2_BOOTSTRAP        PIM_BOOTSTRAP
#define PIM_V2_ASSERT           PIM_ASSERT
#define PIM_V2_GRAFT            PIM_GRAFT
#define PIM_V2_GRAFT_ACK        PIM_GRAFT_ACK
#define PIM_V2_CAND_RP_ADV      PIM_CAND_RP_ADV

#define PIM_V1_QUERY            0
#define PIM_V1_REGISTER         1
#define PIM_V1_REGISTER_STOP    2
#define PIM_V1_JOIN_PRUNE       3
#define PIM_V1_RP_REACHABILITY  4
#define PIM_V1_ASSERT           5
#define PIM_V1_GRAFT            6
#define PIM_V1_GRAFT_ACK        7

#ifndef IGMP_MEMBERSHIP_QUERY
#define IGMP_MEMBERSHIP_QUERY           IGMP_HOST_MEMBERSHIP_QUERY
#if !(defined(NetBSD) || defined(OpenBSD) || defined(__FreeBSD__))
#define IGMP_V1_MEMBERSHIP_REPORT       IGMP_HOST_MEMBERSHIP_REPORT
#define IGMP_V2_MEMBERSHIP_REPORT       IGMP_HOST_NEW_MEMBERSHIP_REPORT
#else
#define IGMP_V1_MEMBERSHIP_REPORT       IGMP_v1_HOST_MEMBERSHIP_REPORT
#define IGMP_V2_MEMBERSHIP_REPORT       IGMP_v2_HOST_MEMBERSHIP_REPORT
#endif
#define IGMP_V2_LEAVE_GROUP             IGMP_HOST_LEAVE_MESSAGE
#endif
#if defined(__FreeBSD__)                /* From FreeBSD 8.x */
#define IGMP_V3_MEMBERSHIP_REPORT       IGMP_v3_HOST_MEMBERSHIP_REPORT
#else
#define IGMP_V3_MEMBERSHIP_REPORT       0x22    /* Ver. 3 membership report */
#endif

#if defined(NetBSD) || defined(OpenBSD) || defined(__FreeBSD__)
#define IGMP_MTRACE_RESP                IGMP_MTRACE_REPLY
#define IGMP_MTRACE                     IGMP_MTRACE_QUERY
#endif

#define DVMRP_PROBE             1       /* for finding neighbors             */
#define DVMRP_REPORT            2       /* for reporting some or all routes  */
#define DVMRP_ASK_NEIGHBORS     3       /* sent by mapper, asking for a list */
                                        /* of this router's neighbors. */
#define DVMRP_NEIGHBORS         4       /* response to such a request */
#define DVMRP_ASK_NEIGHBORS2    5       /* as above, want new format reply */
#define DVMRP_NEIGHBORS2        6
#define DVMRP_PRUNE             7       /* prune message */
#define DVMRP_GRAFT             8       /* graft message */
#define DVMRP_GRAFT_ACK         9       /* graft acknowledgement */
#define DVMRP_INFO_REQUEST      10      /* information request */
#define DVMRP_INFO_REPLY        11      /* information reply */

#define RECV_BUF_SIZE (128*1024) 
#define SO_RECV_BUF_SIZE_MAX (256*1024)
#define SO_RECV_BUF_SIZE_MIN (48*1024)
#define MINTTL                  1 

#define ENABLINGSTR(val)        (val) ? "enabling" : "disabling"

#define TRUE                    1
#define FALSE                   0

#define MAX_INET_BUF_LEN 19

extern char s1[MAX_INET_BUF_LEN];
extern char s2[MAX_INET_BUF_LEN];

#define INADDR_ANY_N (uint32_t)0x00000000 
#define DEFAULT_METRIC           1       /* default subnet/tunnel metric     */
#define DEFAULT_THRESHOLD        1       /* default subnet/tunnel threshold  */
	
#define DEFAULT_REG_RATE_LIMIT  0             /* default register_vif rate limit  */
#define DEFAULT_PHY_RATE_LIMIT  0             /* default phyint rate limit  */
// #define IFNAMSIZ                16

#define UCAST_DEFAULT_ROUTE_METRIC     1024
#define UCAST_DEFAULT_ROUTE_DISTANCE   101

#define INADDR_ALLRPTS_GROUP ((in_addr_t)0xe0000016) /* 224.0.0.22, IGMPv3 */

#ifndef strlcpy
size_t  strlcpy(char *dst, const char *src, size_t siz);
#endif

#endif