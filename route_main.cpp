#include "route_manager.h"

int main(int argc, const char *argv[])
{
	/* code */
	CRoute_manager route_module;

	pthread_join(route_module.mt_thread_id, NULL);
	pthread_join(route_module.info_thread_id,NULL);

	return 0;
}