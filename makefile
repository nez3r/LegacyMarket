# Имя компилятора (точный путь к 32-битному MinGW 11.2)
CXX = c:/mingw32/bin/g++.exe
CC = c:/mingw32/bin/gcc.exe

# Флаги компилятора
CXXFLAGS = -Wall -O2 -D_WIN32_WINNT=0x0501
CFLAGS = -Wall -O2 -D_WIN32_WINNT=0x0501

# Библиотеки для линковки (WinINet для загрузки файлов)
LIBS = -Wl,--subsystem,windows:5.01 -static -static-libgcc -lcomctl32 -lshell32 -lgdi32 -luser32 -lwininet -mwindows -Wl,--wrap=GetTickCount64

# Папка для готовой сборки
OUT_DIR = out

# Имя готового исполняемого файла
TARGET = $(OUT_DIR)/market.exe

# Исходные файлы
SRCS = main.cpp download.cpp xp_compat.cpp
OBJS = main.o download.o xp_compat.o

all: $(TARGET)

main.o: main.cpp download.h
	$(CXX) -c main.cpp -o main.o $(CXXFLAGS)

download.o: download.cpp download.h
	$(CXX) -c download.cpp -o download.o $(CXXFLAGS)

xp_compat.o: xp_compat.cpp
	$(CXX) -c xp_compat.cpp -o xp_compat.o $(CXXFLAGS)

$(TARGET): $(OBJS)
	@echo Building in $(OUT_DIR)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS) $(LIBS)
	@echo Build complete!

clean:
	rm -f $(OUT_DIR)/market.exe $(OBJS)