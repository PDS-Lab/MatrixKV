//
//功能：nvm对应column_family
//

#pragma once

#include <memory>
#include <string>

//#include "db/version_set.h"

#include "common.h"
#include "nvm_option.h"
#include "persistent_bitmap.h"
#include "persistent_sstable.h"
#include "my_log.h"
#include "sstable_meta.h"
#include "column_compaction.h"
#include "keys_merge_iterator.h"


namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;
struct FileEntry;
class SstableMetadata;
class VersionStorageInfo;


class NvmCfModule {
 public:
  NvmCfModule(NvmCfOptions* nvmcfoption,const std::string &cf_name,uint32_t cf_id,const InternalKeyComparator* icmp);
  ~NvmCfModule();
  void Delete();
  bool AddL0TableRoom(uint64_t filenum,char** raw,persistent_ptr<FileEntry> &file);
  uint64_t GetSstableEachSize() { return pinfo_->ptr_sst_->GetEachSize(); }
  bool NeedsColumnCompaction() { return pinfo_->sst_meta_->immu_head != nullptr; }
  void UpdateKeyNext(persistent_ptr<FileEntry> &file) { pinfo_->sst_meta_->UpdateKeyNext(file); }
  void UpdateCompactionState(int num) { pinfo_->sst_meta_->UpdateCompactionState(num); }

  ColumnCompactionItem* PickColumnCompaction(VersionStorageInfo* vstorage);
  const InternalKeyComparator* GetInternalKeyComparator() { return vinfo_->icmp_; }
  double GetCompactionScore();

  char* GetIndexPtr(int index) { return pinfo_->ptr_sst_->GetIndexPtr(index); }

  void DeleteL0file(uint64_t filenumber);

  bool Get(VersionStorageInfo* vstorage,Status *s,const LookupKey &lkey,std::string *value);


 private:
  bool UserKeyInRange(Slice *user_key,InternalKey *start,InternalKey *end);
  bool BinarySearchInFile(persistent_ptr<FileEntry> &file,Slice *user_key,int *find_index,int *pre_left = nullptr,int *pre_right = nullptr);

  NvmCfOptions* nvmcfoption_;

  struct PersistentCfInfo {
    p<bool> inited_;
    persistent_ptr<PersistentBitMap> sst_bitmap_;
    persistent_ptr<PersistentSstable> ptr_sst_;
    persistent_ptr<SstableMetadata> sst_meta_;
  };

  pool<PersistentCfInfo> pop_;
  persistent_ptr<PersistentCfInfo> pinfo_;

  struct VolatileInfo {
    const InternalKeyComparator* icmp_;

    VolatileInfo(const InternalKeyComparator* icmp):icmp_(icmp){}
    ~VolatileInfo(){}

  };

  VolatileInfo *vinfo_;
};

NvmCfModule* NewNvmCfModule(NvmCfOptions* nvmcfoption,const std::string &cf_name,uint32_t cf_id,const InternalKeyComparator* icmp);
}  // namespace rocksdb