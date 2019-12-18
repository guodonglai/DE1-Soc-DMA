In this project, we managed to make DMA data transfer on DE1-SOC board by leveraging linux DMA API.
Up until now, our system could handle DMA transfer for more than 400 MB of data using multiple small transfers inside a loop. But we did not make the scatter-gather
style work entirely. sg works only for data transfering end but not for data receiving end.
