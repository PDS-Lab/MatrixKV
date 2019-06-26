#include "nvm_cf_mod.h"

#include "db/version_set.h"
#include "column_compaction_iterator.h"
#include "global_statistic.h"

namespace rocksdb {

NvmCfModule::NvmCfModule(NvmCfOptions* nvmcfoption, const std::string& cf_name,
                         uint32_t cf_id, const InternalKeyComparator* icmp)
    : nvmcfoption_(nvmcfoption),icmp_(icmp) {
  
  nvm_dir_no_exists_and_creat(nvmcfoption_->pmem_path);
  char buf[100];
  snprintf(buf, sizeof(buf), "%s/cf_%u_%s_sstable.pool",
           nvmcfoption_->pmem_path.c_str(), cf_id, cf_name.c_str());
  std::string pol_path(buf, strlen(buf));
  
  uint64_t level0_table_num = (Level0_column_compaction_stop_size/nvmcfoption_->write_buffer_size + 1)*2;
  ptr_sst_ = new PersistentSstable(pol_path,nvmcfoption_->write_buffer_size + 8ul * 1024 * 1024,
            level0_table_num);
  
  sst_meta_ = new SstableMetadata(icmp_);
  
}
NvmCfModule::~NvmCfModule() {
  RECORD_LOG("NvmCfModule:close\n");
  delete ptr_sst_;
  delete sst_meta_;
}
void NvmCfModule::Delete() {}

bool NvmCfModule::AddL0TableRoom(uint64_t filenum, char** raw,
                    FileEntry** file) {
  int index = -1;
  char* tmp = nullptr;
  tmp = ptr_sst_->AllocSstable(index);
  if (index == -1 || tmp == nullptr) {
    printf("error:AddL0TableRoom AllocSstable error!\n");
    return false;
  }
  *raw = tmp;
  FileEntry* filetmp = nullptr;
  filetmp = sst_meta_->AddFile(filenum, index);
  if (filetmp == nullptr) {
    printf("error:AddL0TableRoom AddFile error!\n");
    return false;
  } else {
    *file = filetmp;
  }
  RECORD_LOG("add L0 table:%lu index:%d\n",filenum,index);
  return true;
}

ColumnCompactionItem* NvmCfModule::PickColumnCompaction(VersionStorageInfo* vstorage){
  ColumnCompactionItem* c = nullptr;
  //todo:选择数据
  auto L0files = vstorage->LevelFiles(0);
  UpdateCompactionState(L0files); //更新compaction files



  uint64_t comfiles_num = sst_meta_->compaction_files.size();   //compaction files number
  if(comfiles_num == 0){
    RECORD_LOG("error:comfiles_num == 0, l0:%ld\n",L0files.size());
    return nullptr;
  }
  
  
  std::vector<FileEntry*> comfiles;   //compaction files
  std::vector<uint64_t> first_key_indexs;       //file <-> first_key_indexs
  uint64_t *keys_num = new uint64_t[comfiles_num];   //file对应加入compaction的keys num
  uint64_t *keys_size = new uint64_t[comfiles_num];
  comfiles.reserve(comfiles_num);
  first_key_indexs.reserve(comfiles_num);

  FileEntry* tmp = nullptr;
  unsigned int j = 0;
  bool find_file = false;
  for(unsigned int i=0;i < comfiles_num; i++) {
    tmp = FindFile(sst_meta_->compaction_files[i]);
    comfiles.push_back(tmp);

    j = 0;
    find_file = false;
    while(j < L0files.size()){
      if(L0files.at(j)->fd.GetNumber() == tmp->filenum){
        find_file = true;
        break;
      }
      j++;
    }
    if(find_file){
      first_key_indexs.push_back(L0files.at(j)->first_key_index);  //对应文件的first_key_index插入
    }
    else {
      RECORD_LOG("error:L0files no find_file:%ld",tmp->filenum);
      return nullptr;    //未找到对应文件，错误
    }
  }
  
  RECORD_LOG("compaction comfiles:[");
  for(unsigned int i = 0;i < comfiles.size(); i++){
    RECORD_LOG("%ld ",comfiles[i]->filenum);
  }
  RECORD_LOG("]\n");
  

  for(unsigned int i=0;i < comfiles_num; i++) {
    keys_num[i] = 0;
    keys_size[i] = 0;
  }

  c = new ColumnCompactionItem();
  uint64_t all_comption_size = 0;
  auto user_comparator = icmp_->user_comparator(); //比较只根据user key比较
  KeysMergeIterator* k_iter = new KeysMergeIterator(&comfiles,&first_key_indexs,user_comparator);
  
  uint64_t L1NoneCompactionSizeStop = Column_compaction_no_L1_select_L0 * nvmcfoption_->target_file_size_base - 6ul*1024*1024 * Column_compaction_no_L1_select_L0;  //每个文件减去6MB是为了防止小文件的产生
  uint64_t L1HaveCompactionSizeStop = Column_compaction_have_L1_select_L0 * nvmcfoption_->target_file_size_base - 6ul*1024*1024 * Column_compaction_have_L1_select_L0;
  int files_index = -1;
  int keys_index = -1;
  uint64_t itemsize = 0;

  InternalKey minsmallest; //  smallest <= comfiles  <= largest       
  InternalKey maxlargest;

  k_iter->SeekToLast();
  if(k_iter->Valid()){
    k_iter->GetCurret(files_index,keys_index);
    maxlargest = comfiles.at(files_index)->keys_meta[keys_index].key;
  }
  k_iter->SeekToFirst();
  if(k_iter->Valid()){
    k_iter->GetCurret(files_index,keys_index);
    minsmallest = comfiles.at(files_index)->keys_meta[keys_index].key;
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
      c->L0smallest = comfiles.at(files_index)->keys_meta[keys_index].key;
    }
    while(k_iter->Valid()){
      if(all_comption_size >= L1NoneCompactionSizeStop) {
        int next_files_index,next_keys_index;
        k_iter->GetCurret(next_files_index,next_keys_index);

        if(user_comparator->Compare(ExtractUserKey(comfiles[files_index]->keys_meta[keys_index].key.Encode()),ExtractUserKey(comfiles[next_files_index]->keys_meta[next_keys_index].key.Encode())) == 0){
          //如果与下一个key相等，则继续加入compaction，不论超过大小与否
          k_iter->GetCurret(files_index,keys_index);
          itemsize = comfiles.at(files_index)->keys_meta[keys_index].size;
          keys_num[files_index]++;
          keys_size[files_index] += itemsize;
          all_comption_size += itemsize;
          k_iter->Next();
          continue;
        }
        c->L0largest = comfiles.at(files_index)->keys_meta[keys_index].key;
        break;
      }
      k_iter->GetCurret(files_index,keys_index);
      itemsize = comfiles.at(files_index)->keys_meta[keys_index].size;
      keys_num[files_index]++;
      keys_size[files_index] += itemsize;
      all_comption_size += itemsize;
      k_iter->Next();
    }
    if(all_comption_size > 0 && !k_iter->Valid()){
      c->L0largest = comfiles.at(files_index)->keys_meta[keys_index].key;
    }
    for(unsigned int index = 0;index < comfiles_num;index++){
      if(keys_num[index] != 0){
        tmp = comfiles.at(index);
        c->files.push_back(tmp);
        c->keys_num.push_back(keys_num[index]);
        c->keys_size.push_back(keys_size[index]);
      }
    }
    c->L0select_size = all_comption_size;
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
      c->L0smallest = comfiles.at(files_index)->keys_meta[keys_index].key;
    }
    while(k_iter->Valid()){
      k_iter->GetCurret(files_index,keys_index);
      key_current = comfiles.at(files_index)->keys_meta[keys_index].key;
      if( ((L1Range_index != L1Ranges.size()) && (L1Range_index % 2 == 0) && (user_comparator->Compare(ExtractUserKey(key_current.Encode()),ExtractUserKey(L1Ranges.at(L1Range_index).Encode())) >= 0)) || \
            ((L1Range_index % 2 == 1) && (user_comparator->Compare(ExtractUserKey(key_current.Encode()),ExtractUserKey(L1Ranges.at(L1Range_index).Encode())) > 0)) ){
        if((L1Range_index % 2 == 1) && all_comption_size >= L1HaveCompactionSizeStop){  //在文件范围内时，超过边界满足大小即可
          c->L0largest = comfiles.at(files_index)->keys_meta[keys_index].key;
          break;
        }
        L1Range_index++;
        continue;
      }

      itemsize = comfiles.at(files_index)->keys_meta[keys_index].size;
      keys_num[files_index]++;
      keys_size[files_index] += itemsize;
      all_comption_size += itemsize;
      if((L1Range_index % 2 == 0) && all_comption_size >= L1HaveCompactionSizeStop){  //不在文件范围内时，满足数据量即可
          c->L0largest = comfiles.at(files_index)->keys_meta[keys_index].key;
          break;
      }
      k_iter->Next();
    }
    if(all_comption_size > 0 && !k_iter->Valid()){
      c->L0largest = comfiles.at(files_index)->keys_meta[keys_index].key;
    }

