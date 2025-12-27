OBJS     = malahit.o ../GPIO.o ../STM.o ../SPI.o
CXXFLAGS = -O3 -I..
LIBS     = -lgpiod

malahit: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)

