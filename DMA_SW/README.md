To compile code inside each folder:
run make first and then gcc -o exec_name fpga_dma_test.c which compiles the test code.
After that, run insmod fpga_dma.ko and ./test to see the result.
