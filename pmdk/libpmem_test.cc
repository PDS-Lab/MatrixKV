#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <libpmem.h>

/* using 4k of pmem for this example */
#define PMEM_LEN 64ul*1024*1024

#define PATH "/pmem/ceshi.pool"

int
main(int argc, char *argv[])
{
	char *pmemaddr;
	size_t mapped_len;
	int is_pmem;

	/* create a pmem file and memory map it */
	if ((pmemaddr = (char *)pmem_map_file(PATH, PMEM_LEN, PMEM_FILE_CREATE,
				0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}
    printf("mapped_len:%ld - %.2f MB is_pmem:%d\n",mapped_len,1.0*mapped_len/1024/1024,is_pmem);

	/* store a string to the persistent memory */
	memcpy(pmemaddr, "hello, persistent memory",100);

	/* flush above strcpy to persistence */
	if (is_pmem)
		pmem_persist(pmemaddr, mapped_len);
	else
		pmem_msync(pmemaddr, mapped_len);

	/*
	 * Delete the mappings. The region is also
	 * automatically unmapped when the process is
	 * terminated.
	 */
    printf("%s\n",pmemaddr);
	pmem_unmap(pmemaddr, mapped_len);
}