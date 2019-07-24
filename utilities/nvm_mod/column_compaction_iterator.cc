#include "column_compaction_iterator.h"

namespace rocksdb {

InternalIterator* NewColumnCompactionItemIterator(const InternalKeyComparator* icmp, char *raw_data,FileEntry* file,uint64_t first_key_index, uint64_t keys_num,bool use_buffer){
    if(!use_buffer) {
        return new ColumnCompactionItemIterator(icmp, raw_data,file,first_key_index,keys_num);
    }
    else {
        return new ColumnCompactionWithBufferIterator(raw_data,file,first_key_index,keys_num);
    }
}

InternalIterator* NewNVMLevel0ReadIterator(const InternalKeyComparator* icmp, char *raw_data,FileEntry* file,uint64_t first_key_index,uint64_t keys_num){
    return new NVMLevel0ReadIterator(icmp, raw_data,file,first_key_index,keys_num);
}

}