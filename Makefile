parser: nmea-parser.c
	cc -g -o parser nmea-parser.c

clean:
	rm -f parser
