all: libipt_FULLCONENAT.so libip6t_FULLCONENAT.so
.PHONY: all

%.so: %.o
	$(CC) -shared -lxtables -o $@ $^

%.o: %.c
	$(CC) ${CFLAGS} -fPIC -c -o $@ $<

obj-m += xt_FULLCONENAT.o
