default:t7
t2:
	g++ libpmemlog_test.cc -o t2 -lpmemlog -lpmem -pthread

t3:
	g++ libpmemlog_get_size.cc -o t3 -lpmemlog -lpmem -pthread

t4:
	g++ file_test.cc -o t4 

t5:
	g++ obj_test.cc -o t5 -lpmemobj -lpmem -pthread

t6:
	g++ obj_rw.cc -o t6 -lpmemobj -lpmem -pthread
t7:
	g++ objtest.cc -o t7 -lpmemobj -lpmem -pthread -std=c++11
t8:
	g++ libpmem_test.cc -o t8 -lpmem -pthread

clean:
	rm -f test
	rm -f t2
	rm -f t3
	rm -f t4
	rm -f t5
	rm -f t6
	rm -f t7
	rm -f t8