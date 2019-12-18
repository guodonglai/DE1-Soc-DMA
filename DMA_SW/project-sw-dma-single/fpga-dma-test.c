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
	int dma_fd,csr_fd,clr_fd, i;
	dma_fd = open("/sys/kernel/debug/fpga_dma/dma", O_RDWR);
	csr_fd = open("/sys/kernel/debug/fpga_dma/csr", O_RDWR);
	clr_fd = open("/sys/kernel/debug/fpga_dma/clear", O_RDWR);
	if(dma_fd < 1){
		printf("Unable to open fpga-dma dma debug file");
		return -1;
	}
	int numofwords = 2048;
	unsigned int write_buf[numofwords];
	unsigned int read_buf[numofwords];
	for(i = 0; i < numofwords; i++){
		write_buf[i] = i;
	}
	size_t count = numofwords * 4;
	loff_t *ppos = 0;
	
	write(clr_fd, write_buf, count, ppos);
	struct timeval t1, t2;
	double elapsedTime;
	gettimeofday(&t1, NULL);
	write(dma_fd, write_buf, count, ppos);
	gettimeofday(&t2, NULL);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000.0;
	elapsedTime += (t2.tv_usec - t1.tv_usec);
	printf("DMA write single T=%.0f uSec  n/sec=%2.3e\n\r", elapsedTime,numofwords * 4 *1e6/elapsedTime);        
	
	gettimeofday(&t1, NULL);
	int readcount = read(dma_fd, read_buf, count, ppos);
	gettimeofday(&t2, NULL);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000.0;
	elapsedTime += (t2.tv_usec - t1.tv_usec);
	printf("DMA read single T=%.0f uSec  n/sec=%2.3e\n\r", elapsedTime, numofwords * 4 *1e6/elapsedTime);
	for(i = 0; i < numofwords; i++){
		if(write_buf[i] != read_buf[i]){
			printf("mismatch: read_buf[%d] = %d\n", i, read_buf[i]);
		}
	}

	printf("read %d\n", readcount);
	return 0;
}
