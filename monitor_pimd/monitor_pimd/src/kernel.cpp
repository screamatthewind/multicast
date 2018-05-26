#include "igmp.h"
#include "utils.h"
#include "kernel.h"
#include "vif.h"

int mrt_table_id = 0;

void process_kernel_call(void)
{
	struct igmpmsg *igmpctl = (struct igmpmsg *)igmp_recv_buf;

	switch (igmpctl->im_msgtype) {
	case IGMPMSG_NOCACHE:
		process_cache_miss(igmpctl);
		break;

	case IGMPMSG_WRONGVIF:
		printf("IGMPMSG_WRONGVIF\n");
		break;

	case IGMPMSG_WHOLEPKT:
		printf("IGMPMSG_WHOLEPKT\n");
		break;

	default:
		printf("Unknown IGMP message type from kernel: %d\n", igmpctl->im_msgtype);
		break;
	}
}

static void process_cache_miss(struct igmpmsg *igmpctl)
{
	uint32_t source, mfc_source;
	uint32_t group;
	uint32_t rp_addr;
	vifi_t iif;

	/* When there is a cache miss, we check only the header of the packet
	 * (and only it should be sent up by the kernel. */

	group  = igmpctl->im_dst.s_addr;
	source = mfc_source = igmpctl->im_src.s_addr;
	iif    = igmpctl->im_vif;

	printf(
		"Cache miss, src %s, dst %s, iif %d\n",
		inet_fmt(source, s1, sizeof(s1)),
		inet_fmt(group, s2, sizeof(s2)),
		iif);

	/* TODO: XXX: check whether the kernel generates cache miss for the LAN scoped addresses */
	if (ntohl(group) <= INADDR_MAX_LOCAL_GROUP)
		return; /* Don't create routing entries for the LAN scoped addresses */
}

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

	vc.vifc_vifi = vifi;
	uvif_to_vifctl(&vc, v);
	if (setsockopt(socket, IPPROTO_IP, MRT_ADD_VIF, (char *)&vc, sizeof(vc)) < 0)
		printf(
			"Error %d: Failed adding VIF %d (MRT_ADD_VIF) for iface %s\n",
		    errno,
			vifi,
			v->uv_name);
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
#ifdef __linux__
	struct vifctl vc;

	vc.vifc_vifi = vifi;
	uvif_to_vifctl(&vc, v); /* 'v' is used only on Linux systems. */

	if (setsockopt(socket, IPPROTO_IP, MRT_DEL_VIF, (char *)&vc, sizeof(vc)) < 0)
#else /* *BSD et al. */
		if (setsockopt(socket, IPPROTO_IP, MRT_DEL_VIF, (char *)&vifi, sizeof(vifi)) < 0)
#endif /* !__linux__ */
		{
			if (errno == EADDRNOTAVAIL || errno == EINVAL)
				return;

			printf("Error: %d Failed removing VIF %d (MRT_DEL_VIF)\n", errno, vifi);
		}
}

void k_join(int socket, uint32_t grp, struct uvif *v)
{
#ifdef __linux__
	struct ip_mreqn mreq;
#else
	struct ip_mreq mreq;
#endif /* __linux__ */

#ifdef __linux__
	mreq.imr_ifindex          = v->uv_ifindex;
	mreq.imr_address.s_addr   = v->uv_lcl_addr;
#else
	mreq.imr_interface.s_addr = v->uv_lcl_addr;
#endif /* __linux__ */
	mreq.imr_multiaddr.s_addr = grp;

	if (setsockopt(socket,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,
		(char *)&mreq,
		sizeof(mreq)) < 0) {
#ifdef __linux__
		printf(
			"Error %d: Cannot join group %s on interface %s (ifindex %d)\n",
			errno,
			inet_fmt(grp, s1, sizeof(s1)),
			inet_fmt(v->uv_lcl_addr, s2, sizeof(s2)),
			v->uv_ifindex);
#else
		logit(LOG_WARNING,
			errno,
			"Cannot join group %s on interface %s",
			inet_fmt(grp, s1, sizeof(s1)),
			inet_fmt(v->uv_lcl_addr, s2, sizeof(s2)));
#endif /* __linux__ */
	}
}


