# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/rrlingam/workspace/lab2b_platform/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/rrlingam/workspace/lab2b_platform/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {lab2b_platform}\
-hw {/home/rrlingam/Pre_Lab/system_wrapper.xsa}\
-proc {microblaze_0} -os {standalone} -out {/home/rrlingam/workspace}

platform write
platform generate -domains 
platform active {lab2b_platform}
bsp reload
platform active {lab2b_platform}
domain create -name {standalone_microblaze_0} -display-name {standalone_microblaze_0} -os {standalone} -proc {microblaze_0} -runtime {cpp} -arch {32-bit} -support-app {hello_world}
platform generate -domains 
platform write
domain active {standalone_domain}
domain active {standalone_microblaze_0}
platform generate -quick
platform generate
platform generate
platform generate
platform active {lab2b_platform}
domain active {standalone_domain}
bsp reload
platform generate -domains 
