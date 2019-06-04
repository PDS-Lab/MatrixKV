#include <stdio.h>
#include <string.h>
#include <libpmemobj.h>


#define POOL_SIZE ((uint64_t)(65 << 20))
#define MAX_BUF_LEN 100

POBJ_LAYOUT_BEGIN(string_store);
POBJ_LAYOUT_ROOT(string_store, struct my_root);
POBJ_LAYOUT_END(string_store);

const char filename[]="/pmem/ceshi.pool";

struct my_root {
	char buf[MAX_BUF_LEN];
};

int main(int argc, char *argv[])
{

	PMEMobjpool *pop = pmemobj_create(filename,POBJ_LAYOUT_NAME(string_store), POOL_SIZE, 0666);

	if (pop == NULL) {
		pop=pmemobj_open(filename, POBJ_LAYOUT_NAME(string_store));
	}
    if(pop == NULL) {
        perror(filename);
        return 1;
    }

	char buf[]="abcdefg";
    char buf2[100];
    memset(buf2,0,sizeof(buf2));

	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	TX_BEGIN(pop) {
		TX_MEMCPY(D_RW(root)->buf, buf, strlen(buf));
	} TX_END

    printf("%s\n",D_RO(root)->buf + 2);
    memcpy(buf2,D_RO(root)->buf + 3,MAX_BUF_LEN - 4);
    printf("%s\n",buf2);
	pmemobj_close(pop);

	return 0;
}
