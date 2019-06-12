#include "global_statistic.h"

namespace rocksdb{
#ifdef STATISTIC_OPEN
    GLOBAL_STATS global_stats;
#endif

uint64_t get_now_micros(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000000 + tv.tv_usec;
}

}