    for(unsigned int index = 0;index < comfiles_num;index++){
      if(keys_num[index] != 0){
        tmp = comfiles.at(index);
        c->files.push_back(tmp);
        c->keys_num.push_back(keys_num[index]);
        c->keys_size.push_back(keys_size[index]);
      }
    }
    c->L0select_size = all_comption_size;
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
  uint64_t compactionfilenum = sst_meta_->GetFilesNumber();
  score = 1.0 + (double)compactionfilenum/4;
  return score;

}
void NvmCfModule::DeleteL0file(uint64_t filenumber){
  FileEntry* tmp = sst_meta_->FindFile(filenumber);
  if(tmp == nullptr) return;
  RECORD_LOG("delete l0 table:%lu\n",filenumber);
  ptr_sst_->DeleteSstable(tmp->sstable_index);
  sst_meta_->DeteleFile(filenumber);
}

bool NvmCfModule::Get(VersionStorageInfo* vstorage,Status *s,const LookupKey &lkey,std::string *value){
  auto L0files = vstorage->LevelFiles(0);
  std::vector<FileEntry*> findfiles;
  std::vector<uint64_t> first_key_indexs;
  
#ifdef STATISTIC_OPEN
  uint64_t start_time = get_now_micros();
#endif
  FileEntry* tmp = nullptr;
  for(unsigned int i = 0;i < L0files.size();i++){
    tmp = sst_meta_->FindFile(L0files.at(i)->fd.GetNumber());
    findfiles.push_back(tmp);
    first_key_indexs.push_back(L0files.at(i)->first_key_index);
  }
#ifdef STATISTIC_OPEN
  uint64_t end_time = get_now_micros();
  global_stats.l0_find_files_time += (end_time - start_time);
#endif
  /*RECORD_LOG("find key:%s L0:%lu\n",lkey.user_key().ToString(true).c_str(),L0files.size());
  for(unsigned int i = 0;i < L0files.size();i++){
    RECORD_LOG("L0 file:%lu first:%lu [%s-%s][%s-%s]\n",L0files[i]->fd.GetNumber(),first_key_indexs[i],findfiles[i]->keys_meta[first_key_indexs[i]].key.DebugString(true).c_str(),
      findfiles[i]->keys_meta[findfiles[i]->keys_num -1].key.DebugString(true).c_str(),L0files[i]->smallest.DebugString(true).c_str(),L0files[i]->largest.DebugString(true).c_str());
  }*/
  
  Slice user_key = lkey.user_key();
  FileEntry* file = nullptr;
  int find_index = -1;
  int pre_left = -1;
  int pre_right = -1;
  uint64_t next_file_num = 0;
  for(unsigned int i = 0;i < findfiles.size();i++){
    file = findfiles.at(i);
    if(next_file_num != file->filenum){
      pre_left = -1;
      pre_right = -1;
    }
    int com_result = UserKeyCompareRange(&user_key,&(file->keys_meta[first_key_indexs[i]].key),&(file->keys_meta[file->keys_num - 1].key));
    /*RECORD_LOG("search:%lu point:%lu pre_left:%d pre_right:%d com_result:%d\n",file->filenum,next_file_num,pre_left,pre_right,com_result);
    if(pre_left > 0 && pre_right > 0){
      RECORD_LOG("[%s-%s]\n",file->keys_meta[pre_left].key.DebugString(true).c_str(),file->keys_meta[pre_right].key.DebugString(true).c_str());
    }*/
    if( com_result == 0) { //在范围内
      if(BinarySearchInFile(file,first_key_indexs[i],&user_key,&find_index,&pre_left,&pre_right)){
        //RECORD_LOG("find!%d\n",find_index);
        GetValueInFile(file,find_index,value);
        *s=Status::OK();
        return true;
      }
      /*if(pre_left > 0 && pre_right > 0){
        RECORD_LOG("left:%d right:%d [%s-%s]\n",pre_left,pre_right,file->keys_meta[pre_left].key.DebugString(true).c_str(),file->keys_meta[pre_right].key.DebugString(true).c_str());
      }*/
      if(pre_left >= (int)first_key_indexs[i] && pre_left < (int)file->keys_num){
        pre_left = (int)file->keys_meta[pre_left].next - 1;  //在BinarySearchInFile中会有检测范围，无需担心-1带来越界
      }
      if(pre_right >= (int)first_key_indexs[i] && pre_right < (int)file->keys_num){
        pre_right = (int)file->keys_meta[pre_right].next;
      }
      next_file_num = file->key_point_filenum;
    }
    else if(com_result < 0) {  //小于该范围
      pre_left = -1;
      pre_right = (int)file->keys_meta[first_key_indexs[i]].next;  
      next_file_num = file->key_point_filenum;
    }
    else{  //大于该范围
      pre_left = (int)file->keys_meta[file->keys_num - 1].next - 1;  //在BinarySearchInFile中会有检测范围，无需担心-1带来越界
      pre_right = -1;  
      next_file_num = file->key_point_filenum;
    }
    /* if(UserKeyInRange(&user_key,&(file->keys_meta[first_key_indexs[i]].key),&(file->keys_meta[file->keys_num - 1].key))){
        if(BinarySearchInFile(file,first_key_indexs[i],&user_key,&find_index,&pre_left,&pre_right)){
          GetValueInFile(file,find_index,value);
          *s=Status::OK();
          return true;
        }
        if(pre_left >= (int)first_key_indexs[i] && pre_left < (int)file->keys_num){
          pre_left = (int)file->keys_meta[pre_left].next - 1;
        }
        if(pre_right >= (int)first_key_indexs[i] && pre_right < (int)file->keys_num){
          pre_right = (int)file->keys_meta[pre_right].next;
        }
        next_file_num = file->key_point_filenum;
    }*/
  }
  return false;

}
bool NvmCfModule::UserKeyInRange(Slice *user_key,InternalKey *start,InternalKey *end){
  auto user_comparator = icmp_->user_comparator();
  if(user_comparator->Compare(*user_key,start->user_key()) < 0 || user_comparator->Compare(*user_key,end->user_key()) > 0 ){
    return false;
  }
  return true;
}
int NvmCfModule::UserKeyCompareRange(Slice *user_key,InternalKey *start,InternalKey *end){   // user_key < start,return -1; user_key > end, return 1; start < user_key < end, return 0;
  auto user_comparator = icmp_->user_comparator();
  if(user_comparator->Compare(*user_key,start->user_key()) < 0 ) {
    return -1;
  }
  else if(user_comparator->Compare(*user_key,end->user_key()) > 0) {
    return 1;
  }
  else {
    return 0;
  }

}

