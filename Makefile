
PHP_CONFIG = php-config
PHP_INCLUDE = $(shell $(PHP_CONFIG) --includes)
PHP_LIBS = $(shell $(PHP_CONFIG) --libs)
PHP_LDFLAGS = $(shell $(PHP_CONFIG) --ldflags)
EXTENSION_DIR = $(shell $(PHP_CONFIG) --extension-dir)

GO_SRC = src/go/lib.go
GO_LIB = libphpgo.v1.so
GO_HEADER = src/go/libphpgo.h

C_SRC = src/c/php_phpgo.c
C_OBJ = php_phpgo.o
EXTENSION = phpgo.so

all: $(EXTENSION)

$(GO_LIB): $(GO_SRC)
	go build -o $(GO_LIB) -buildmode=c-shared $(GO_SRC)
	mv libphpgo.v1.h src/go/libphpgo.h

$(EXTENSION): $(GO_LIB) $(C_SRC)
	gcc -fPIC -shared -o $(EXTENSION) $(C_SRC) $(PHP_INCLUDE) -L. -lphpgo.v1 -Wl,-rpath,.

clean:
	rm -f *.so *.o *.h src/go/*.h
