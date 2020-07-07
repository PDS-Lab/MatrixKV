#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>

#include <libpmemobj.h>

#define PATH_FILE "/pmem/test.pool"
#define POOL_SIZE ((uint64_t)(20ull << 30))
#define WRITE_SIZE ((uint64_t)(8ull <<25 ))
#define BUF_SIZE (4*1024)

POBJ_LAYOUT_BEGIN(my_test);
POBJ_LAYOUT_ROOT(my_test, struct my_root);
POBJ_LAYOUT_TOID(my_test,struct my_data);
POBJ_LAYOUT_END(my_test);


struct my_root {
	TOID(struct my_data) dat[WRITE_SIZE/BUF_SIZE];
};

struct my_data {
    int len;
    char buf[];
};

static uint64_t get_now_micros(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000000 + tv.tv_usec;
}

static int write_size(PMEMobjpool *pop, TOID(struct my_data) *dat,const char *data, int len){
    int ret = 0;
    TX_BEGIN(pop) {
		
		TOID(struct my_data) entry = TX_ALLOC(struct my_data,sizeof(struct my_data) + len);
		D_RW(entry)->len = len;
		memcpy(D_RW(entry)->buf, data, len);

		TX_ADD_DIRECT(dat);
		*dat = entry;
	} TX_ONABORT { 
		ret = -1;
	} TX_END

	return ret;
}

int main(int argc, char *argv[]){

    if(access(PATH_FILE,F_OK) == 0){
        if(remove(PATH_FILE) != 0){
            perror(PATH_FILE);
            return 1;
        }
    }

    PMEMobjpool *pop = pmemobj_create(PATH_FILE,POBJ_LAYOUT_NAME(my_test), POOL_SIZE, 0666);

    if(pop == NULL) {
        perror(PATH_FILE);
        return 1;
    }
    
    TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);
    struct my_root *rootp = D_RW(root);

    uint64_t dat_num = WRITE_SIZE/BUF_SIZE;
    
    char *buf;
    buf=(char *)malloc(BUF_SIZE);
    //memset(buf,0,BUF_SIZE);
    memset(buf,'1',BUF_SIZE);

    uint64_t start_time,end_time,do_time,next_report_,i;
    int ret;

    next_report_=0;
    start_time=get_now_micros();
    for(i=0;i<dat_num;i++){
        ret=write_size(pop,&(rootp->dat[i]),buf,BUF_SIZE);
        if(ret != 0){
            printf("ret:%d write falid!\n",ret);
            return 0;
        }

        if (i >= next_report_) {
            if      (next_report_ < 1000)   next_report_ += 100;
            else if (next_report_ < 5000)   next_report_ += 500;
            else if (next_report_ < 10000)  next_report_ += 1000;
            else if (next_report_ < 50000)  next_report_ += 5000;
            else if (next_report_ < 100000) next_report_ += 10000;
            else if (next_report_ < 500000) next_report_ += 50000;
            else                            next_report_ += 100000;
            fprintf(stderr, "... finished %d ops%30s\r", i, "");
            fflush(stderr);
        }
    }

    end_time=get_now_micros();
    do_time=end_time-start_time;
    printf("all_write_size:%ld MB write_op:%ld write_size:%ld K\n",WRITE_SIZE/1048576,dat_num,BUF_SIZE/1024);
    printf("%11.3f micros/op %6.1f MB/s\n",(1.0*do_time)/(dat_num),(WRITE_SIZE/1048576.0)/(do_time*1e-6));

    pmemobj_close(pop);
    free(buf);
    return 0;
}