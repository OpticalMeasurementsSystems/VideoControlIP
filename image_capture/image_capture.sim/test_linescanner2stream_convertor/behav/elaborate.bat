@echo off
set xv_path=C:\\XilinxTools\\Vivado\\2016.2\\bin
call %xv_path%/xelab  -wto 3f5aa38eb5794db5b10b89e0e066941c -m64 --debug typical --relax --mt 2 -L xil_defaultlib -L xpm -L unisims_ver -L unimacro_ver -L secureip --snapshot test_linescanner2stream_convertor_behav xil_defaultlib.test_linescanner2stream_convertor xil_defaultlib.glbl -log elaborate.log
if "%errorlevel%"=="0" goto SUCCESS
if "%errorlevel%"=="1" goto END
:END
exit 1
:SUCCESS
exit 0
