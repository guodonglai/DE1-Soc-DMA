DMA transfer using dma_single mutiple times to handle large data
By far, we have tested 50000 x 2048 words of data and the data rate is approximately 2.841 x 10 ^7 byte/sec.
However, there is always a mismatch for the for time we run this test. When we run the test again, the mismatch disappeared automatically and we conclude that it might be because of the timing issue inside the verilog logic where two fifos are communicating with each other.
