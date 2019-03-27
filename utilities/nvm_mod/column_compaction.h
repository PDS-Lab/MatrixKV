
#pragma once

#include "common.h"

#include "sstable_meta.h"

#include "db/dbformat.h"

namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;
struct FileMetaData;
class InternalKey;

struct ColumnCompactionItem{
public:
    ColumnCompactionItem(){};
    ~ColumnCompactionItem(){};

    std::vector<persistent_ptr<FileEntry>> files;  //新的先插入，旧的在后面
    std::vector<uint64_t> keys_num; //对应files的需要compaction的key个数
    std::vector<uint64_t> keys_size; //对应的key-value的总大小
    uint64_t L0select_size;

    std::vector<FileMetaData*> L0compactionfiles;
    std::vector<FileMetaData*> L1compactionfiles;

    InternalKey L0smallest; //  smallest <= compaction的范围  <= largest       
    InternalKey L0largest;

    

};


}