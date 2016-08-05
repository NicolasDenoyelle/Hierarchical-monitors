LDFLAGS=-L../.. -lhwloc -lhmon
CFLAGS=-std=gnu99 -g

hierarchical.monitor_plugin.so: hierarchical_monitor.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f hierarchical.monitor_plugin.so

