#pragma once

#define STATISTIC_OPEN

namespace rocksdb{
#ifdef STATISTIC_OPEN
    struct GLOBAL_STATS {
        int read_count;
        GLOBAL_STATS(){
            read_count = 0;
        }
    };
    extern struct GLOBAL_STATS global_stats;
    
#endif
}
