#include "defs.h"

struct rpfctl rpf;
char *ident       = "pimd_netlink";
int mrt_table_id = 0;
struct rp_hold *g_rp_hold = NULL;
int do_vifs       = 1;
char *config_file = "/etc/pimd.conf";

int main(int argc, char *argv[])
{
	debug = DEBUG_ALL;
	
	log_init(TRUE);

	init_vifs();
	init_routesock();
	
	k_req_incoming(inet_parse("10.153.128.1", 4), &rpf);
	k_req_incoming(inet_parse("10.153.128.1", 4), &rpf);
	k_req_incoming(inet_parse("10.153.128.1", 4), &rpf);
	k_req_incoming(inet_parse("10.153.128.1", 4), &rpf);
	
	logit(LOG_DEBUG, 0, "Done");
	exit(0);
}