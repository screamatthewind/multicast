#include "utils.h"

#define MAXHOSTNAMELEN 255
#define IGMP_V3_MEMBERSHIP_REPORT       0x22    /* Ver. 3 membership report */

char s1[MAX_INET_BUF_LEN];
char s2[MAX_INET_BUF_LEN];
char s3[MAX_INET_BUF_LEN];

#define C(x)    ((x) & 0xff)

int nhandlers = 0;

ihandler ihandlers[NHANDLERS];

int register_input_handler(int fd, ihfunc_t func)
{
	if (nhandlers >= NHANDLERS)
		return -1;

	ihandlers[nhandlers].fd = fd;
	ihandlers[nhandlers++].func = func;

	return 0;
}

size_t strlcpy(char *dst, const char *src, size_t size)     
{
	size_t    srclen; /* Length of source string */


   /*
    * Figure out how much room is needed...
    */

	size--;

	srclen = strlen(src);

	/*
	 * Copy the appropriate amount...
	 */

	if (srclen > size)
		srclen = size;

	memcpy(dst, src, srclen);
	dst[srclen] = '\0';

	return (srclen);
}

uint32_t inet_parse(char *s, int n)
{
	uint32_t a = 0;
	u_int a0 = 0, a1 = 0, a2 = 0, a3 = 0;
	int i;
	char c;

	i = sscanf(s, "%u.%u.%u.%u%c", &a0, &a1, &a2, &a3, &c);
	if (i < n || i > 4 || a0 > 255 || a1 > 255 || a2 > 255 || a3 > 255)
		return 0xffffffff;

	((uint8_t *)&a)[0] = a0;
	((uint8_t *)&a)[1] = a1;
	((uint8_t *)&a)[2] = a2;
	((uint8_t *)&a)[3] = a3;

	return a;
}

int inet_valid_host(uint32_t naddr)
{
	uint32_t addr;

	addr = ntohl(naddr);

	return !(IN_MULTICAST(addr) ||
	         IN_BADCLASS(addr) ||
	         (addr & 0xff000000) == 0);
}

int inet_valid_mask(uint32_t mask)
{
	if (~(((mask & -mask) - 1) | mask) != 0) {
		/* Mask is not contiguous */
		return false;
	}

	return true;
}

int inet_valid_subnet(uint32_t nsubnet, uint32_t nmask)
{
	uint32_t subnet, mask;

	subnet = ntohl(nsubnet);
	mask   = ntohl(nmask);

	if ((subnet & mask) != subnet)
		return false;

	if (subnet == 0)
		return mask == 0;

	if (IN_CLASSA(subnet)) {
		if (mask < 0xff000000 ||
		    (subnet & 0xff000000) == 0x7f000000 ||
		    (subnet & 0xff000000) == 0x00000000)
			return false;
	}
	else if (IN_CLASSD(subnet) || IN_BADCLASS(subnet)) {
		/* Above Class C address space */
		return false;
	}
	if (subnet & ~mask) {
		/* Host bits are set in the subnet */
		return false;
	}
	if (!inet_valid_mask(mask)) {
		/* Netmask is not contiguous */
		return false;
	}

	return true;
}

static uint32_t forgemask(uint32_t a)
{
	uint32_t m;

	if (IN_CLASSA(a))
		m = IN_CLASSA_NET;
	else if (IN_CLASSB(a))
		m = IN_CLASSB_NET;
	else
		m = IN_CLASSC_NET;

	return (m);
}


static void domask(char *dst, size_t len, uint32_t addr, uint32_t mask)
{
	int b, i;

	if (!mask || (forgemask(addr) == mask)) {
		*dst = '\0';
		return;
	}

	i = 0;
	for (b = 0; b < 32; b++) {
		if (mask & (1 << b)) {
			int bb;

			i = b;
			for (bb = b + 1; bb < 32; bb++) {
				if (!(mask & (1 << bb))) {
					i = -1; /* noncontig */
					break;
				}
			}
			break;
		}
	}

	if (i == -1)
		snprintf(dst, len, "&0x%x", mask);
	else
		snprintf(dst, len, "/%d", 32 - i);
}


/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *netname(uint32_t addr, uint32_t mask)
{
	static char line[MAXHOSTNAMELEN + 4];
	uint32_t omask;
	uint32_t i;

	i = ntohl(addr);
	omask = mask = ntohl(mask);
	if ((i & 0xffffff) == 0)
		snprintf(line, sizeof(line), "%u", C(i >> 24));
	else if ((i & 0xffff) == 0)
		snprintf(line, sizeof(line), "%u.%u", C(i >> 24), C(i >> 16));
	else if ((i & 0xff) == 0)
		snprintf(line, sizeof(line), "%u.%u.%u", C(i >> 24), C(i >> 16), C(i >> 8));
	else
		snprintf(line,
			sizeof(line),
			"%u.%u.%u.%u",
			C(i >> 24),
			C(i >> 16),
			C(i >> 8),
			C(i));
	domask(line + strlen(line), sizeof(line) - strlen(line), i, omask);

	return line;
}


