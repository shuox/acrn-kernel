# SPDX-License-Identifier: GPL-2.0

.PHONY: vm-sample

CC=aarch64-linux-gnu-gcc -static
LDSHARED=aarch64-linux-gnu-gcc -static
LD=aarch64-linux-gnu-ld -static
AS=aarch64-linux-gnu-as
AR=aarch64-linux-gnu-ar rc
RANLIB=aarch64-linux-gnu-ranlib

vm-sample: vm-sample.o payload.o
	$(CC) $^ -o $@

payload.o: payload.ld startup.o
	$(LD) -T $< -o $@

clean:
	rm *.o vm-sample
