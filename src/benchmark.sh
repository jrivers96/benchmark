#!/bin/bash

iquery -anq "remove(foo)" > /dev/null 2>&1
iquery -naq "store(build(<val:double>[i=0:3,4,0,j=0:3,4,0],i*4+j),foo)" >/dev/null 2>&1
iquery -anq "remove(zero_to_255)"                > /dev/null 2>&1
iquery -anq "store( build( <val:string> [x=0:255,10,0],  string(x % 256) ), zero_to_255 )"  > /dev/null 2>&1
iquery -anq "remove(zero_to_255_overlap)" > /dev/null 2>&1
iquery -anq "store( build( <val:string> [x=0:255,10,5],  string(x % 256) ), zero_to_255_overlap )"  > /dev/null 2>&1
iquery -anq "remove(temp)" > /dev/null 2>&1
iquery -naq "store(apply(build(<a:double> [x=1:10000000,1000000,0], double(x)), b, iif(x%2=0, 'abc','def'), c, int64(0)), temp)" > /dev/null 2>&1

rm test.out
rm test.expected
touch ./test.expected

iquery -o csv:l -aq "aggregate(filter(summarize(zero_to_255), attid=0), sum(count) as count)" >> test.out
echo 'count' >> ./test.expected
echo '256' >> ./test.expected

iquery -o csv:l -aq "aggregate(filter(summarize(between(zero_to_255,0,9)), attid=0), sum(count) as count)" >> test.out
echo 'count' >> ./test.expected
echo '10' >> ./test.expected

iquery -o csv:l -aq "aggregate(filter(summarize(zero_to_255_overlap), attid=0), sum(count) as count)" >> test.out
echo 'count' >> ./test.expected
echo '256' >> ./test.expected

iquery -o csv:l -aq "summarize(temp)" >> test.out
echo 'att,count,bytes,chunks,min_count,avg_count,max_count,min_bytes,avg_bytes,max_bytes' >> ./test.expected
echo "'all',10000000,170002720,40,1000000,1e+06,1000000,48,4.25007e+06,9000072" >> ./test.expected

iquery -o csv:l -aq "summarize(temp, 'per_attribute=1')" >> test.out
echo "att,count,bytes,chunks,min_count,avg_count,max_count,min_bytes,avg_bytes,max_bytes" >> ./test.expected
echo "'a',10000000,80000720,10,1000000,1e+06,1000000,8000072,8.00007e+06,8000072" >> ./test.expected
echo "'b',10000000,90000720,10,1000000,1e+06,1000000,9000072,9.00007e+06,9000072" >> ./test.expected
echo "'c',10000000,800,10,1000000,1e+06,1000000,80,80,80" >> ./test.expected
echo "'EmptyTag',10000000,480,10,1000000,1e+06,1000000,48,48,48" >> ./test.expected

diff test.out test.expected
exit 0

