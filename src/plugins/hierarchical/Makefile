LDFLAGS=-L../.. -lhwloc -lhmon

hierarchical.monitor_plugin.so: hierarchical_monitor.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f hierarchical.monitor_plugin.so

