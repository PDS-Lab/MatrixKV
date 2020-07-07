#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <libpmemlog.h>

#define POOL_SIZE ((uint64_t)(64 << 20))

using namespace std;

const char filename[]="/pmem/pmdk_test_dir/";
const uint64_t file_op = 20;
const uint64_t write_size=(4*1024*1024);
const uint64_t read_size=(1024*1024);
const uint64_t read_op=1000;

static uint64_t get_now_micros(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000000 + tv.tv_usec;
}
static int printit(const void *buf, size_t len, void *arg)
{
	memcpy(arg, buf, len);
	return 0;
}

int main(int argc, char **argv)
{
    
    uint64_t ret,start_time,end_time,do_time,next_report_;
    void *buf;
    void *cp_buf;
    int buf_num=sizeof(char)*write_size;
    buf=(void *)malloc(buf_num);
    cp_buf=(void *)malloc(buf_num);
    memset(buf,'1',buf_num);

    PMEMlogpool *plp;
    
    char namebuf[100];

    start_time=get_now_micros();
    next_report_=0;
    int i,j;
    bool error_flag=false;
    for(i=0;i<file_op;i++){
        snprintf(namebuf,sizeof(namebuf),"%sceshi%04d.pool",filename,i);
        plp = pmemlog_create(namebuf, POOL_SIZE, 0666);

        if (plp == NULL)
            plp = pmemlog_open(namebuf);

        if (plp == NULL) {
            perror(namebuf);
            return 1;
        }
        pmemlog_rewind(plp);
        for(j=0;j<(POOL_SIZE/write_size);j++){
            ret=pmemlog_append(plp, buf, write_size);
            if(ret<0){
                printf("ret:%ld append falid!\n",ret);
                error_flag = true;
                break;
            }
        }
        if(error_flag){
            break;
        }
        pmemlog_close(plp);
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
    printf("file_size:%ld MB file_op:%ld write_size:%ld K\n",POOL_SIZE/1048576,file_op,write_size/1024);
    printf("%11.3f micros/op %6.1f MB/s\n",(1.0*do_time)/(file_op*((POOL_SIZE/write_size))),(POOL_SIZE*file_op/1048576.0)/(do_time*1e-6));
    
    

    free(buf);
    free(cp_buf);
    return 0;

}