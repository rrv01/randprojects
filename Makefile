parser: nmea-parser.c
	cc -o parser nmea-parser.c

clean:
	rm -f parser
