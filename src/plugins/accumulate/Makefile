LDFLAGS=-L../.. -lhwloc -lhmon

accumulate.monitor_plugin.so: accumulate_monitor.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f accumulate.monitor_plugin.so

