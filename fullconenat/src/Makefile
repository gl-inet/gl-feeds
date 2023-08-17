libipt_FULLCONENAT.so: libipt_FULLCONENAT.o
	$(CC) -shared -lxtables -o $@ $^;
libipt_FULLCONENAT.o: libipt_FULLCONENAT.c
	$(CC) ${CFLAGS} -fPIC -c -o $@ $<;

obj-m += xt_FULLCONENAT.o