/*
 * Leave a multicast group on virtual interface 'v'.
 */
void k_leave(int socket, uint32_t grp, struct uvif *v)
{
#ifdef __linux__
	struct ip_mreqn mreq;
#else
	struct ip_mreq mreq;
#endif /* __linux__ */

#ifdef __linux__
	mreq.imr_ifindex          = v->uv_ifindex;
	mreq.imr_address.s_addr   = v->uv_lcl_addr;
#else
	mreq.imr_interface.s_addr = v->uv_lcl_addr;
#endif /* __linux__ */
	mreq.imr_multiaddr.s_addr = grp;

	if (setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
#ifdef __linux__
		printf(
			"Error %d: Cannot leave group %s on interface %s (ifindex %d)\n",
			errno,
			inet_fmt(grp, s1, sizeof(s1)),
			inet_fmt(v->uv_lcl_addr, s2, sizeof(s2)),
			v->uv_ifindex);
#else
		logit(LOG_WARNING,
			errno,
			"Cannot leave group %s on interface %s",
			inet_fmt(grp, s1, sizeof(s1)),
			inet_fmt(v->uv_lcl_addr, s2, sizeof(s2)));
#endif /* __linux__ */
	}
}


void k_hdr_include(int socket, int val)
{
#ifdef IP_HDRINCL
	if (setsockopt(socket, IPPROTO_IP, IP_HDRINCL, (char *)&val, sizeof(val)) < 0)
		printf(
			"Error %d Failed %s IP_HDRINCL on socket %d\n",
			errno,
			ENABLINGSTR(val),
			socket);
#endif
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
			printf("OS-allowed recv buffer size %u < app min %u\n", bufsize, minsize);
		}
	}

	printf("k_set_rcvbuf: Got %d byte recv buffer size in %d iterations\n",
		bufsize,
		iter);
}

void k_set_loop(int socket, int flag)
{
	uint8_t loop;

	loop = flag;
	if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loop, sizeof(loop)) < 0)
		printf(
			"Error: %d Failed %s IP_MULTICAST_LOOP on socket %d\n",
			errno,
			ENABLINGSTR(flag),
			socket);
}

/*
 * Open/init the multicast routing in the kernel and sets the
 * MRT_PIM (aka MRT_ASSERT) flag in the kernel.
 */
void k_init_pim(int socket)
{
	int v = 1;

#ifdef MRT_TABLE /* Currently only available on Linux  */
	if (mrt_table_id != 0) {
		printf("Initializing multicast routing table id %u\n", mrt_table_id);
		if (setsockopt(socket, IPPROTO_IP, MRT_TABLE, &mrt_table_id, sizeof(mrt_table_id)) < 0) {
			printf("Cannot set multicast routing table id\n");
			printf("Make sure your kernel has CONFIG_IP_MROUTE_MULTIPLE_TABLES=y\n");
		}
	}
#endif

	if (setsockopt(socket, IPPROTO_IP, MRT_INIT, (char *)&v, sizeof(int)) < 0) {
		if (errno == EADDRINUSE)
			printf("Another multicast routing application is already running.\n");
		else
		{
			if (errno == 95)
				printf("Cannot enable multicast routing in kernel.  Error(%d): Operation not supported\n", errno);
			else
				printf("Cannot enable multicast routing in kernel: %d\n", errno);
		}
	}

	if (setsockopt(socket, IPPROTO_IP, MRT_PIM, (char *)&v, sizeof(int)) < 0)
		printf("Cannot set PIM flag in kernel\n");
}


/*
 * Stops the multicast routing in the kernel and resets the
 * MRT_PIM (aka MRT_ASSERT) flag in the kernel.
 */
void k_stop_pim(int socket)
{
	int v = 0;

	if (setsockopt(socket, IPPROTO_IP, MRT_PIM, (char *)&v, sizeof(int)) < 0)
		printf("Cannot reset PIM flag in kernel\n");

	if (setsockopt(socket, IPPROTO_IP, MRT_DONE, (char *)NULL, 0) < 0)
		printf("Cannot disable multicast routing in kernel\n");
}
