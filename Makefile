parser: nmea-parser.c
	cc -g -o parser nmea-parser.c -lpthread

clean:
	rm -f parser
