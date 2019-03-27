#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <iostream>

#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

using pmem::obj::delete_persistent;
using pmem::obj::make_persistent;
using pmem::obj::p;
using pmem::obj::persistent_ptr;
using pmem::obj::pool;
using pmem::obj::pool_base;
using pmem::obj::transaction;

#define CREATE_MODE_RW (S_IWUSR | S_IRUSR)

static inline int file_exists(char const *file) { return access(file, F_OK); }

#define LAYOUT_NAME "mydata1"
#define POOL_PATH "/pmem/testobj.pool"

struct my_data{
    p<int> num;
    char data[30];
    persistent_ptr<char[]> str = nullptr;
};

int main(int argc, char *argv[])
{
    pool<my_data> pop_;
    if (file_exists(POOL_PATH) != 0) {
        
        pop_ = pmem::obj::pool<my_data>::create(POOL_PATH, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
                                                       CREATE_MODE_RW);
    } else {
        pop_ = pmem::obj::pool<my_data>::open(POOL_PATH, LAYOUT_NAME);
    }
    persistent_ptr<my_data> pinfo_ = pop_.root();
    printf("num:%d\n",pinfo_->num);
    pinfo_->num=pinfo_->num + 1;
    printf("num:%d\n",pinfo_->num);
    char buf[] = "hello world 0";
    memcpy(pinfo_->data,buf,strlen(buf));
    printf("data:%s\n",pinfo_->data);

    if(pinfo_->str == nullptr){
        printf("str:null!\n");
        transaction::run(pop_, [&] {
        pinfo_->str = make_persistent<char[]>(30);
        });
        pinfo_->str[0]='1';
        printf("str:%c\n",pinfo_->str[0]);
    }
    else{
        pinfo_->str[0]=pinfo_->str[0] + 1;
        pinfo_->str[1]='2';
        pinfo_->str[2]='2';
        pinfo_->str[3]=0;
        memcpy(&(pinfo_->str[0]),buf,20);
        printf("str_p:%c\n",pinfo_->str[0]);
        printf("str:no null!\n");
    }




    pop_.close();

    return 0;
}