#include "mr_cache.h"

mrc_node *head;

void mrc_add(uint32_t src, uint32_t grp, int iif, long millis, int status)
{
	mrc_node *temp;

	//	IF_DEBUG(DEBUG_MRT)
	//	  logit(LOG_DEBUG,
	//		0,
	//		"rdl: mrc_add(src %s grp %s iif %d",
	//		inet_fmt(src, s1, sizeof(s1)),
	//		inet_fmt(grp, s2, sizeof(s2)),
	//		iif);


	if(mrc_search(src, grp)) {
	//		IF_DEBUG(DEBUG_MRT)
	//		    logit(LOG_DEBUG, 0, "rdl: mrc_add: found existing entry");
			return;
	}

	temp = (mrc_node *)malloc(sizeof(mrc_node));
	temp->src = src;
	temp->grp = grp;
	temp->iif = iif;
	temp->millis = millis;
	temp->status = status;

	if (head == NULL)
	{
		head = temp;
		head->next = NULL;
	}
	else
	{
		temp->next = head;
		head = temp;
	}
}

void mrc_delete(uint32_t src, uint32_t grp)
{
	mrc_node *temp, *prev;
	temp = head;

	//	IF_DEBUG(DEBUG_MRT)
	//	  logit(LOG_DEBUG,
	//		0,
	//		"rdl: mrc_delete(src %s grp %s)",
	//		inet_fmt(src, s1, sizeof(s1)),
	//		inet_fmt(grp, s2, sizeof(s2)));

		while(temp != NULL)
	{
		if ((temp->src == src) && (temp->grp == grp))
		{
			if (temp == head)
			{
				head = temp->next;
				free(temp);
				return;
			}
			else
			{
				prev->next = temp->next;
				free(temp);
				return;
			}
		}
		else
		{
			prev = temp;
			temp = temp->next;
		}
	}
	
	return;
}
 
mrc_node *mrc_search(uint32_t src, uint32_t grp)
{
	mrc_node *temp;
	temp = head;

	while (temp != NULL)
	{
		if ((temp->src == src) && (temp->grp == grp))
			return temp;
		else
			temp = temp->next;
	}

	return NULL;
}
 
void  mrc_dump()
{
	mrc_node *r ;

//	logit(LOG_DEBUG, 0, "URC Dump") ;

	r = head ;
	if(r == NULL)
		return ;
	
	while(r != NULL)
	{
		// logit(LOG_DEBUG, 0, "%s %s %d %p", inet_fmt(r->src, s1, sizeof(s1)), inet_fmt(r->grp, s2, sizeof(s2)), r->iif, r->next) ;
		r = r->next ;
	}

	printf("\n") ;
}
 
 
int count()
{
	mrc_node *n ;
	int c = 0 ;
	n = head ;
	while(n != NULL)
{
	n = n->next ;
	c++ ;
}
return c ;
}

