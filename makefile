# Имя компилятора (точный путь к 32-битному MinGW 11.2)
CXX = c:/mingw32/bin/g++.exe
CC = c:/mingw32/bin/gcc.exe

# Флаги компилятора
CXXFLAGS = -Wall -O2 -DCURL_STATICLIB -D_WIN32_WINNT=0x0501
CFLAGS = -Wall -O2 -D_WIN32_WINNT=0x0501

# Пути к заголовочным файлам Curl
INCLUDES = -I./curl/include

# Пути к библиотекам Curl
LDFLAGS = -L./curl/lib

# Библиотеки для линковки (ДОБАВЛЕН ФЛАГ --allow-multiple-definition В НАЧАЛО)
LIBS = -Wl,--allow-multiple-definition -Wl,--subsystem,windows:5.01 -static -lcurl -lngtcp2_crypto_openssl -lngtcp2 -lnghttp3 -lnghttp2 -lssh2 -lssl -lcrypto -lgsasl -lidn2 -lzstd -lbrotlidec -lbrotlicommon -lz -lws2_32 -lwldap32 -lcrypt32 -lnormaliz -lcomctl32 -lshell32 -lgdi32 -luser32 -mwindows

# Папка для готовой сборки
OUT_DIR = out

# Имя готового исполняемого файла
TARGET = $(OUT_DIR)/market.exe

# Исходные файлы
SRCS = main.cpp xp_compat.c
OBJS = main.o xp_compat.o

all: $(TARGET)

main.o: main.cpp
	$(CXX) -c main.cpp -o main.o $(CXXFLAGS) $(INCLUDES)

xp_compat.o: xp_compat.c xp_compat.h
	$(CC) -c xp_compat.c -o xp_compat.o $(CFLAGS)

$(TARGET): $(OBJS)
	@echo Building in $(OUT_DIR)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS) $(INCLUDES) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OUT_DIR)/market.exe $(OBJS)