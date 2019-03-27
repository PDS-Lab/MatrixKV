#include <stdio.h>
#include <string.h>
#include <libpmemobj.h>

#define LAYOUT_NAME "mydata1"
#define POOL_PATH "/pmem/test.pool"

struct my_data{
    int num;
    char buf[20];
};

int main(int argc, char *argv[])
{

    PMEMobjpool *pool = pmemobj_create(POOL_PATH, LAYOUT_NAME, PMEMOBJ_MIN_POOL, 0666);
    if (pool == NULL) {
        pool = pmemobj_open(POOL_PATH, LAYOUT_NAME);
    }
    if (pool == NULL) {
        perror("pmemobj_open");
        return 1;
    }
    // 从内存池中得到对应数据结构的根节点，并得到该根节点的指针
    PMEMoid root = pmemobj_root(pool, sizeof(struct my_data));
    struct my_data *rootp = (struct my_data *)pmemobj_direct(root);

    char buf[] = "hello world";
    printf("the first num:%ld\n",rootp->num);
    // 将数据写入根节点对应的地址，以实现持久化:

    rootp->num = 30;
    pmemobj_persist(pool, &rootp->num, sizeof(rootp->num));
    pmemobj_memcpy_persist(pool, rootp->buf, buf, strlen(buf));

    pmemobj_close(pool);

    return 0;
}