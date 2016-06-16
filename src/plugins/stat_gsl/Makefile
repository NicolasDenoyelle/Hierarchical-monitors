LDFLAGS=-L../.. -lhwloc -lhmon -lgsl
CFLAGS+=-I../..

stat_gsl.monitor_plugin.so: stat.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f stat_gsl.monitor_plugin.so
