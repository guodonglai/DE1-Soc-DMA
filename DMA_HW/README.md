Detailed instruction about how to set up hardware portion on QSYS can be found in the google docs that we shared with you.
There are two versions of hardware inside ip/flow_control_fifo folder:
flow_control_fifo.v is the version with only one fifo.
flow_control_fifo_tx_ack.v is the version with two fifos, one for data_in and one for data_out. 
To switch between two versions, you just need to edit the source file inside loopback_fifo ip. 
