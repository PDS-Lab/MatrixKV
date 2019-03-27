#include "nvm_cf_mod.h"

#include "db/version_set.h"

namespace rocksdb {

NvmCfModule::NvmCfModule(NvmCfOptions* nvmcfoption, const std::string& cf_name,
                         uint32_t cf_id, const InternalKeyComparator* icmp)
    : nvmcfoption_(nvmcfoption) {
  vinfo_ = new VolatileInfo(icmp);
  nvm_dir_no_exists_and_creat(nvmcfoption_->pmem_path);
  char buf[100];
  snprintf(buf, sizeof(buf), "%s/cf_%u_%s.pool",
           nvmcfoption_->pmem_path.c_str(), cf_id, cf_name.c_str());
  std::string pol_path(buf, strlen(buf));
  

  
  if (nvm_file_exists(pol_path.c_str()) != 0) {
    RECORD_LOG("creat %s\n",pol_path.c_str());
    pop_ = pmem::obj::pool<PersistentCfInfo>::create(
        pol_path.c_str(), "NvmCfModule", nvmcfoption_->cf_pmem_size,
        CREATE_MODE_RW);

  } else {
    RECORD_LOG("open %s\n",pol_path.c_str());
    pop_ = pmem::obj::pool<PersistentCfInfo>::open(pol_path.c_str(),
                                                   "NvmCfModule");
  }

  pinfo_ = pop_.root();
  if (!pinfo_->inited_) {
    snprintf(buf, sizeof(buf), "%s/cf_%u_%s_sstable.pool",
             nvmcfoption_->pmem_path.c_str(), cf_id, cf_name.c_str());
    std::string pol_path2(buf, strlen(buf));
    RECORD_LOG("pol_path2:%s\n", pol_path2.c_str());
    transaction::run(pop_, [&] {
      pinfo_->sst_bitmap_ = make_persistent<PersistentBitMap>(
          pop_, nvmcfoption_->level0_stop_writes_trigger);
      pinfo_->ptr_sst_ = make_persistent<PersistentSstable>(
          pol_path2, nvmcfoption_->write_buffer_size + 1 * 1024 * 1024,
          nvmcfoption_->level0_stop_writes_trigger, pinfo_->sst_bitmap_);
      pinfo_->sst_meta_ = make_persistent<SstableMetadata>(pop_,icmp,nvmcfoption_->level0_stop_writes_trigger);
      pinfo_->inited_ = true;
    });
  } else if (nvmcfoption_->reset_nvm_storage) {
    // reset
    //pinfo_->ptr_sst_->Reset();

  } else {
    // rebuild cache
  }
}
NvmCfModule::~NvmCfModule() {
  RECORD_LOG("NvmCfModule:close\n");
  delete vinfo_;
  pop_.close();
}
void NvmCfModule::Delete() {}

bool NvmCfModule::AddL0TableRoom(uint64_t filenum, char** raw,
                    persistent_ptr<FileEntry>& file) {
  int index = -1;
  char* tmp = nullptr;
  tmp = pinfo_->ptr_sst_->AllocSstable(index);
  if (index == -1 || tmp == nullptr) {
    printf("error:AddL0TableRoom AllocSstable error!\n");
    return false;
  }
  *raw = tmp;
  persistent_ptr<FileEntry> filetmp = nullptr;
  filetmp = pinfo_->sst_meta_->AddFile(filenum, index, 0);
  if (filetmp == nullptr) {
    printf("error:AddL0TableRoom AddFile error!\n");
    return false;
  } else {
    file = filetmp;
  }
  RECORD_LOG("add L0 table:%lu index:%d\n",filenum,index);
  return true;
}

ColumnCompactionItem* NvmCfModule::PickColumnCompaction(VersionStorageInfo* vstorage){
  ColumnCompactionItem* c = nullptr;
  c = new ColumnCompactionItem();
  //todo:选择数据
  auto L0files = vstorage->LevelFiles(0);
  UpdateCompactionState(L0files.size());

  uint64_t immufiles_num = pinfo_->sst_meta_->GetImmuFileEntryNum();
  assert(immufiles_num != 0);
  
  std::vector<persistent_ptr<FileEntry>> immufiles;
  uint64_t *keys_num = new uint64_t[immufiles_num];   //file对应加入compaction的keys num
  uint64_t *keys_size = new uint64_t[immufiles_num];
  immufiles.reserve(immufiles_num);
  
  persistent_ptr<FileEntry> tmp = nullptr;
  RECORD_LOG("immu L0table[");
  for(tmp=pinfo_->sst_meta_->immu_head;tmp != nullptr; tmp=tmp->next){
    immufiles.push_back(tmp);
    RECORD_LOG("%lu ",tmp->filenum);
  }
  RECORD_LOG("]\n");
  if(immufiles.size() > L0files.size()){
    RECORD_LOG("error:immufiles:%lu L0files:%lu\n",immufiles.size(),L0files.size());
  }
  for(unsigned int i=0;i < immufiles_num; i++){
    keys_num[i] = 0;
    keys_size[i] = 0;
  }

  uint64_t all_comption_size = 0;
  auto user_comparator = vinfo_->icmp_->user_comparator(); //比较只根据user key比较
  KeysMergeIterator* k_iter = new KeysMergeIterator(&immufiles,user_comparator);
  
  uint64_t L1NoneCompactionSizeStop = Column_compaction_no_L1_select_L0 * nvmcfoption_->target_file_size_base;
  uint64_t L1HaveCompactionSizeStop = Column_compaction_have_L1_select_L0 * nvmcfoption_->target_file_size_base;
  int files_index = -1;
  int keys_index = -1;
  uint64_t itemsize = 0;

  InternalKey minsmallest; //  smallest <= immufiles  <= largest       
  InternalKey maxlargest;

  k_iter->SeekToLast();
  if(k_iter->Valid()){
    k_iter->GetCurret(files_index,keys_index);
    maxlargest = immufiles.at(files_index)->keys_meta[keys_index].key;
  }
  k_iter->SeekToFirst();
  if(k_iter->Valid()){
    k_iter->GetCurret(files_index,keys_index);
    minsmallest = immufiles.at(files_index)->keys_meta[keys_index].key;
    RECORD_LOG("L0 minsmallest:%s maxlargest:%s\n",minsmallest.DebugString(true).c_str(),maxlargest.DebugString(true).c_str());
  }

  auto L1files = vstorage->LevelFiles(1);
  for(unsigned int i = 0;i < L1files.size();i++){
    RECORD_LOG("L1table:%lu [%s-%s]\n",L1files.at(i)->fd.GetNumber(),L1files.at(i)->smallest.DebugString(true).c_str(),L1files.at(i)->largest.DebugString(true).c_str());
  }
  std::vector<FileMetaData*> L1overlapfiles;
  vstorage->GetOverlappingInputs(1,&minsmallest,&maxlargest,&L1overlapfiles);
  for(unsigned int i = 0;i < L1overlapfiles.size();i++){
    RECORD_LOG("L1over:%lu [%s-%s]\n",L1overlapfiles.at(i)->fd.GetNumber(),L1overlapfiles.at(i)->smallest.DebugString(true).c_str(),L1overlapfiles.at(i)->largest.DebugString(true).c_str());
  }


  if(L1overlapfiles.size() == 0){  //L1没有交集文件，根据数据量选取
    RECORD_LOG("nvm cf pick no L1\n");
    if(k_iter->Valid()){
      k_iter->GetCurret(files_index,keys_index);
      c->L0smallest = immufiles.at(files_index)->keys_meta[keys_index].key;
    }
    while(k_iter->Valid()){
      if(all_comption_size >= L1NoneCompactionSizeStop) {
        c->L0largest = immufiles.at(files_index)->keys_meta[keys_index].key;
        break;
      }
      k_iter->GetCurret(files_index,keys_index);
      itemsize = immufiles.at(files_index)->keys_meta[keys_index].size;
      keys_num[files_index]++;
      keys_size[files_index] += itemsize;
      all_comption_size += itemsize;
      k_iter->Next();
    }
    if(all_comption_size > 0 && !k_iter->Valid()){
      c->L0largest = immufiles.at(files_index)->keys_meta[keys_index].key;
    }
    for(unsigned int index = 0;index < immufiles_num;index++){
      if(keys_num[index] != 0){
        tmp = immufiles.at(index);
        c->files.push_back(tmp);
        c->keys_num.push_back(keys_num[index]);
        c->keys_size.push_back(keys_size[index]);
      }
    }
    c->L0select_size = all_comption_size;
    unsigned int j = 0;
    FileMetaData* ftmp = nullptr;
    uint64_t filenum = 0;
    L0files = vstorage->LevelFiles(0);
    for(unsigned int i = 0;i < c->files.size();i++){
      ftmp = nullptr;
      filenum = c->files.at(i)->filenum;
      j = 0;
      while(j < L0files.size()){
        if(L0files.at(j)->fd.GetNumber() == filenum){
          ftmp = L0files.at(j);
          break;
        }
        j++;
      }
      if(ftmp != nullptr){
        c->L0compactionfiles.push_back(ftmp);
      }else{
        printf("error:no find L0:%lu table!\n",filenum);
      }
    }
    delete k_iter;
    delete []keys_num;
    delete []keys_size;
    return c;

  }else{  //L1有交集文件，根据文件分隔选取
    RECORD_LOG("nvm cf pick have L1\n");
    std::vector<InternalKey> L1Ranges; //L1交集文件组成分隔范围  ---|f1.smallest|---|f1.largest|---|f2.smallest|---|f2.largest|---
    for(unsigned int i = 0;i < L1overlapfiles.size(); i++){
      L1Ranges.emplace_back(L1overlapfiles.at(i)->smallest);
      L1Ranges.emplace_back(L1overlapfiles.at(i)->largest);
    }
    unsigned int L1Range_index = 0;
    InternalKey key_current;
    
    if(k_iter->Valid()){
      k_iter->GetCurret(files_index,keys_index);
      c->L0smallest = immufiles.at(files_index)->keys_meta[keys_index].key;
    }
    while(k_iter->Valid()){
      k_iter->GetCurret(files_index,keys_index);
      key_current = immufiles.at(files_index)->keys_meta[keys_index].key;
      if( ((L1Range_index != L1Ranges.size()) && (L1Range_index % 2 == 0) && (user_comparator->Compare(ExtractUserKey(key_current.Encode()),ExtractUserKey(L1Ranges.at(L1Range_index).Encode())) >= 0)) || \
            ((L1Range_index % 2 == 1) && (user_comparator->Compare(ExtractUserKey(key_current.Encode()),ExtractUserKey(L1Ranges.at(L1Range_index).Encode())) > 0)) ){
        if(all_comption_size >= L1HaveCompactionSizeStop){
          c->L0largest = immufiles.at(files_index)->keys_meta[keys_index].key;
          break;
        }
        L1Range_index++;
      }
      itemsize = immufiles.at(files_index)->keys_meta[keys_index].size;
      keys_num[files_index]++;
      keys_size[files_index] += itemsize;
      all_comption_size += itemsize;
      k_iter->Next();
    }
    if(all_comption_size > 0 && !k_iter->Valid()){
      c->L0largest = immufiles.at(files_index)->keys_meta[keys_index].key;
    }

    for(unsigned int index = 0;index < immufiles_num;index++){
      if(keys_num[index] != 0){
        tmp = immufiles.at(index);
        c->files.push_back(tmp);
        c->keys_num.push_back(keys_num[index]);
        c->keys_size.push_back(keys_size[index]);
      }
    }
    c->L0select_size = all_comption_size;
    unsigned int j = 0;
    FileMetaData* ftmp = nullptr;
    uint64_t filenum = 0;
    L0files = vstorage->LevelFiles(0);
    for(unsigned int i = 0;i < c->files.size();i++){
      ftmp = nullptr;
      filenum = c->files.at(i)->filenum;
      j = 0;
      while(j < L0files.size()){
        if(L0files.at(j)->fd.GetNumber() == filenum){
          ftmp = L0files.at(j);
          break;
        }
        j++;
      }
      if(ftmp != nullptr){
        c->L0compactionfiles.push_back(ftmp);
      }else{
        printf("error:no find L0:%lu table!\n",filenum);
      }
    }
    RECORD_LOG("L1Range_index:%u\n",L1Range_index);
    for(unsigned int i = 0;i < ((L1Range_index + 1)/2);i++){
      c->L1compactionfiles.push_back(L1overlapfiles.at(i));
    }
    delete k_iter;
    delete []keys_num;
    delete []keys_size;
    return c;


  }
  delete k_iter;
  delete []keys_num;
  delete []keys_size;
  return c;
}
double NvmCfModule::GetCompactionScore(){
  double score = 0;
  uint64_t immutablenum = pinfo_->sst_meta_->GetImmuFileEntryNum();
  score = 1.0 + (double)immutablenum/4;
  return score;

}
void NvmCfModule::DeleteL0file(uint64_t filenumber){
  persistent_ptr<FileEntry> tmp = pinfo_->sst_meta_->FindFile(filenumber);
  if(tmp == nullptr) return;
  pinfo_->ptr_sst_->DeleteSstable(tmp->sstable_index);
  pinfo_->sst_meta_->DeteleFile(filenumber);
}


NvmCfModule* NewNvmCfModule(NvmCfOptions* nvmcfoption,const std::string &cf_name,uint32_t cf_id,const InternalKeyComparator* icmp){
  return new NvmCfModule(nvmcfoption,cf_name,cf_id,icmp);
}

}  // namespace rocksdb