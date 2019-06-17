#include "column_compaction_iterator.h"

namespace rocksdb {

InternalIterator* NewColumnCompactionItemIterator(char *raw_data,FileEntry* file,uint64_t first_key_index, uint64_t keys_num,bool use_buffer){
    if(!use_buffer) {
        return new ColumnCompactionItemIterator(raw_data,file,first_key_index,keys_num);
    }
    else {
        return new ColumnCompactionWithBufferIterator(raw_data,file,first_key_index,keys_num);
    }
}

}