CXX = g++
CXXFLAGS = -std=c++11 -pedantic -Wall -Wextra -O2
LDFLAGS = -lsqlite3
TARGETS = tolhnet_plot
STATIC_FILES = index.html planimetria2.png service.html
STATIC_CGIS = tolhnet_service

# targets

all: $(TARGETS)

clean:
	rm -f $(TARGETS) 

install: $(TARGETS)
	cp $(STATIC_FILES) /var/www/html/tolhnet/
	cp $(TARGETS) $(STATIC_CGIS) /usr/lib/cgi-bin/tolhnet/


tolhnet_plot: tolhnet_plot.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean install

