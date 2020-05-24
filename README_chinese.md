# MatrixKV
## 1 介绍
MatrixKV是基于NVM(AEP)优化[Rocksdb](https://github.com/facebook/rocksdb)的一个键值存储数据库。

## 2 编译与运行
### 2.1 工具
MatrixKV 使用了[PMDK](https://github.com/pmem/pmdk)当作使用NVM的读写接口，必须安装PMDK。

### 2.2 编译
```
> make -j64   
```
暂时只支持Makefile，没有实现cmake。

### 2.3 运行
如果要使用MatrixKV，需要修改配置，在``include/rocksdb/option.h``
```
std::shared_ptr<NvmSetup> nvm_setup = nullptr;
```
想了解更多关于``NvmSetup``,可以查看文件``include/rocksdb/nvm_option.h``

我们主要使用db_bench和ycsb进行测试，如果使用其它的测试方法，需要自行增加接口。

运行db_bench可以参考测试脚本文件``test_sh/matrixkv_test_4value.sh ``,运行测试可以：
```
> ./tesh_sh/matrixkv_test_4value.sh
> nohup ./tesh_sh/matrixkv_test_4value.sh >out.out 2>&1 &     ##后台运行
```

## 3 Rocksdb代码
我们基于Rocksdb v5.18.3版本代码进行实现MatrixKV，如果想查看rocksdb代码：
```
> git checkout 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253
```
如果想查看代码修改了哪些部分：
```
> git reset --mixed 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253
```
推荐使用vscode查看。
## 4 致谢
感谢Intel公司和PingCAP公司的帮助和支持！
## 5 贡献者
- 导师: 万继光 (jgwan@hust.edu.cn)
- 姚婷 (tingyao@hust.edu.cn)
- 刘志文 (993096281@qq.com)
- 张艺文 (zhangyiwen@hust.edu.cn)
