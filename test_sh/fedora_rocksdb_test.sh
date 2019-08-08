#! /bin/sh

db="/home/lzw/ceshi"
#bench_level0_file_path="/pmem/ceshi"
level0_file_path=""
value_size="4096"
compression_type="none" #"snappy,none"

#bench_benchmarks="fillseq,stats,readseq,readrandom,stats" #"fillrandom,fillseq,readseq,readrandom,stats"
#bench_benchmarks="fillrandom,stats,readseq,readrandom,readrandom,readrandom,stats"
#bench_benchmarks="fillrandom,stats,wait,stats,readseq,readrandom,readrandom,readrandom,stats"
#bench_benchmarks="fillrandom,stats,wait,clean_cache,stats,readseq,readrandom,readrandom,readrandom,stats"
benchmarks="fillrandomcontrolrequest,stats"
#benchmarks="fillrandom,stats"
num="50000"
#reads="100"
#bench_max_open_files="1000"
max_background_jobs="2"
#max_bytes_for_level_base="`expr 8 \* 1024 \* 1024 \* 1024`"   #8G
max_bytes_for_level_base="`expr 256 \* 1024 \* 1024`" 

#perf_level="1"

#report_write_latency="true"

#stats_interval="100"
#stats_interval_seconds="10"
histogram="true"

threads="4"

request_rate_limit="18000"


const_params=""

function FILL_PATAMS() {
    if [ -n "$db" ];then
        const_params=$const_params"--db=$db "
    fi

    if [ -n "$level0_file_path" ];then
        const_params=$const_params"--level0_file_path=$level0_file_path "
    fi

    if [ -n "$value_size" ];then
        const_params=$const_params"--value_size=$value_size "
    fi

    if [ -n "$compression_type" ];then
        const_params=$const_params"--compression_type=$compression_type "
    fi

    if [ -n "$benchmarks" ];then
        const_params=$const_params"--benchmarks=$benchmarks "
    fi

    if [ -n "$num" ];then
        const_params=$const_params"--num=$num "
    fi

    if [ -n "$reads" ];then
        const_params=$const_params"--reads=$reads "
    fi

    if [ -n "$max_background_jobs" ];then
        const_params=$const_params"--max_background_jobs=$max_background_jobs "
    fi

    if [ -n "$max_bytes_for_level_base" ];then
        const_params=$const_params"--max_bytes_for_level_base=$max_bytes_for_level_base "
    fi

    if [ -n "$perf_level" ];then
        const_params=$const_params"--perf_level=$perf_level "
    fi

    if [ -n "$report_write_latency" ];then
        const_params=$const_params"--report_write_latency=$report_write_latency "
    fi

    if [ -n "$threads" ];then
        const_params=$const_params"--threads=$threads "
    fi

    if [ -n "$stats_interval" ];then
        const_params=$const_params"--stats_interval=$stats_interval "
    fi

    if [ -n "$stats_interval_seconds" ];then
        const_params=$const_params"--stats_interval_seconds=$stats_interval_seconds "
    fi

    if [ -n "$histogram" ];then
        const_params=$const_params"--histogram=$histogram "
    fi

    if [ -n "$benchmark_write_rate_limit" ];then
        const_params=$const_params"--benchmark_write_rate_limit=$benchmark_write_rate_limit "
    fi

    if [ -n "$request_rate_limit" ];then
        const_params=$const_params"--request_rate_limit=$request_rate_limit "
    fi

}


bench_file_path="$(dirname $PWD )/db_bench"

if [ ! -f "${bench_file_path}" ];then
bench_file_path="$PWD/db_bench"
fi

if [ ! -f "${bench_file_path}" ];then
echo "Error:${bench_file_path} or $(dirname $PWD )/db_bench not find!"
exit 1
fi

FILL_PATAMS 

cmd="$bench_file_path $const_params "

if [ -n "$1" ];then
cmd="nohup $bench_file_path $const_params >>out.out 2>&1 &"
echo $cmd >out.out
fi

echo $cmd
eval $cmd
