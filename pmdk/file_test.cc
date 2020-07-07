#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>


using namespace std;
const char filename[]="/pmem/pmdk_test_dir/";
const uint64_t file_op = 1;
const uint64_t file_size=(20ull << 30);
const uint64_t write_size=(64*1024*1024);
const uint64_t read_size=(1024*1024);
const uint64_t read_op=1000;

uint64_t get_now_micros(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000000 + tv.tv_usec;
}

int main(int argc, char **argv)
{
    int fd;
    
    uint64_t ret,start_time,end_time,do_time,next_report_;
    void *buf;
    uint64_t ofst=0;
    int buf_num=sizeof(char)*write_size;
    buf=(void *)malloc(buf_num);
    memset(buf,'1',buf_num);
    
    char namebuf[100];

    start_time=get_now_micros();
    next_report_=0;
    int i,j;
    bool error_flag=false;
    for(i=0;i<file_op;i++){
        snprintf(namebuf,sizeof(namebuf),"%sceshi%04d",filename,i);
        fd=open(namebuf,O_RDWR | O_CREAT | O_TRUNC | O_DIRECT);
        if(fd==-1){
            printf("open failed! i:%d \n",i);
            return 0;
        }
        ofst = 0;
        for(j=0;j<(file_size/write_size);j++){
            ret=pwrite(fd, buf, write_size, ofst);
            if(ret<=0){
                printf("ret:%ld pwrite falid!\n",ret);
                error_flag = true;
                break;
            }
            ofst += write_size;
        }
        if(error_flag){
            break;
        }
        close(fd);
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
    printf("file_size:%ld MB file_op:%ld write_size:%ld K\n",file_size/1048576,file_op,write_size/1024);
    printf("%11.3f micros/op %6.1f MB/s\n",(1.0*do_time)/(file_op*((file_size/write_size))),(file_size*file_op/1048576.0)/(do_time*1e-6));
    
    /*fd=open(filename,O_RDWR | O_CREAT | O_TRUNC);
    if(fd==-1){
        printf("open failed! i:%ld \n",i);
        return 0;
    }
    start_time=get_now_micros();
    next_report_=0;
    for(i=0;i<read_op;i++){
        ret=pread(fd, buf, read_size,ofst);
        if(ret<=0){
            printf("ret:%ld zbc_pread falid!\n",ret);
            break;
        }
        ofst = (ofst+read_size)%write_size;

        if (i >= next_report_) {
        if      (next_report_ < 1000)   next_report_ += 100;
        else if (next_report_ < 5000)   next_report_ += 500;
        else if (next_report_ < 10000)  next_report_ += 1000;
        else if (next_report_ < 50000)  next_report_ += 5000;
        else if (next_report_ < 100000) next_report_ += 10000;
        else if (next_report_ < 500000) next_report_ += 50000;
        else                            next_report_ += 100000;
        fprintf(stderr, "... finished %ld ops%30s\r", i, "");
        fflush(stderr);
    }
    }
    end_time=get_now_micros();
    do_time=end_time-start_time;
    printf("read_size:%ld KB read_op:%ld \n",read_size/1024,read_op);
    printf("%11.3f micros/op %6.1f MB/s\n",(1.0*do_time)/read_op,(read_size*read_op/1048576.0)/(do_time*1e-6));*/

    free(buf);
    return 0;

}
