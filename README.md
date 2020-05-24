# MatrixKV
## 1 Introduction
MatrixKV is a key-value storage database optimized for [Rocksdb](https://github.com/facebook/rocksdb) based on NVM(AEP).


## 2 Compilation and Run
### 2.1 Tools
MatrixKV uses [PMDK](https://github.com/pmem/pmdk) as read and write interface using NVM, and PMDK must be installed.


### 2.2 Compilation
```
> make -j64   
```
Only Makefile is currently supported, and cmake is not implemented.

### 2.3 Run
If you want to use MatrixKV, you need to modify the configuration, in ``include/rocksdb/option.h``.
```
std::shared_ptr<NvmSetup> nvm_setup = nullptr;
```
To learn more about ``NvmSetup``, you can view the file ``include/rocksdb/nvm_option.h``.


Run db_bench can refer to the test script file ``test_sh/matrixkv_test_4value.sh ``, you can :

```
> ./tesh_sh/matrixkv_test_4value.sh
> nohup ./tesh_sh/matrixkv_test_4value.sh >out.out 2>&1 &     ##Run in the background
```

## 3 Implement
We implement MatrixKV based on the Rocksdb v5.18.3 version code. If you want to view the rocksdb code:
```
> git checkout 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253
```
If you want to see what parts of the code have been modified:
```
> git reset --mixed 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253
```
It is recommended to use vscode to view.
## 4 Acknowledgement
Thanks to Intel and PingCAP for their help and support!
## 5 Contributors
- Ph.D supervisor: Jiguang Wan  (jgwan@hust.edu.cn)
- Ting Yao (tingyao@hust.edu.cn)
- Zhiwen Liu (993096281@qq.com)
- Yiwen Zhang (zhangyiwen@hust.edu.cn)
