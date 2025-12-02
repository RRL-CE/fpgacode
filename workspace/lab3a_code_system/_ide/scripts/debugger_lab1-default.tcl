# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: /home/rrlingam/workspace/lab3a_code_system/_ide/scripts/debugger_lab1-default.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source /home/rrlingam/workspace/lab3a_code_system/_ide/scripts/debugger_lab1-default.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -filter {jtag_cable_name =~ "Digilent Nexys A7 -100T 210292BD0269A" && level==0 && jtag_device_ctx=="jsn-Nexys A7 -100T-210292BD0269A-13631093-0"}
fpga -file /home/rrlingam/workspace/lab3a_code/_ide/bitstream/system_wrapper.bit
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
loadhw -hw /home/rrlingam/workspace/lab3a_platform/export/lab3a_platform/hw/system_wrapper.xsa -regs
configparams mdm-detect-bscan-mask 2
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
rst -system
after 3000
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
dow /home/rrlingam/workspace/lab3a_code/Debug/lab3a_code.elf
bpadd -addr &main
