CC=gcc
CFLAGS=-Wall -Wextra -Werror -g -std=gnu99
LDFLAGS=-L. -lhwloc -lpthread -lrt -ldl -lm

# Installation directories
PREFIX=$(HOME)/local
MONITOR_INCLUDE_INSTALL_DIR=$(PREFIX)/include
MONITOR_LIB_INSTALL_DIR=$(PREFIX)/lib
MONITOR_PLUGINS_INSTALL_DIR=$(PREFIX)/lib
MONITOR_BIN_INSTALL_DIR=$(PREFIX)/bin

# performance plugins to compile
PLUGINS=fake accumulate hierarchical papi trace maqao stat_default stat_gsl

# Not a filename targets
.PHONY: default all install clean uninstall

# default target
default: all

################################################################################################################
#                                                    BIN+LIB                                                   #
################################################################################################################
LIB=libhmon.so
BIN=hmon 
SRC=monitor.c hmon_array.c hwloc.c proc.c parser.c scanner.c console.c plugin.c
OBJ=$(patsubst %.c, %.o, $(SRC))

STAT_PLUGINS_DEF=stat_default
ifneq (,$(findstring stat_gsl, $(PLUGINS)))
	STAT_PLUGINS_DEF=stat_default:stat_gsl
endif
CFLAGS+=-DSTAT_PLUGINS="\"$(STAT_PLUGINS_DEF)\""

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@  $(LDFLAGS)

parser.c: parser.y 
	bison -o $@ --defines=parser.h $<

scanner.c: scanner.l
	flex -o $@ $^

$(LIB): $(OBJ)
	$(CC) $(CFLAGS) --shared $^ -o $@ $(LDFLAGS) -fPIC

$(BIN): main.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lhmon

################################################################################################################
#                                                      plugins                                                 #
################################################################################################################
PLUGINS_LIB=$(patsubst %, %.monitor_plugin.so, $(PLUGINS))

ifneq (,$(findstring papi, $(PLUGINS)))
	PLUGINS_LDFLAGS=-lpapi
endif
ifneq (,$(findstring maqao, $(PLUGINS)))
	PLUGINS_LDFLAGS+=-llprof
endif
ifneq (,$(findstring stat_gsl, $(PLUGINS)))
	PLUGINS_LDFLAGS+=-lgsl
endif

%.monitor_plugin.so: plugins/%/*.c
	$(CC) $(CFLAGS) --shared $^ -o $@ -fPIC $(LDFLAGS) $(PLUGINS_LDFLAGS) -lhmon


###############################################################################################################
#                                             PHONY: Clean, Install ...                                       #
###############################################################################################################

all: $(LIB) $(BIN) $(PLUGINS_LIB)

install: all
	cp $(LIB) $(MONITOR_LIB_INSTALL_DIR)
	cp $(BIN) $(MONITOR_BIN_INSTALL_DIR)
	@for plugin in $(PLUGINS_LIB); do\
		cp $$plugin $(MONITOR_LIB_INSTALL_DIR); \
	done
	cp monitor.h $(MONITOR_INCLUDE_INSTALL_DIR)
	cp monitor_utils.h $(MONITOR_INCLUDE_INSTALL_DIR)

clean: 
	rm -f scanner.c parser.h parser.c $(LIB) $(BIN) $(OBJ) $(PLUGINS_LIB)

uninstall: clean
	@for file in $(PLUGINS_LIB); do\
                echo "rm -f $(MONITOR_LIB_INSTALL_DIR)/$$file";\
		rm -f $(MONITOR_LIB_INSTALL_DIR)/$$file; \
	done
	rm -f  $(MONITOR_INCLUDE_INSTALL_DIR)/monitor.h $(MONITOR_INCLUDE_INSTALL_DIR)/monitor_utils.h
	rm -f $(MONITOR_BIN_INSTALL_DIR)/$(BIN)
	rm -f $(MONITOR_LIB_INSTALL_DIR)/$(LIB) 

