#! /bin/sh

bench_db_path="/home/czl/pmem0/ceshi"
bench_value="4096"
bench_compression="none" #"snappy,none"

#bench_benchmarks="fillseq,stats,readseq,readrandom,stats" #"fillrandom,fillseq,readseq,readrandom,stats"
bench_benchmarks="fillrandom,stats,readseq,readrandom,stats"
#bench_benchmarks="fillseq,stats"
bench_num="2000000"
bench_readnum="100000"
bench_max_open_files="1000"
max_background_jobs="3"
max_bytes_for_level_base="`expr 8 \* 1024 \* 1024 \* 1024`"   #8G
#max_bytes_for_level_base="`expr 256 \* 1024 \* 1024`" 
level0_file_num_compaction_trigger="128"   #8G
level0_slowdown_writes_trigger="144"       #9G
level0_stop_writes_trigger="160"           #10G

const_params="
    --db=$bench_db_path \
    --value_size=$bench_value \
    --benchmarks=$bench_benchmarks \
    --num=$bench_num \
    --reads=$bench_readnum \
    --compression_type=$bench_compression \
    --open_files=$bench_max_open_files \
    --max_background_jobs=$max_background_jobs \
    --max_bytes_for_level_base=$max_bytes_for_level_base \
    --level0_file_num_compaction_trigger=$level0_file_num_compaction_trigger \
    --level0_slowdown_writes_trigger=$level0_slowdown_writes_trigger \
    --level0_stop_writes_trigger=$level0_stop_writes_trigger \
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
