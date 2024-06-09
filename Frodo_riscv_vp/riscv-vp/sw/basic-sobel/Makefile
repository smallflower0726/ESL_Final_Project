all : main.cpp
	/opt/riscv-gnu-toolchain/bin/riscv-none-elf-g++ -std=c++14 main.cpp -o main -march=rv32ima -mabi=ilp32
	
sim: all
	$(EE6470)/riscv-vp/vp/build/bin/riscv-vp-acc --intercept-syscalls main
	
clean:
	rm -f main
