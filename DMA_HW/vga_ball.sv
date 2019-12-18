/*
 * Avalon memory-mapped peripheral that generates VGA
 *
 * Stephen A. Edwards
 * Columbia University
 */

module vga_ball(input logic        clk,
	        input logic 	   reset,
		input logic [31:0]  writedata,
		input logic 	   write,
		input 		   chipselect,
		input logic [7:0]  address,

		output logic [7:0] VGA_R, VGA_G, VGA_B,
		output logic 	   VGA_CLK, VGA_HS, VGA_VS,
		                   VGA_BLANK_n,
		output logic 	   VGA_SYNC_n);

   logic [10:0]	   hcount;
   logic [9:0]     vcount;
   logic [3:0]	   title_state [0:39];
   logic [0:15]    number_zero[0:15], number_one[0:15], number_two[0:15],number_three[0:15],number_four[0:15];
   logic [0:15]	   number_five[0:15],number_six[0:15],number_seven[0:15],number_eight[0:15],number_nine[0:15];

   vga_counters counters(.clk50(clk), .*);

   always_ff @(posedge clk)
     if (reset) begin
	title_state[0][3:0]  <=		4'd 0;
	title_state[1][3:0]  <=		4'd 1;
	title_state[2][3:0]  <=		4'd 2;
	title_state[3][3:0]  <=		4'd 3;
	title_state[4][3:0]  <=		4'd 4;
	title_state[5][3:0]  <=		4'd 5;
	title_state[6][3:0]  <=		4'd 6;
	title_state[7][3:0]  <=		4'd 7;
	title_state[8][3:0]  <=		4'd 8;
	title_state[9][3:0]  <=		4'd 9;
	for(int i = 10; i < 14 ; i++)	title_state[i][3:0]  <=4'd 11;
	title_state[14][3:0]  <=	4'd 0;
	title_state[15][3:0]  <=	4'd 1;
	title_state[16][3:0]  <=	4'd 2;
	title_state[17][3:0]  <=	4'd 3;
	title_state[18][3:0]  <=	4'd 4;
	title_state[19][3:0]  <=	4'd 5;
	title_state[20][3:0]  <=	4'd 6;
	title_state[21][3:0]  <=	4'd 7;
	title_state[22][3:0]  <=	4'd 8;
	title_state[23][3:0]  <=	4'd 9;
	for( int j = 24; j < 40; j++)	title_state[j][3:0]  <=4'd 11;

	//number0
	number_zero[0][0:15] <=		16'b0000000000000000;
	number_zero[1][0:15] <=		16'b0000000000000000;
	number_zero[2][0:15] <=		16'b0000000000000000;
	number_zero[3][0:15] <=		16'b0000011111100000;
	number_zero[4][0:15] <=		16'b0000111111110000;
	number_zero[5][0:15] <=		16'b0000110000110000;
	number_zero[6][0:15] <=		16'b0000110000110000;
	number_zero[7][0:15] <=		16'b0000110000110000;
	number_zero[8][0:15] <=		16'b0000110000110000;
	number_zero[9][0:15] <=		16'b0000110000110000;
	number_zero[10][0:15] <=	16'b0000110000110000;
	number_zero[11][0:15] <=	16'b0000111111110000;
	number_zero[12][0:15] <=	16'b0000011111100000;
	number_zero[13][0:15] <=	16'b0000000000000000;
	number_zero[14][0:15] <=	16'b0000000000000000;
	number_zero[15][0:15] <=	16'b0000000000000000;
	//number1
	number_one[0][0:15] <=		16'b0000000000000000;
	number_one[1][0:15] <=		16'b0000000000000000;
	number_one[2][0:15] <=		16'b0000000000000000;
	number_one[3][0:15] <=		16'b0000000110000000;
	number_one[4][0:15] <=		16'b0000001110000000;
	number_one[5][0:15] <=		16'b0000011110000000;
	number_one[6][0:15] <=		16'b0000000110000000;
	number_one[7][0:15] <=		16'b0000000110000000;
	number_one[8][0:15] <=		16'b0000000110000000;
	number_one[9][0:15] <=		16'b0000000110000000;
	number_one[10][0:15] <=		16'b0000000110000000;
	number_one[11][0:15] <=		16'b0000011111100000;
	number_one[12][0:15] <=		16'b0000011111100000;
	number_one[13][0:15] <=		16'b0000000000000000;
	number_one[14][0:15] <=		16'b0000000000000000;
	number_one[15][0:15] <=		16'b0000000000000000;
	//number2
	number_two[0][0:15] <=		16'b0000000000000000;
	number_two[1][0:15] <=		16'b0000000000000000;
	number_two[2][0:15] <=		16'b0000000000000000;
	number_two[3][0:15] <=		16'b0000011111100000;
	number_two[4][0:15] <=		16'b0000111111110000;
	number_two[5][0:15] <=		16'b0000110000110000;
	number_two[6][0:15] <=		16'b0000000001110000;
	number_two[7][0:15] <=		16'b0000000011100000;
	number_two[8][0:15] <=		16'b0000000111000000;
	number_two[9][0:15] <=		16'b0000001110000000;
	number_two[10][0:15] <=		16'b0000011100000000;
	number_two[11][0:15] <=		16'b0000111111110000;
	number_two[12][0:15] <=		16'b0000111111110000;
	number_two[13][0:15] <=		16'b0000000000000000;
	number_two[14][0:15] <=		16'b0000000000000000;
	number_two[15][0:15] <=		16'b0000000000000000;
	//number3
	number_three[0][0:15] <=	16'b0000000000000000;
	number_three[1][0:15] <=	16'b0000000000000000;
	number_three[2][0:15] <=	16'b0000000000000000;
	number_three[3][0:15] <=	16'b0000011111100000;
	number_three[4][0:15] <=	16'b0000111111110000;
	number_three[5][0:15] <=	16'b0000110000110000;
	number_three[6][0:15] <=	16'b0000000000110000;
	number_three[7][0:15] <=	16'b0000000111100000;
	number_three[8][0:15] <=	16'b0000000111100000;
	number_three[9][0:15] <=	16'b0000000000110000;
	number_three[10][0:15] <=	16'b0000110000110000;
	number_three[11][0:15] <=	16'b0000111111110000;
	number_three[12][0:15] <=	16'b0000011111100000;
	number_three[13][0:15] <=	16'b0000000000000000;
	number_three[14][0:15] <=	16'b0000000000000000;
	number_three[15][0:15] <=	16'b0000000000000000;
	//number4
	number_four[0][0:15] <=		16'b0000000000000000;
	number_four[1][0:15] <=		16'b0000000000000000;
	number_four[2][0:15] <=		16'b0000000000000000;
	number_four[3][0:15] <=		16'b0000000011100000;
	number_four[4][0:15] <=		16'b0000000111100000;
	number_four[5][0:15] <=		16'b0000001111100000;
	number_four[6][0:15] <=		16'b0000011101100000;
	number_four[7][0:15] <=		16'b0000111001100000;
	number_four[8][0:15] <=		16'b0000110001100000;
	number_four[9][0:15] <=		16'b0000111111110000;
	number_four[10][0:15] <=	16'b0000111111110000;
	number_four[11][0:15] <=	16'b0000000001100000;
	number_four[12][0:15] <=	16'b0000000001100000;
	number_four[13][0:15] <=	16'b0000000000000000;
	number_four[14][0:15] <=	16'b0000000000000000;
	number_four[15][0:15] <=	16'b0000000000000000;
	//number5
	number_five[0][0:15] <=		16'b0000000000000000;
	number_five[1][0:15] <=		16'b0000000000000000;
	number_five[2][0:15] <=		16'b0000000000000000;
	number_five[3][0:15] <=		16'b0000111111110000;
	number_five[4][0:15] <=		16'b0000111111110000;
	number_five[5][0:15] <=		16'b0000110000000000;
	number_five[6][0:15] <=		16'b0000110000000000;
	number_five[7][0:15] <=		16'b0000111111100000;
	number_five[8][0:15] <=		16'b0000111111110000;
	number_five[9][0:15] <=		16'b0000000000110000;
	number_five[10][0:15] <=	16'b0000000000110000;
	number_five[11][0:15] <=	16'b0000111111110000;
	number_five[12][0:15] <=	16'b0000111111100000;
	number_five[13][0:15] <=	16'b0000000000000000;
	number_five[14][0:15] <=	16'b0000000000000000;
	number_five[15][0:15] <=	16'b0000000000000000;
	//number6
	number_six[0][0:15] <=		16'b0000000000000000;
	number_six[1][0:15] <=		16'b0000000000000000;
	number_six[2][0:15] <=		16'b0000000000000000;
	number_six[3][0:15] <=		16'b0000011111110000;
	number_six[4][0:15] <=		16'b0000111111110000;
	number_six[5][0:15] <=		16'b0000110000000000;
	number_six[6][0:15] <=		16'b0000110000000000;
	number_six[7][0:15] <=		16'b0000111111100000;
	number_six[8][0:15] <=		16'b0000111111110000;
	number_six[9][0:15] <=		16'b0000110000110000;
	number_six[10][0:15] <=		16'b0000110000110000;
	number_six[11][0:15] <=		16'b0000111111110000;
	number_six[12][0:15] <=		16'b0000011111100000;
	number_six[13][0:15] <=		16'b0000000000000000;
	number_six[14][0:15] <=		16'b0000000000000000;
	number_six[15][0:15] <=		16'b0000000000000000;
	//number7
	number_seven[0][0:15] <=	16'b0000000000000000;
	number_seven[1][0:15] <=	16'b0000000000000000;
	number_seven[2][0:15] <=	16'b0000000000000000;
	number_seven[3][0:15] <=	16'b0000111111110000;
	number_seven[4][0:15] <=	16'b0000111111110000;
	number_seven[5][0:15] <=	16'b0000000000110000;
	number_seven[6][0:15] <=	16'b0000000001110000;
	number_seven[7][0:15] <=	16'b0000000011100000;
	number_seven[8][0:15] <=	16'b0000000111000000;
	number_seven[9][0:15] <=	16'b0000001110000000;
	number_seven[10][0:15] <=	16'b0000011100000000;
	number_seven[11][0:15] <=	16'b0000111000000000;
	number_seven[12][0:15] <=	16'b0000110000000000;
	number_seven[13][0:15] <=	16'b0000000000000000;
	number_seven[14][0:15] <=	16'b0000000000000000;
	number_seven[15][0:15] <=	16'b0000000000000000;
	//number8
	number_eight[0][0:15] <=	16'b0000000000000000;
	number_eight[1][0:15] <=	16'b0000000000000000;
	number_eight[2][0:15] <=	16'b0000000000000000;
	number_eight[3][0:15] <=	16'b0000011111100000;
	number_eight[4][0:15] <=	16'b0000111111110000;
	number_eight[5][0:15] <=	16'b0000110000110000;
	number_eight[6][0:15] <=	16'b0000110000110000;
	number_eight[7][0:15] <=	16'b0000011111100000;
	number_eight[8][0:15] <=	16'b0000011111100000;
	number_eight[9][0:15] <=	16'b0000110000110000;
	number_eight[10][0:15] <=	16'b0000110000110000;
	number_eight[11][0:15] <=	16'b0000111111110000;
	number_eight[12][0:15] <=	16'b0000011111100000;
	number_eight[13][0:15] <=	16'b0000000000000000;
	number_eight[14][0:15] <=	16'b0000000000000000;
	number_eight[15][0:15] <=	16'b0000000000000000;
	//number9
	number_nine[0][0:15] <=		16'b0000000000000000;
	number_nine[1][0:15] <=		16'b0000000000000000;
	number_nine[2][0:15] <=		16'b0000000000000000;
	number_nine[3][0:15] <=		16'b0000011111100000;
	number_nine[4][0:15] <=		16'b0000111111110000;
	number_nine[5][0:15] <=		16'b0000110000110000;
	number_nine[6][0:15] <=		16'b0000110000110000;
	number_nine[7][0:15] <=		16'b0000111111110000;
	number_nine[8][0:15] <=		16'b0000011111110000;
	number_nine[9][0:15] <=		16'b0000000000110000;
	number_nine[10][0:15] <=	16'b0000000000110000;
	number_nine[11][0:15] <=	16'b0000111111110000;
	number_nine[12][0:15] <=	16'b0000111111100000;
	number_nine[13][0:15] <=	16'b0000000000000000;
	number_nine[14][0:15] <=	16'b0000000000000000;
	number_nine[15][0:15] <=	16'b0000000000000000;
	

     end else if (chipselect && write) begin
         title_state[address][3:0] <= writedata;	
       end

   always_comb begin
      {VGA_R, VGA_G, VGA_B} = {8'h0, 8'h0, 8'h0};
      if (VGA_BLANK_n )
	if(title_state[hcount[10:1]>>4][3:0] == 0 && number_zero[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};
	else if(title_state[hcount[10:1]>>4][3:0] == 1 && number_one[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};
	else if(title_state[hcount[10:1]>>4][3:0] == 2 && number_two[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 3 && number_three[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 4 && number_four[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 5 && number_five[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 6 && number_six[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 7 && number_seven[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 8 && number_eight[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else if(title_state[hcount[10:1]>>4][3:0] == 9 && number_nine[vcount[3:0]][hcount[4:1]] == 1 && vcount < 16)
				{VGA_R, VGA_G, VGA_B} = {8'hff, 8'h00, 8'h00};	
	else begin
		{VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00};	
	end
   end
	       
endmodule

module vga_counters(
 input logic 	     clk50, reset,
 output logic [10:0] hcount,  // hcount[10:1] is pixel column
 output logic [9:0]  vcount,  // vcount[9:0] is pixel row
 output logic 	     VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n);

/*
 * 640 X 480 VGA timing for a 50 MHz clock: one pixel every other cycle
 * 
 * HCOUNT 1599 0             1279       1599 0
 *             _______________              ________
 * ___________|    Video      |____________|  Video
 * 
 * 
 * |SYNC| BP |<-- HACTIVE -->|FP|SYNC| BP |<-- HACTIVE
 *       _______________________      _____________
 * |____|       VGA_HS          |____|
 */
   // Parameters for hcount
   parameter HACTIVE      = 11'd 1280,
             HFRONT_PORCH = 11'd 32,
             HSYNC        = 11'd 192,
             HBACK_PORCH  = 11'd 96,   
             HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC +
                            HBACK_PORCH; // 1600
   
   // Parameters for vcount
   parameter VACTIVE      = 10'd 480,
             VFRONT_PORCH = 10'd 10,
             VSYNC        = 10'd 2,
             VBACK_PORCH  = 10'd 33,
             VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC +
                            VBACK_PORCH; // 525

   logic endOfLine;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          hcount <= 0;
     else if (endOfLine) hcount <= 0;
     else  	         hcount <= hcount + 11'd 1;

   assign endOfLine = hcount == HTOTAL - 1;
       
   logic endOfField;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          vcount <= 0;
     else if (endOfLine)
       if (endOfField)   vcount <= 0;
       else              vcount <= vcount + 10'd 1;

   assign endOfField = vcount == VTOTAL - 1;

   // Horizontal sync: from 0x520 to 0x5DF (0x57F)
   // 101 0010 0000 to 101 1101 1111
   assign VGA_HS = !( (hcount[10:8] == 3'b101) &
		      !(hcount[7:5] == 3'b111));
   assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

   assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused
   
   // Horizontal active: 0 to 1279     Vertical active: 0 to 479
   // 101 0000 0000  1280	       01 1110 0000  480
   // 110 0011 1111  1599	       10 0000 1100  524
   assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
			!( vcount[9] | (vcount[8:5] == 4'b1111) );

   /* VGA_CLK is 25 MHz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *        
    *             _____       __
    * hcount[0]__|     |_____|
    */
   assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive
   
endmodule
