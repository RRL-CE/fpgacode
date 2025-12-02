# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/rrlingam/workspace/system_wrapper/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/rrlingam/workspace/system_wrapper/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {system_wrapper}\
-hw {/home/rrlingam/Pre_Lab/system_wrapper.xsa}\
-out {/home/rrlingam/workspace}

platform write
domain create -name {standalone_microblaze_0} -display-name {standalone_microblaze_0} -os {standalone} -proc {microblaze_0} -runtime {cpp} -arch {32-bit} -support-app {hello_world}
platform generate -domains 
platform active {system_wrapper}
platform generate -quick
bsp reload
platform generate
platform active {system_wrapper}
bsp reload
platform active {system_wrapper}
bsp reload
platform active {system_wrapper}
platform active {system_wrapper}
bsp reload
platform active {system_wrapper}
bsp reload
platform clean
catch {bsp regenerate}
platform generate
platform clean
platform generate
platform clean
platform generate
platform active {system_wrapper}
bsp reload
platform active {system_wrapper}
bsp reload
platform active {system_wrapper}
bsp reload
catch {platform remove lab1b}
platform clean
platform clean
platform generate
platform clean
platform generate
platform generate
