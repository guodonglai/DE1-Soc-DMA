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


int main(int argc, char *argv[]){
	int dma_fd,csr_fd,clr_fd, i;
	dma_fd = open("/sys/kernel/debug/fpga_dma/dma", O_RDWR);
	csr_fd = open("/sys/kernel/debug/fpga_dma/csr", O_RDWR);
	clr_fd = open("/sys/kernel/debug/fpga_dma/clear", O_RDWR);
	if(dma_fd < 1){
		printf("Unable to open fpga-dma dma debug file");
		return -1;
	}
	unsigned int write_buf[1024];
	unsigned int read_buf[1024];
	for(i = 0; i < 1024; i++){
		write_buf[i] = i;
	}
	size_t count = 1024 * 4;
	loff_t *ppos = 0;
	
	write(clr_fd, write_buf, count, ppos);
	write(dma_fd, write_buf, count, ppos);
	int readcount = read(dma_fd, read_buf, count, ppos);
	for(i = 0; i < 1024; i++){
		if(write_buf[i] != read_buf[i]){
			printf("mismatch: read_buf[%d] = %d\n", i, read_buf[i]);
		}
	}
  	printf("tx and rx buf are same\n");
	printf("read %d\n", readcount);
//	read(csr_fd, write_buf, count, ppos);
	return 0;
}
