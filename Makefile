CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

OBJS =		main.o UPNPNAT.o xmlParser.o

LIBS =

TARGET =	UPNPPortForward

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
