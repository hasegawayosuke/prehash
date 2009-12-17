CC=CL
CFLAGS=/Ox /GS /GA /W4 /DUNICODE /D_UNICODE 
LIBS=shell32.lib user32.lib advapi32.lib
TARGET=prehash

all : $(TARGET).exe

.c.exe :
	$(CC) $(CFLAGS) $< /link $(LIBS)

clean :
	@if exist $(TARGET).exe del $(TARGET).exe
	@if exist $(TARGET).obj del $(TARGET).obj
