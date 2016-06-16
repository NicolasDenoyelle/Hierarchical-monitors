LDFLAGS=-L../.. -lhwloc -lhmon -llprof

fake.monitor_plugin.so: fake_monitor.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f fake.monitor_plugin.so

