COMP 3370 Assignment 2 Part 2 Test Results
Name: Al-amin Lawal
Student ID: 7833358
Instructor: Franklin Bristow
==================================================================================================================

Hit and Miss Rate Results for "test1.asm" with different number of blocks and block sizes in table format
===================================================================================================
| NumBlocks | Block Size | Hits | Misses | Hit Ratio(%) |
| :-------- | :--------- | :--- | :----- | :----------- |
| 8         | 1          | 1    | 7      | 12.50%       |
| 4         | 2          | 4    | 4      | 50.00%       |
| 2         | 4          | 6    | 2      | 75.00%       |
| 1         | 8          | 7    | 1      | 87.50%       |
| 10        | 8          | 7    | 1      | 87.50%       |
| 20        | 8          | 7    | 1      | 87.50%       |
| 40        | 8          | 7    | 1      | 87.50%       |
| 60        | 8          | 7    | 1      | 87.50%       |
| 80        | 8          | 7    | 1      | 87.50%       |
| 100       | 4          | 6    | 2      | 75.00%       |
| 90        | 4          | 6    | 2      | 75.00%       |
| 100       | 2          | 4    | 4      | 50.00%       |
| 50        | 2          | 4    | 4      | 50.00%       |
| 20        | 2          | 4    | 4      | 50.00%       |
| 80        | 1          | 1    | 7      | 12.50%       |
| 50        | 1          | 1    | 7      | 12.50%       |
| 35        | 1          | 1    | 7      | 12.50%       |
| 40        | 1          | 7    | 7      | 12.50%       |


1 block and a block size of 8 is the optimal cache settings for test1.asm


It is clear that the program(test1.asm) hits the same address in memory.
Therefore, the only miss that occurs with a block size of 8 is a COMPULSORY miss which occurs the first time that specific block in
the cache is accessed; As we know, we can not do anything about COMPULSORY misses.



Hit and Miss Rate Results for "test2.asm" with different number of blocks and block sizes in table format
==============================================================================================

| NumBlocks | Block Size | Hits | Misses | Hit Ratio(%) |
| :-------- | :--------- | :--- | :----- | :----------- |
| 8         | 1          | 523  | 511    | 50.58%       |
| 4         | 2          | 779  | 255    | 75.34%       |
| 2         | 4          | 843  | 191    | 81.53%       |
| 1         | 8          | 491  | 543    | 47.49%       |
| 105       | 8          | 491  | 543    | 47.49%       |
| 100       | 8          | 1001 | 33     | 96.81%       |
| 90        | 8          | 1001 | 33     | 96.81%       |
| 80        | 8          | 1001 | 33     | 96.81%       |
| 35        | 8          | 1001 | 33     | 96.81%       |
| 33        | 8          | 1001 | 33     | 96.81%       |
| 32        | 8          | 1000 | 34     | 96.71%       |
| 20        | 8          | 988  | 46     | 95.55%       |
| 10        | 8          | 978  | 56     | 94.58%       |
| 8         | 8          | 976  | 58     | 94.39%       |
| 4         | 8          | 972  | 62     | 94.00%       |
| 100       | 4          | 969  | 65     | 93.71%       |
| 50        | 4          | 954  | 80     | 92.26%       |
| 25        | 4          | 929  | 105    | 89.85%       |
| 8         | 4          | 912  | 122    | 88.20%       |
| 100       | 2          | 969  | 65     | 84.62%       |
| 1         | 2          | 390  | 644    | 37.72%       |
| 2         | 2          | 649  | 385    | 62.77%       |
| 100       | 1          | 615  | 419    | 59.48%       |
| 1         | 1          | 260  | 77     | 25.15        |


Test2.asm hits addresses in memory at a very frequent rate; We can see from the table that the optimal cache settings 
is 33 blocks and a block size of 8; There are 33 Misses, and 1001 hits. We can see that even 100 blocks
and a block size of 8, we still have 33 misses and we can also see that with 32 blocks and a block size of 8,
we now have 34 misses. This tells me that multiple addresses are being hit frequently and thus, 
we have 33 COMPULSORY misses given the optimal cache settings of 33 blocks and a block size of 8





