#AXI Signals
add_force {/test_linescanner2stream_convertor/axi_aclk} -radix hex {1 0ns} {0 5000ps} -repeat_every 10000ps
add_force {/test_linescanner2stream_convertor/axi_tready} -radix hex {0} {1 150us} {0 950us} 
add_force {/test_linescanner2stream_convertor/axi_aresetn} -radix hex {1} {0 100us} {1 970us} 
#module enable
add_force {/test_linescanner2stream_convertor/enable} -radix hex {0} {1 200us} {0 900us} 
#data from scanner
add_force {/test_linescanner2stream_convertor/pixel_data} -radix hex {0} {0x0F 220us} {0xF0 270us} {0x0F 320us} {0xF0 370us} {0 420us} {0x0F 520us} {0xF0 570us} {0x0F 620us} {0xF0 670us} {0 720us}
add_force {/test_linescanner2stream_convertor/pixel_captured} -radix hex {0} {1 220us} {0 270us} {1 320us} {0 370us} {1 520us} {0 570us} {1 620us} {0 670us}