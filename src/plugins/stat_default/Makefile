LDFLAGS=-L../.. -lhwloc -lhmon
CFLAGS+=-I../..

stat_default.monitor_plugin.so: stats.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f stat_default.monitor_plugin.so