char *packet_kind(int proto, int type, int code)
{
	static char unknown[60];

	switch (proto) {
	case IPPROTO_IGMP:
		switch (type) {
		case IGMP_MEMBERSHIP_QUERY:     return "IGMP Membership Query    ";
		case IGMP_V1_MEMBERSHIP_REPORT: return "IGMP v1 Membership Report";
		case IGMP_V2_MEMBERSHIP_REPORT: return "IGMP v2 Membership Report";
		case IGMP_V3_MEMBERSHIP_REPORT: return "IGMP v3 Membership Report";
		case IGMP_V2_LEAVE_GROUP:       return "IGMP Leave message       ";
//		case IGMP_DVMRP:
//			switch (code) {
//			case DVMRP_PROBE:          return "DVMRP Neighbor Probe     ";
//			case DVMRP_REPORT:         return "DVMRP Route Report       ";
//			case DVMRP_ASK_NEIGHBORS:  return "DVMRP Neighbor Request   ";
//			case DVMRP_NEIGHBORS:      return "DVMRP Neighbor List      ";
//			case DVMRP_ASK_NEIGHBORS2: return "DVMRP Neighbor request 2 ";
//			case DVMRP_NEIGHBORS2:     return "DVMRP Neighbor list 2    ";
//			case DVMRP_PRUNE:          return "DVMRP Prune message      ";
//			case DVMRP_GRAFT:          return "DVMRP Graft message      ";
//			case DVMRP_GRAFT_ACK:      return "DVMRP Graft message ack  ";
//			case DVMRP_INFO_REQUEST:   return "DVMRP Info Request       ";
//			case DVMRP_INFO_REPLY:     return "DVMRP Info Reply         ";
//			default:
//				snprintf(unknown, sizeof(unknown), "UNKNOWN DVMRP message code = %3d", code);
//				return unknown;
//			}

//		case IGMP_PIM:
//			/* The old style (PIM v1) encapsulation of PIM messages
//			 * inside IGMP messages.
//			 */
//			/* PIM v1 is not implemented but we just inform that a message
//			 *  has arrived.
//			 */
//			switch (code) {
//			case PIM_V1_QUERY:         return "PIM v1 Router-Query     ";
//			case PIM_V1_REGISTER:      return "PIM v1 Register         ";
//			case PIM_V1_REGISTER_STOP: return "PIM v1 Register-Stop    ";
//			case PIM_V1_JOIN_PRUNE:    return "PIM v1 Join/Prune       ";
//			case PIM_V1_RP_REACHABILITY:
//				return "PIM v1 RP-Reachability  ";
//
//			case PIM_V1_ASSERT:        return "PIM v1 Assert           ";
//			case PIM_V1_GRAFT:         return "PIM v1 Graft            ";
//			case PIM_V1_GRAFT_ACK:     return "PIM v1 Graft_Ack        ";
//			default:
//				snprintf(unknown, sizeof(unknown), "UNKNOWN PIM v1 message type =%3d", code);
//				return unknown;
//			}

		case IGMP_MTRACE:              return "IGMP trace query         ";
		case IGMP_MTRACE_RESP:         return "IGMP trace reply         ";
		default:
			snprintf(unknown, sizeof(unknown), "UNKNOWN IGMP message: type = 0x%02x, code = 0x%02x", type, code);
			return unknown;
		}

//	case IPPROTO_PIM:    /* PIM v2 */
//		switch (type) {
//		case PIM_V2_HELLO:             return "PIM v2 Hello            ";
//		case PIM_V2_REGISTER:          return "PIM v2 Register         ";
//		case PIM_V2_REGISTER_STOP:     return "PIM v2 Register_Stop    ";
//
//		case PIM_V2_JOIN_PRUNE:        return "PIM v2 Join/Prune       ";
//		case PIM_V2_BOOTSTRAP:         return "PIM v2 Bootstrap        ";
//		case PIM_V2_ASSERT:            return "PIM v2 Assert           ";
//		case PIM_V2_GRAFT:             return "PIM-DM v2 Graft          ";
//		case PIM_V2_GRAFT_ACK:         return "PIM-DM v2 Graft_Ack      ";
//		case PIM_V2_CAND_RP_ADV:       return "PIM v2 Cand. RP Adv.    ";
//		default:
//			snprintf(unknown, sizeof(unknown), "UNKNOWN PIM v2 message type =%3d", type);
//			return unknown;
//		}

	default:
		snprintf(unknown, sizeof(unknown), "UNKNOWN proto =%3d", proto);
		return unknown;
	}
}

/*
 * Convert an IP address in uint32_t (network) format into a printable string.
 */
char *inet_fmt(uint32_t addr, char *s, size_t len)
{
	uint8_t *a;

	a = (uint8_t *)&addr;
	snprintf(s, len, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);

	return s;
}