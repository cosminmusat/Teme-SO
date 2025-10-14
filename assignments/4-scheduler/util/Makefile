# CC = gcc
# CFLAGS = -g -fPIC -Wall - Wextra
# LDFLAGS = -m32

# .PHONY: build
# build: libscheduler.so

# libscheduler.so: so_scheduler.o
# 	$(CC) $(CFLAGS) -shared -o $@ $^

# so_scheduler.o: so_scheduler.c so_scheduler.h
# 	$(CC) $(CFLAGS) -o $@ -c $<

# .PHONY: clean
# clean:
# 	-rm -f so_scheduler.o libscheduler.so


CC = gcc
CFLAGS = -g -fPIC -pthread -Wall -Wextra
LDFLAGS = -m32

.PHONY: build
build: libscheduler.so

libscheduler.so: so_scheduler.o circular_queue.o
	$(CC) $(CFLAGS) -shared -o libscheduler.so so_scheduler.o circular_queue.o

so_scheduler.o: so_scheduler.c so_scheduler.h
	$(CC) $(CFLAGS) -o so_scheduler.o -c so_scheduler.c

circular_queue.o: circular_queue.c circular_queue.h
	$(CC) $(CFLAGS) -o circular_queue.o -c circular_queue.c

.PHONY: clean
clean:
	-rm -rf so_scheduler.o libscheduler.so