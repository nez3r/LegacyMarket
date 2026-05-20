# Имя компилятора (точный путь к 32-битному MinGW 11.2)
CXX = c:/mingw32/bin/g++.exe
CC = c:/mingw32/bin/gcc.exe

# Флаги компилятора
CXXFLAGS = -Wall -O2 -D_WIN32_WINNT=0x0501
CFLAGS = -Wall -O2 -D_WIN32_WINNT=0x0501

# Библиотеки для линковки (без cURL - используем curl.exe)
LIBS = -Wl,--subsystem,windows:5.01 -static -lcomctl32 -lshell32 -lgdi32 -luser32 -mwindows

# Папка для готовой сборки
OUT_DIR = out

# Имя готового исполняемого файла
TARGET = $(OUT_DIR)/market.exe

# Исходные файлы
SRCS = main.cpp
OBJS = main.o

all: $(TARGET)

main.o: main.cpp
	$(CXX) -c main.cpp -o main.o $(CXXFLAGS)

$(TARGET): $(OBJS)
	@echo Building in $(OUT_DIR)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS) $(LIBS)
	@echo Build complete!

clean:
	rm -f $(OUT_DIR)/market.exe $(OBJS)