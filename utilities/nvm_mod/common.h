//
//
//

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>

#include <libpmem.h>

#include "libpmemobj++/make_persistent.hpp"
#include "libpmemobj++/make_persistent_array.hpp"
#include "libpmemobj++/p.hpp"
#include "libpmemobj++/persistent_ptr.hpp"
#include "libpmemobj++/pool.hpp"
#include "libpmemobj++/transaction.hpp"


namespace rocksdb {

#define CREATE_MODE_RW (S_IWUSR | S_IRUSR)

extern int nvm_file_exists(char const* file) ;

extern void nvm_dir_no_exists_and_creat(const std::string& name);

}  // namespace rocksdb