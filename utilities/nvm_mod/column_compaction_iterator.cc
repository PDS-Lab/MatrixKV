#include "column_compaction_iterator.h"

namespace rocksdb {

InternalIterator* NewColumnCompactionItemIterator(char *raw_data,persistent_ptr<FileEntry> &file,uint64_t first_key_index, uint64_t keys_num){
    return new ColumnCompactionItemIterator(raw_data,file,first_key_index,keys_num);
}

}