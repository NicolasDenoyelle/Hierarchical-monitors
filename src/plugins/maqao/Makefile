LDFLAGS=-L../.. -lhwloc -lhmon -llprof

maqao.monitor_plugin.so: maqao_monitor.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f maqao.monitor_plugin.so
