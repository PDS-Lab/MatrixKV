
#include "my_mutex.h"
namespace rocksdb {

static void PthreadCall(const char* label, int result) {
  if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    abort();
  }
}

MyMutex::MyMutex() { PthreadCall("init mutex", pthread_mutex_init(&mu_, NULL)); }

MyMutex::~MyMutex() { PthreadCall("destroy mutex", pthread_mutex_destroy(&mu_)); }

void MyMutex::Lock() { PthreadCall("lock", pthread_mutex_lock(&mu_)); }

void MyMutex::Unlock() { PthreadCall("unlock", pthread_mutex_unlock(&mu_)); }











}