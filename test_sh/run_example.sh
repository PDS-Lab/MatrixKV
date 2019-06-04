#! /bin/sh


bench_db_path="/home/hm/ceshi"
bench_value="4096"
bench_compression="snappy" #"snappy,none"

#bench_benchmarks="fillseq,stats,readseq,readrandom,stats" #"fillrandom,fillseq,readseq,readrandom,stats"
bench_benchmarks="fillrandom,stats,readseq,readrandom,stats"
#bench_benchmarks="fillseq,stats"
bench_num="2000000"
bench_readnum="1000000"
bench_sync="0"
bench_direct="0"
bench_statistics="0"
bench_max_open_files="1000"

pmem_path="/pmem/nvm"


const_params="
    --db=$bench_db_path \
    --value_size=$bench_value \
    --benchmarks=$bench_benchmarks \
    --num=$bench_num \
    --reads=$bench_readnum \
    --sync=$bench_sync \
    --use_direct_io_for_flush_and_compaction=$bench_direct \
    --compression_type=$bench_compression \
    --statistics=$bench_statistics \
    --open_files=$bench_max_open_files \
    --use_nvm_module=true \
    --reset_nvm_storage=true \
    --pmem_path=$pmem_path \
    --pmem_size=`expr 2 \* 1024 \* 1024 \* 1024`
    "

bench_file_path="$(dirname $PWD )/db_bench"

if [ ! -f "${bench_file_path}" ];then
bench_file_path="$PWD/db_bench"
fi

if [ ! -f "${bench_file_path}" ];then
echo "Error:${bench_file_path} or $(dirname $PWD )/db_bench not find!"
exit 1
fi

cmd="$bench_file_path $const_params "

if [ -n "$1" ];then
cmd="nohup $bench_file_path $const_params >out.out 2>&1 &"
fi

echo $cmd
eval $cmd
