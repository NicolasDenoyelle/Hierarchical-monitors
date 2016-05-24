CC=gcc
CFLAGS=-Wall -Wextra
LDFLAGS=-L. -lhwloc 

# Installation directories
PREFIX=/usr
MONITOR_INCLUDE_INSTALL_DIR=$(PREFIX)/include
MONITOR_LIB_INSTALL_DIR=$(PREFIX)/lib
MONITOR_PLUGINS_INSTALL_DIR=$(PREFIX)/lib
MONITOR_BIN_INSTALL_DIR=$(PREFIX)/bin

# performance plugins to compile
PERF_PLUGINS=fake hierarchical papi trace maqao 

# Not a filename targets
.PHONY: default all install clean uninstall

# default target
default: all

################################################################################################################
#                                                    BIN+LIB                                                   #
################################################################################################################
LIB=libmonitor.so
BIN=monitor 
SRC=monitor.c array.c hwloc.c proc.c parser.c scanner.c console.c performance.c stats.c 
OBJ=$(patsubst %.c, %.o, $(SRC))

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@  $(LDFLAGS)

parser.c: parser.y 
	bison -o $@ --defines=parser.h $<

scanner.c: scanner.l
	flex -o $@ $^

$(LIB): $(OBJ)
	$(CC) $(CFLAGS) --shared $^ -o $@ $(LDFLAGS) -fPIC -lm -lpthread

$(BIN): main.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lmonitor -lrt

################################################################################################################
#                                                Performance plugins                                           #
################################################################################################################
PERF_PLUGINS_LIB=$(patsubst %, %.monitor_plugin.so, $(PERF_PLUGINS))
PERF_PLUGINS_LDFLAGS=-lmonitor

ifeq (papi, $(findstring papi,$(PERF_PLUGINS))) 
PERF_PLUGINS_LDFLAGS+= -lpapi
endif

ifeq (maqao, $(findstring maqao,$(PERF_PLUGINS))) 
PERF_PLUGINS_LDFLAGS+= -llprof
endif

%.monitor_plugin.so: plugins/%/*.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS) $(PERF_PLUGINS_LD_FLAGS)


###############################################################################################################
#                                             PHONY: Clean, Install ...                                       #
###############################################################################################################

all: $(LIB) $(BIN) $(PERF_PLUGINS_LIB)

install: all
	mv $(LIB) $(MONITOR_LIB_INSTALL_DIR)
	mv $(BIN) $(MONITOR_BIN_INSTALL_DIR)
	@for file in $(PERF_PLUGINS_LIB); do\
		cp $$file $(MONITOR_LIB_INSTALL_DIR); \
	done
	cp monitor.h $(MONITOR_INCLUDE_INSTALL_DIR)
	cp $(MONITOR_BIN) $(MONITOR_BIN_INSTALL_DIR)

clean: 
	rm -f scanner.c parser.h parser.c $(LIB) $(BIN) $(OBJ) $(PERF_PLUGINS_LIB)

uninstall: clean
	@for file in $(PERF_PLUGINS_LIB); do\
                echo "rm -f $(MONITOR_LIB_INSTALL_DIR)/$$file";\
		rm -f $(MONITOR_LIB_INSTALL_DIR)/$$file; \
	done
	rm -f $(MONITOR_INCLUDE_INSTALL_DIR)/monitor.h
	rm -f $(MONITOR_BIN_INSTALL_DIR)/$(BIN)
	rm -f $(MONITOR_LIB_INSTALL_DIR)/$(LIB) 

