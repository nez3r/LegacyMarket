# Имя компилятора (убедись, что новый 32-битный MinGW прописан в PATH, либо укажи полный путь, например: C:/mingw32/bin/g++)
CXX = g++

# Флаги компилятора (убрали -m32, так как компилятор уже x86)
CXXFLAGS = -Wall -O2

# Пути к заголовочным файлам Curl
INCLUDES = -I./curl/include

# Пути к библиотекам Curl
LDFLAGS = -L./curl/lib

# Библиотеки для линковки (включая интерфейс Windows)
LIBS = -lcurl -lws2_32 -lwldap32 -lcrypt32 -lcomctl32 -mwindows

# Папка для готовой сборки
OUT_DIR = out

# Имя готового исполняемого файла внутри папки out
TARGET = $(OUT_DIR)/market.exe

# Исходные файлы
SRCS = main.cpp

# Правило по умолчанию
all: $(TARGET)

# Правило для сборки исполняемого файла
$(TARGET): $(SRCS)
	@mkdir -p $(OUT_DIR)
	$(CXX) $(SRCS) -o $(TARGET) $(CXXFLAGS) $(INCLUDES) $(LDFLAGS) $(LIBS)

# Очистка папки сборки
clean:
	rm -f $(OUT_DIR)/*