CROSS = riscv64-unknown-elf-

CFLAGS = -march=rv64gc -mabi=lp64 \
         -mcmodel=medany \
         -msmall-data-limit=0 \
         -ffreestanding \
         -nostdlib -nostartfiles \
         -fno-stack-protector \
         -Wall -Iinclude

OBJS = start.o trap_entry.o context.o \
       main.o task.o scheduler.o uart.o string.o memory.o

all: kernel.elf

kernel.elf: $(OBJS)
	$(CROSS)ld -T linker.ld $(OBJS) -o kernel.elf

start.o:      boot/start.S
	$(CROSS)gcc $(CFLAGS) -c boot/start.S
trap_entry.o: boot/trap_entry.S
	$(CROSS)gcc $(CFLAGS) -c boot/trap_entry.S
context.o:    kernel/context.S
	$(CROSS)gcc $(CFLAGS) -c kernel/context.S
main.o:       kernel/main.c
	$(CROSS)gcc $(CFLAGS) -c kernel/main.c
task.o:       kernel/task.c
	$(CROSS)gcc $(CFLAGS) -c kernel/task.c
scheduler.o:  kernel/scheduler.c
	$(CROSS)gcc $(CFLAGS) -c kernel/scheduler.c
uart.o:       kernel/uart.c
	$(CROSS)gcc $(CFLAGS) -c kernel/uart.c
string.o:     kernel/string.c
	$(CROSS)gcc $(CFLAGS) -c kernel/string.c
memory.o:     kernel/memory.c
	$(CROSS)gcc $(CFLAGS) -c kernel/memory.c

qemu: kernel.elf
	qemu-system-riscv64 -machine virt -bios none -kernel kernel.elf -nographic

clean:
	rm -f *.o kernel.elf