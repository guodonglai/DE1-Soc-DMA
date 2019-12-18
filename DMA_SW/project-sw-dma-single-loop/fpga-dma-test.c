/* DMA Proxy Test Application
 *
 * This application is intended to be used with the DMA Proxy device driver. It provides
 * an example application showing how to use the device driver to do user space DMA
 * operations.
 *
 * It has been tested with an AXI DMA system with transmit looped back to receive.
 * The device driver implements a blocking ioctl() function such that a thread is
 * needed for the 2nd channel. Since the AXI DMA transmit is a stream without any
 * buffering it is throttled until the receive channel is running.
 */

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
int main(int argc, char *argv[]){
	struct timeval t1, t2;
	double elapsedTime;
	int dma_fd,csr_fd,clr_fd, i,j;
	dma_fd = open("/sys/kernel/debug/fpga_dma/dma", O_RDWR);
	csr_fd = open("/sys/kernel/debug/fpga_dma/csr", O_RDWR);
	clr_fd = open("/sys/kernel/debug/fpga_dma/clear", O_RDWR);
	if(dma_fd < 1){
		printf("Unable to open fpga-dma dma debug file");
		return -1;
	}
	int numofwords = 2048;
	int numoftx = 50000;
	printf("start write buf\n");
	int *write_buf, *read_buf;
	write_buf = (int *)malloc(numoftx * numofwords * sizeof(int));
	read_buf = (int *)malloc(numoftx * numofwords * sizeof(int));
	printf("finishe write buf\n");
	for(i = 0; i < numoftx; i++){
	  	for(j = 0;j<numofwords;j++){
			*(write_buf + i * numofwords + j) = i * numofwords + j;
		}
	}
	printf("write_buf_done\n");
	size_t count = numofwords * 4;
	loff_t *ppos = 0;
	gettimeofday(&t1, NULL);
        for(i = 0; i < numoftx;i++){
            	write(dma_fd, (write_buf + i * 2048), numofwords*4, ppos);
        	int readcount = read(dma_fd, (read_buf + i * 2048), numofwords*4, ppos);
        }
	gettimeofday(&t2, NULL);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000.0;
	elapsedTime += (t2.tv_usec - t1.tv_usec);
	printf("DMA write and read T=%.0f uSec  n/sec=%2.3e\n\r", elapsedTime, numoftx * numofwords * 4 *1e6/elapsedTime);
	for(i = 0; i < numoftx;i++){
		for(j = 0; j < numofwords; j++){
			if(*(write_buf + i * numofwords + j) != *(read_buf + i * numofwords + j)){
				printf("mismatch: read_buf[%d][%d] = %d,write_buf = %d\n", i,j, *(read_buf + i * numofwords + j),*(write_buf + i * numofwords + j));	
			}
		}
	}

  	printf("tx and rx buf are same\n");

	free(write_buf);
	free(read_buf);
	return 0;
}
