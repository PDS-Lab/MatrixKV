

#include "common.h"

namespace rocksdb {
int nvm_file_exists(const char* file) { return access(file, F_OK); }

void nvm_dir_no_exists_and_creat(const std::string& name) {
  if (nvm_file_exists(name.c_str()) != 0) {
    if (mkdir(name.c_str(), 0755) != 0) {
      printf("creat file:%s failed!\n", name.c_str());
    }
  }
}

}  // namespace rocksdb