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

iquery -o csv:l -aq "pull(zero_to_255)" >> test.out

#iquery -o csv:l -aq "aggregate(filter(summarize(between(zero_to_255,0,9)), attid=0), sum(count) as count)" >> test.out
#iquery -o csv:l -aq "aggregate(filter(summarize(zero_to_255_overlap), attid=0), sum(count) as count)" >> test.out
#iquery -o csv:l -aq "summarize(temp)" >> test.out
#iquery -o csv:l -aq "summarize(temp, 'per_attribute=1')" >> test.out
#diff test.out test.expected
exit 0

