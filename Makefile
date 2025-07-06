CC = clang
CFLAGS_COMMON = -std=c23 -Iinclude
SRCDIR = src
BUILDDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS_DEBUG = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/debug/%.o)
OBJECTS_RELEASE = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/release/%.o)
PROGRAM = network_info

CFLAGS_DEBUG = $(CFLAGS_COMMON) -g -O0 -Wall -Wextra -Wpedantic \
               -Wconversion -Wsign-conversion -Wcast-align -Wformat=2 \
               -Wuninitialized -Wmissing-prototypes -Wstrict-prototypes \
               -Wold-style-definition -Wredundant-decls -Wnested-externs \
               -Winline -Wdisabled-optimization -fsanitize=address \
               -fsanitize=undefined -fno-omit-frame-pointer -DDEBUG

CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native -flto \
                 -ffunction-sections -fdata-sections

LDFLAGS_RELEASE = -Wl,-O1 -Wl,--as-needed -Wl,--gc-sections

# Create build directories
$(BUILDDIR)/debug:
	mkdir -p $(BUILDDIR)/debug

$(BUILDDIR)/release:
	mkdir -p $(BUILDDIR)/release

# Debug build
$(BUILDDIR)/debug/%.o: $(SRCDIR)/%.c | $(BUILDDIR)/debug
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(BUILDDIR)/release/%.o: $(SRCDIR)/%.c | $(BUILDDIR)/release
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

debug: $(OBJECTS_DEBUG)
	$(CC) $(CFLAGS_DEBUG) $^ -o $(BUILDDIR)/debug/$(PROGRAM)

release: $(OBJECTS_RELEASE)
	$(CC) $(CFLAGS_RELEASE) $(LDFLAGS_RELEASE) $^ -o $(BUILDDIR)/release/$(PROGRAM)

clean:
	rm -rf $(BUILDDIR)

run-debug: debug
	./$(BUILDDIR)/debug/$(PROGRAM)

run-release: release
	./$(BUILDDIR)/release/$(PROGRAM)

.PHONY: debug release clean run-debug run-release
