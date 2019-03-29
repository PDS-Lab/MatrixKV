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

persistent_ptr<FileEntry> SstableMetadata::AddFile(uint64_t filenumber,int index,uint64_t oft){
    persistent_ptr<FileEntry> tmp;
    transaction::run(pop_, [&] {
			tmp= make_persistent<FileEntry>(filenumber,index,oft);

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
persistent_ptr<FileEntry> SstableMetadata::FindFile(uint64_t filenumber,bool forward){
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
    tmp = ImmuFindFile(filenumber,forward);
    if(tmp != nullptr){
        return tmp;
    }
    else{
        printf("no find FileEntry:%lu\n",filenumber);
        return tmp;
    }
}
bool SstableMetadata::DeteleFile(uint64_t filenumber){
    if(ImmuDeteleFile(filenumber)){
        return true;
    }
    persistent_ptr<FileEntry> tmp = FindFile(filenumber,0);
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
persistent_ptr<FileEntry> SstableMetadata::ImmuFindFile(uint64_t filenumber,bool forward){
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
    transaction::run(pop_, [&] {
        delete_persistent<FileEntry>(tmp);
    });
    return true;

}
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
void SstableMetadata::UpdateCompactionState(int num){
    if(immu_head != nullptr || immu_tail != nullptr){
        return;
    }
    if(new_file_num == (uint64_t)num){
    //if(new_file_num >= (uint64_t)(level0_stop_writes_trigger_/2)){
        RECORD_LOG("UpdateCompactionState all:%d\n",num);
        transaction::run(pop_, [&] {
            immu_head = head;
            immu_tail = tail;
            head = nullptr;
            tail = nullptr;
            new_file_num = 0;
        });

    }
    else{
        persistent_ptr<FileEntry> tmp = tail;
        int i = num;
        while(i > 1){
            tmp = tmp->prev;
            i--;
        }
        RECORD_LOG("UpdateCompactionState new_file_num:%lu num:%d\n",new_file_num,num);
        transaction::run(pop_, [&] {
            immu_tail = tail;
            tail = tmp->prev;
            tail->next = nullptr;
            immu_head = tmp;
            immu_head->prev = nullptr;
            new_file_num = new_file_num - num;
        });

    }
}
uint64_t SstableMetadata::GetImmuFileEntryNum(){
    uint64_t file_num = 0;
    persistent_ptr<FileEntry> tmp = nullptr;
    for(tmp = immu_head;tmp != nullptr;tmp = tmp->next){
            file_num++;
    }
    return file_num;

}

}