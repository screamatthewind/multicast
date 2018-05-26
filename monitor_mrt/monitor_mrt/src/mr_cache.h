#ifndef MR_CACHE_H
#define MR_CACHE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "utils.h"

enum MRC_STATUS {
	MRC_NEW,
	MRC_UNRESOLVED,
	MRC_RESOLVED
};

typedef struct mrc_node
{
	uint32_t          src;
	uint32_t          grp;
	int	              iif;
	long              millis;
	int               status;
	struct mrc_node  *next;
} mrc_node;

void mrc_add(uint32_t src, uint32_t grp, int iif, long millis, int status);
mrc_node *mrc_search(uint32_t src, uint32_t grp);
void mrc_delete(uint32_t src, uint32_t grp);
void mrc_dump();

#endif