bool NvmCfModule::BinarySearchInFile(FileEntry* file, int first_key_index, Slice *user_key,int *find_index,int *pre_left ,int *pre_right){
#ifdef STATISTIC_OPEN
  uint64_t start_time = get_now_micros();
#endif
  auto user_comparator = icmp_->user_comparator();
  int left = first_key_index;
  if(pre_left != nullptr && *pre_left >= first_key_index && *pre_left < (int)file->keys_num){
    left = *pre_left;
  }
  int right = file->keys_num - 1;
  if(pre_right != nullptr && *pre_right >= first_key_index && *pre_right < (int)file->keys_num){
    right = *pre_right;
  }

  int mid = 0;
  while(left <= right){  //有等号可确定跳出循环时 right < left
    mid = (left + right)/2;
    //RECORD_LOG("left:%d right:%d mid:%d\n",left,right,mid);
    int compare_result = user_comparator->Compare(file->keys_meta[mid].key.user_key(),*user_key);  //优化后字符串只比较一次
    if(compare_result > 0){
      right = mid - 1;
    }
    else if(compare_result < 0){
      left = mid + 1;
    }
    else{   //找到的情况次数最少，放在最后，减少比较次数
      *find_index = mid;
#ifdef STATISTIC_OPEN
  uint64_t end_time = get_now_micros();
  global_stats.l0_search_time += (end_time - start_time);
#endif
      return true;
    }
    //RECORD_LOG("left:%d right:%d mid:%d\n",left,right,mid);
    /* if(user_comparator->Compare(file->keys_meta[mid].key.user_key(),*user_key) == 0){   //代码优化
      *find_index = mid;
      return true;
    }
    else if(user_comparator->Compare(file->keys_meta[mid].key.user_key(),*user_key) > 0){
      right = mid - 1;
    }
    else{
      left = mid + 1;
    }*/
  }
  *pre_right = left;
  *pre_left = right;
#ifdef STATISTIC_OPEN
  uint64_t end_time = get_now_micros();
  global_stats.l0_search_time += (end_time - start_time);
#endif
  return false;

}
bool NvmCfModule::GetValueInFile(FileEntry* file,int find_index,std::string *value){
#ifdef STATISTIC_OPEN
  uint64_t start_time = get_now_micros();
#endif
  char* data_addr = GetIndexPtr(file->sstable_index);
  uint64_t key_value_offset = file->keys_meta[find_index].offset;
  uint64_t key_size = DecodeFixed64(data_addr + key_value_offset);
  key_value_offset += 8;
  key_value_offset += key_size;
  uint64_t value_size = DecodeFixed64(data_addr + key_value_offset);
  key_value_offset += 8;
  value->assign(data_addr + key_value_offset,value_size);
#ifdef STATISTIC_OPEN
  uint64_t end_time = get_now_micros();
  global_stats.l0_read_time += (end_time - start_time);
#endif
  return true;
}
void NvmCfModule::AddIterators(VersionStorageInfo* vstorage,MergeIteratorBuilder* merge_iter_builder){
  auto L0files = vstorage->LevelFiles(0);
  std::vector<FileEntry*> findfiles;
  std::vector<uint64_t> first_key_indexs;
  
  FileEntry* tmp = nullptr;
  for(unsigned int i = 0;i < L0files.size();i++){
    tmp = sst_meta_->FindFile(L0files.at(i)->fd.GetNumber());
    findfiles.push_back(tmp);
    first_key_indexs.push_back(L0files.at(i)->first_key_index);
  }
  FileEntry* file = nullptr;
  uint64_t key_num = 0;
  for(unsigned int i = 0;i < findfiles.size();i++){
    file = findfiles.at(i);
    key_num = file->keys_num - first_key_indexs[i];
    merge_iter_builder->AddIterator(NewColumnCompactionItemIterator(GetIndexPtr(file->sstable_index),file,first_key_indexs[i],key_num));
  }


}

NvmCfModule* NewNvmCfModule(NvmCfOptions* nvmcfoption,const std::string &cf_name,uint32_t cf_id,const InternalKeyComparator* icmp){
  return new NvmCfModule(nvmcfoption,cf_name,cf_id,icmp);
}


}  // namespace rocksdb