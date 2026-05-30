CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
CPPFLAGS ?=
LDFLAGS ?=
BUILD_DIR := build
SRC := src/codecs/bmp_decode.c
TEST_BIN := $(BUILD_DIR)/test_browser_codecs
HARNESS_TEST_BIN := $(BUILD_DIR)/test_harness
VERSION_STR := $(shell cat VERSION)

# capypkg packaging (Etapa 9 alpha receiver).
# STAGE selects the canonical package name (see docs/compatibility.md,
# "Publishing as a Capy package"):
#   text -> org.capyos.browser.text  (Etapa 6, CapyBrowse Text)
#   core -> org.capyos.browser.core  (Etapa 7, static graphical core)
STAGE ?= core
CAPY_PKG_NAME_text := org.capyos.browser.text
CAPY_PKG_NAME_core := org.capyos.browser.core
CAPY_PKG_NAME := $(CAPY_PKG_NAME_$(STAGE))
ifeq ($(CAPY_PKG_NAME),)
$(error invalid STAGE '$(STAGE)': use 'text' (Etapa 6) or 'core' (Etapa 7))
endif
CAPY_PKG_VERSION := $(VERSION_STR)
CAPY_PKG_SUMMARY_text := CapyBrowse Text portable browser-core (HTML-to-text)
CAPY_PKG_SUMMARY_core := CapyBrowser portable browser-core stub
CAPY_PKG_SUMMARY := $(CAPY_PKG_SUMMARY_$(STAGE))
CAPY_PKG_INSTALL_ROOT := /var/capypkg/$(CAPY_PKG_NAME)
CAPY_PKG_DEPENDS := org.capyos.codecs.image-basic
PUBLISH_URL_BASE ?= https://github.com/henriquefarisco/CapyBrowser/releases/download/v$(CAPY_PKG_VERSION)
CAPY_PKG_DIR := $(BUILD_DIR)/capypkg
CAPY_PKG_BIN := $(CAPY_PKG_DIR)/$(CAPY_PKG_NAME)-$(CAPY_PKG_VERSION).bin
CAPY_PKG_MANIFEST := $(CAPY_PKG_DIR)/$(CAPY_PKG_NAME).manifest

.PHONY: all clean lint security test test-harness validate version-check package package-clean

all: test test-harness

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): $(SRC) tests/test_browser_codecs.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/codecs $(SRC) tests/test_browser_codecs.c $(LDFLAGS) -o $@
	chmod 755 $@

test: $(TEST_BIN)
	$(TEST_BIN)

$(HARNESS_TEST_BIN): tests/test_harness.c tests/harness/capy_test.h tests/harness/capy_determinism.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/harness tests/test_harness.c $(LDFLAGS) -o $@
	chmod 755 $@

test-harness: $(HARNESS_TEST_BIN)
	$(HARNESS_TEST_BIN)

lint:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(SRC)
	git diff --check

security:
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(SRC)

version-check:
	test -n "$(VERSION_STR)"
	grep -q "Version: $(VERSION_STR)" README.md

validate: lint security test test-harness version-check

# package: build the artefact + manifest the in-tree CapyOS adapter
# consumes (see CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
package: $(CAPY_PKG_MANIFEST)

$(CAPY_PKG_BIN): $(SRC) | $(BUILD_DIR)
	@mkdir -p $(CAPY_PKG_DIR)
	@tar --format=ustar --owner=0 --group=0 --numeric-owner \
	     --mtime='@0' --sort=name \
	     -cf $@ src docs VERSION 2>/dev/null || \
	  tar -cf $@ src docs VERSION
	@echo "[package] $@"

$(CAPY_PKG_MANIFEST): $(CAPY_PKG_BIN)
	@SHA=$$(shasum -a 256 $(CAPY_PKG_BIN) 2>/dev/null | awk '{print $$1}' | tr 'A-F' 'a-f') ; \
	if [ -z "$$SHA" ]; then SHA=$$(sha256sum $(CAPY_PKG_BIN) | awk '{print $$1}' | tr 'A-F' 'a-f'); fi ; \
	SIZE=$$(wc -c < $(CAPY_PKG_BIN) | tr -d ' ') ; \
	URL="$(PUBLISH_URL_BASE)/$(CAPY_PKG_NAME)-$(CAPY_PKG_VERSION).bin" ; \
	{ \
	  echo "name=$(CAPY_PKG_NAME)" ; \
	  echo "version=$(CAPY_PKG_VERSION)" ; \
	  echo "summary=$(CAPY_PKG_SUMMARY)" ; \
	  echo "payload_url=$$URL" ; \
	  echo "payload_sha256=$$SHA" ; \
	  echo "payload_size=$$SIZE" ; \
	  echo "install_root=$(CAPY_PKG_INSTALL_ROOT)" ; \
	  echo "depends=$(CAPY_PKG_DEPENDS)" ; \
	  echo "---" ; \
	} > $@
	@echo "[package] manifest: $@"

package-clean:
	rm -rf $(CAPY_PKG_DIR)

clean:
	rm -rf $(BUILD_DIR)
