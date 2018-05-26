#ifndef PIM_H
#define PIM_H

#include "defs.h"

#define INADDR_ALL_PIM_ROUTERS          (uint32_t)0xe000000D          /* 224.0.0.13 */
#define PIM_HELLO_DR_PRIO_DEFAULT       1

extern int pim_socket;
extern uint32_t allpimrouters_group; 

struct pim {
#ifdef _PIM_VT
	uint8_t         pim_vt; /* PIM version and message type */
#else /* ! _PIM_VT   */
#if BYTE_ORDER == BIG_ENDIAN
	u_int           pim_vers : 4, /* PIM protocol version         */
	                pim_type : 4; /* PIM message type             */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int           pim_type : 4, /* PIM message type             */
	                pim_vers : 4; /* PIM protocol version         */
#endif
#endif /* ! _PIM_VT  */
	uint8_t         pim_reserved; /* Reserved                     */
	uint16_t        pim_cksum; /* IP-style checksum            */
};

typedef struct build_jp_message_ {
	struct build_jp_message_ *next; /* Used to chain the free entries       */
	uint8_t *jp_message; /* The Join/Prune message                     */
	uint32_t jp_message_size; /* Size of the Join/Prune message (in bytes)  */
	uint16_t holdtime; /* Join/Prune message holdtime field          */
	uint32_t curr_group; /* Current group address                      */
	uint8_t  curr_group_msklen; /* Current group masklen                     */
	uint8_t *join_list; /* The working area for the join addresses    */
	uint32_t join_list_size; /* The size of the join_list (in bytes)       */
	uint16_t join_addr_number; /* Number of the join addresses in join_list  */
	uint8_t *prune_list; /* The working area for the prune addresses   */
	uint32_t prune_list_size; /* The size of the prune_list (in bytes)      */
	uint16_t prune_addr_number; /* Number of the prune addresses in prune_list*/
	uint8_t *rp_list_join; /* The working area for RP join addresses     */
	uint32_t rp_list_join_size; /* The size of the rp_list_join (in bytes)    */
	uint16_t rp_list_join_number; /* Number of RP addresses in rp_list_join   */
	uint8_t *rp_list_prune; /* The working area for RP prune addresses   */
	uint32_t rp_list_prune_size; /* The size of the rp_list_prune (in bytes)  */
	uint16_t rp_list_prune_number; /* Number of RP addresses in rp_list_prune */
	uint8_t *num_groups_ptr; /* Pointer to number_of_groups in jp_message  */
} build_jp_message_t;


typedef struct pim_nbr_entry {
	struct pim_nbr_entry *next;           /* link to next neighbor          */
	struct pim_nbr_entry *prev;           /* link to prev neighbor          */
	uint32_t              address; /* neighbor address               */
	int8_t                dr_prio_present; /* If set, this neighbor has prio */
	uint32_t              dr_prio; /* DR priority: 1 (default)       */
	uint32_t              genid; /* Cached generation ID           */
	vifi_t                vifi; /* which interface                */
	uint16_t              timer; /* for timing out neighbor        */
	build_jp_message_t *build_jp_message; /* A structure for fairly
	                                       * complicated Join/Prune
	                                       * message construction.
	                                       */
} pim_nbr_entry_t;

void init_pim(void);
static void pim_read(int f __attribute__((unused)), fd_set *rfd __attribute__((unused)));
static void accept_pim(ssize_t recvlen);

#endif