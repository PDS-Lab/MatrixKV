#include "sstable_meta.h"

#include "db/dbformat.h"

namespace rocksdb {


SstableMetadata::SstableMetadata(pool_base& pop,const InternalKeyComparator* icmp,int level0_stop_writes_trigger)
    :pop_(pop),level0_stop_writes_trigger_(level0_stop_writes_trigger),icmp_(icmp){
    new_file_num = 0;

    RECORD_LOG("creat SstableMetadata level0_stop_writes_trigger:%d\n",level0_stop_writes_trigger);

}

SstableMetadata::~SstableMetadata(){

}

persistent_ptr<FileEntry> SstableMetadata::AddFile(uint64_t filenumber,int index){
    persistent_ptr<FileEntry> tmp;
    transaction::run(pop_, [&] {
			tmp= make_persistent<FileEntry>(filenumber,index);

			if (head == nullptr && tail == nullptr) {
				head = tail = tmp;
			} else {
                tmp->next = head;
				head->prev = tmp;
				head = tmp;
			}
	});
    new_file_num = new_file_num + 1;
    return tmp;
}
persistent_ptr<FileEntry> SstableMetadata::FindFile(uint64_t filenumber,bool forward,bool have_error_print){
    persistent_ptr<FileEntry> tmp=nullptr;
    if(forward){
        for(tmp = head;tmp != nullptr;tmp = tmp->next){
            if(tmp->filenum == filenumber) return tmp;
        }
    }
    else{
        for(tmp = tail;tmp != nullptr;tmp = tmp->prev){
            if(tmp->filenum == filenumber) return tmp;
        }

    }
    if(have_error_print) printf("no find FileEntry:%lu\n",filenumber);
    return tmp;
    
}
bool SstableMetadata::DeteleFile(uint64_t filenumber){

    persistent_ptr<FileEntry> tmp = FindFile(filenumber,false);
    if(tmp == nullptr){
        return false;
    }
    if(tmp == head && tmp == tail){
        head = nullptr;
        tail = nullptr;
    }
    else if(tmp == head){
        head = tmp->next;
        head->prev = nullptr;
    }
    else if(tmp == tail){
        tail = tmp->prev;
        tail->next = nullptr;
    }
    else{
        tmp->prev->next = tmp->next;
        tmp->next->prev = tmp->prev; 
    }
    transaction::run(pop_, [&] {
        delete_persistent<FileEntry>(tmp);
    });
    new_file_num = new_file_num - 1;
    return true;
}
/*persistent_ptr<FileEntry> SstableMetadata::ImmuFindFile(uint64_t filenumber,bool forward){
    persistent_ptr<FileEntry> tmp=nullptr;
    if(forward){
        for(tmp = immu_head;tmp != nullptr;tmp = tmp->next){
            if(tmp->filenum == filenumber) return tmp;
        }
    }
    else{
        for(tmp = immu_tail;tmp != nullptr;tmp = tmp->prev){
            if(tmp->filenum == filenumber) return tmp;
        }

    }
    return tmp;
}
bool SstableMetadata::ImmuDeteleFile(uint64_t filenumber){
    persistent_ptr<FileEntry> tmp = ImmuFindFile(filenumber,0);
    if(tmp == nullptr){
        return false;
    }
    if(tmp == immu_head && tmp == immu_tail){
        immu_head = nullptr;
        immu_tail = nullptr;
    }
    else if(tmp == immu_head){
        immu_head = tmp->next;
        immu_head->prev = nullptr;
    }
    else if(tmp == immu_tail){
        immu_tail = tmp->prev;
        immu_tail->next = nullptr;
    }
    else{
        tmp->prev->next = tmp->next;
        tmp->next->prev = tmp->prev; 
    }
    
    return true;

}*/
void SstableMetadata::UpdateKeyNext(persistent_ptr<FileEntry> &file){
    if(file->next == nullptr || file->keys_num == 0){
        return ;
    }
    struct KeysMetadata* new_keys = file->keys_meta;
    struct KeysMetadata* old_keys = file->next->keys_meta;
    uint64_t new_key_num = file->keys_num;
    uint64_t old_key_num = file->next->keys_num;
    int32_t new_index = 0;
    int32_t old_index = 0;
    while((uint64_t)new_index < new_key_num && (uint64_t)old_index < old_key_num){
        
        if(icmp_->Compare(old_keys[old_index].key,new_keys[new_index].key) >= 0){   //相同user key，新插入的会比旧的小
            new_keys[new_index].next=old_index;
            new_index++;
        }
        else{
            old_index++;
        }
    }
    file->key_point_filenum = file->next->filenum;
}
void SstableMetadata::UpdateCompactionState(std::vector<FileMetaData*>& L0files){
    if (!compaction_files.empty()){   //不为空,则检测是否一致
        int comfile = compaction_files.size() - 1;
        int L0file = L0files.size() - 1;
        bool consistency = true;
        for(;comfile >= 0;) {
            if(compaction_files[comfile] == L0files[L0file]->fd.GetNumber()) {
                comfile--;
                L0file--;
            }
            else { //不一致
                consistency = false;
                break;
            }
        }
        if (consistency) {  //一致则继续
            return;
        }
        else {  //不一致，清空compaction_files，重选
            RECORD_LOG("UpdateCompactionState warn:L0:[");
            for(unsigned int i = 0; i < L0files.size(); i++){
                RECORD_LOG("%ld ",L0files[i]->fd.GetNumber());
            }
            RECORD_LOG("] compaction_files:[");
            for(unsigned int i = 0; i < compaction_files.size(); i++){
                RECORD_LOG("%ld ",compaction_files[i]);
            }
            RECORD_LOG("]\n");

            compaction_files.clear();
        }
        
    }
    if (L0files.size() < Level0_column_compaction_trigger) {
        RECORD_LOG("warn:L0 size:%d < Level0_column_compaction_trigger:%ld\n",L0files.size(), Level0_column_compaction_trigger);
    }
    //int level0_stop_writes_trigger = level0_stop_writes_trigger_;
    int file_num = L0files.size() - 1;
    for(;file_num >= 0; file_num--){  //目前所有table加入compaction_files，后面可设置数量
        compaction_files.insert(compaction_files.begin(),L0files[file_num]->fd.GetNumber());
        if (compaction_files.size() >= (uint64_t)Max_Level0_column_compaction_file ) { //最大加入的column compaction的文件，可调整
            break;
        }
    }

    RECORD_LOG("UpdateCompactionState:L0:%ld[",L0files.size());
    for(unsigned int i=0; i < L0files.size(); i++){
        RECORD_LOG("%ld ",L0files[i]->fd.GetNumber());
    }
    RECORD_LOG("]\n");
    
    RECORD_LOG("UpdateCompactionState:select:%ld[",compaction_files.size());
    for(unsigned int i=0; i < compaction_files.size(); i++){
        RECORD_LOG("%ld ",compaction_files[i]);
    }
    RECORD_LOG("]\n");
}
void SstableMetadata::DeleteCompactionFile(uint64_t filenumber){
    std::vector<uint64_t>::iterator it = compaction_files.begin();
    for(; it != compaction_files.end(); it++) {
        if((*it) == filenumber) {
            compaction_files.erase(it);
            return;
        }
    }
    RECORD_LOG("warn: no delete compaction file:%ld\n",filenumber);
}

}