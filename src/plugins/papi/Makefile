LDFLAGS=-L../.. -lhwloc -lhmon -lpapi

papi.monitor_plugin.so: PAPI_monitor.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS)

clean:
	rm -f papi.monitor_plugin.so
