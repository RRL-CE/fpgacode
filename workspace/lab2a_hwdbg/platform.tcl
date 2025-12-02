# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/rrlingam/workspace/lab2a_hwdbg/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/rrlingam/workspace/lab2a_hwdbg/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {lab2a_hwdbg}\
-hw {/home/rrlingam/Pre_Lab/system_wrapper.xsa}\
-proc {microblaze_0} -os {standalone} -out {/home/rrlingam/workspace}

platform write
platform generate -domains 
platform active {lab2a_hwdbg}
platform generate
platform generate
