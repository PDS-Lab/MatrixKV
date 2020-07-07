#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <libpmemlog.h>

int main(int argc, char *argv[])
{
	const char path[] = "/pmem/pmdk_test_dir/ceshi0000.pool";
	PMEMlogpool *plp;
	size_t nbyte;

    
	plp = pmemlog_open(path);

	if (plp == NULL) {
		perror(path);
		return 1;
	}

	/* how many bytes does the log hold? */
	nbyte = pmemlog_nbyte(plp);
	printf("log holds %zu bytes\n", nbyte);

    pmemlog_close(plp);
    return 0;
}