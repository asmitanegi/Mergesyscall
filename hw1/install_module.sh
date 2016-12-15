dmesg -c
make clean
make
rmmod sys_xmergesort
insmod sys_xmergesort.ko
./xhw1 -u test_file1 test_file2 writefile 
