all:	dt flnks smd sft uid gid slnks perms size sall

dt:		runner.o
	gcc -o dt runner.o

flnks:		followlinks.o
	gcc -o flnks followlinks.o

smd:		showlastmod.o
	gcc -o smd showlastmod.c

sft:		showfiletype.o
	gcc -o sft showfiletype.o

uid:		showuid.o
	gcc -o uid showuid.o

gid:		showgid.o
	gcc -o gid showgid.o

slnks:		showlinks.o
	gcc -o slnks showlinks.o

perms:		showpermissions.o
	gcc -o perms showpermissions.o

size:		showsize.o
	gcc -o size showsize.o

sall:		showall.o
	gcc -o sall showall.o

runner.o:	runner.c
	gcc -c runner.c

followlinks.o:	followlinks.c
	gcc -c followlinks.c

showlastmod.o:	showlastmod.c
	gcc -c showlastmod.c

showfiletype.o:	showfiletype.c
	gcc -c showfiletype.c

showuid.o:	showuid.c
	gcc -c showuid.c

showgid.o:	showgid.c
	gcc -c showgid.c

showlinks.o:	showlinks.c
	gcc -c showlinks.c

showpermissions.o: showpermissions.c
	gcc -c showpermissions.c

showsize.o:	showsize.c
	gcc -c showsize.c

showall.o:	showall.c
	gcc -c showall.c

clean:
	rm -f *.o

smooth:
	rm -f *.o  dt flnks smd sft uid gid slnks perms size sall
