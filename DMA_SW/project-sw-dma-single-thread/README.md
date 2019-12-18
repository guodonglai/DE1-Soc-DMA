DMA transfer using dma_single. tx and rx at same time using pthread. Seprate the tx and rx part into different thread. Transmit and recieve 2048 bytes data at same time. When using thread to test the dma, using 
gcc -pthread -o test fpga-dma-test.c 
instead of 
gcc -o test fpga-dma-test.c
