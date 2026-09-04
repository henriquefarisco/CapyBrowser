CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
CPPFLAGS ?=
LDFLAGS ?=
PYTHON ?= python3
BUILD_DIR := build
SRC := src/codecs/bmp_decode.c
URL_SRC := src/url/url_parse.c src/url/url_normalize.c src/url/origin.c
TEXT_SRC := src/text/html_entities.c src/text/html_tokenizer.c src/text/text_emit.c
IMAGE_SRC := src/codec/image_adapter.c
HTML_SRC := src/html/dom.c src/html/html_parse.c
CSS_SRC := src/css/css_parse.c
CASCADE_SRC := src/css/cascade.c
LAYOUT_SRC := src/layout/layout.c
DL_SRC := src/displaylist/display_list.c
DOWNLOAD_SRC := src/download/download.c
SESSION_SRC := src/session/session.c
FORMS_SRC := src/forms/forms.c
PAGE_SRC := src/page/page_render.c
# HTML parser reuses the C2 tokenizer + entity decoder.
HTML_DEPS := src/text/html_entities.c src/text/html_tokenizer.c
TEST_BIN := $(BUILD_DIR)/test_browser_codecs
URL_TEST_BIN := $(BUILD_DIR)/test_url
TEXT_TEST_BIN := $(BUILD_DIR)/test_text
IMAGE_TEST_BIN := $(BUILD_DIR)/test_image
HTML_TEST_BIN := $(BUILD_DIR)/test_html
CSS_TEST_BIN := $(BUILD_DIR)/test_css
CASCADE_TEST_BIN := $(BUILD_DIR)/test_cascade
LAYOUT_TEST_BIN := $(BUILD_DIR)/test_layout
DL_TEST_BIN := $(BUILD_DIR)/test_displaylist
DOWNLOAD_TEST_BIN := $(BUILD_DIR)/test_download
SESSION_TEST_BIN := $(BUILD_DIR)/test_session
FORMS_TEST_BIN := $(BUILD_DIR)/test_forms
PAGE_TEST_BIN := $(BUILD_DIR)/test_page
HARNESS_TEST_BIN := $(BUILD_DIR)/test_harness
VERSION_STR := $(shell cat VERSION)

# Reference host front-end ("CapyBrowse Text" CLI). Lives outside src/ and
# supplies the side effects the pure core must not perform (fetch/file read).
# Not part of the capy-browser-core ABI. See host/ and docs/compatibility.md.
HOST_LIB_SRC := host/fetch.c
HOST_SRC := host/capybrowse.c $(HOST_LIB_SRC)
HOST_DEPS := $(URL_SRC) $(TEXT_SRC) $(SESSION_SRC) $(DOWNLOAD_SRC)
HOST_INCLUDES := -Ihost -Isrc/url -Isrc/text -Isrc/session -Isrc/download
PAGE_INCLUDES := -Isrc/page -Isrc/html -Isrc/text -Isrc/css -Isrc/layout -Isrc/displaylist -Isrc/url
HOST_BIN := $(BUILD_DIR)/capybrowse
HOST_TEST_BIN := $(BUILD_DIR)/test_host

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
CAPY_PKG_SUMMARY_core := CapyBrowser portable static graphical browser core
CAPY_PKG_SUMMARY := $(CAPY_PKG_SUMMARY_$(STAGE))
CAPY_PKG_INSTALL_ROOT := /var/capypkg/$(CAPY_PKG_NAME)
CAPY_PKG_PROVIDES_ABI := capy-browser-core
CAPY_PKG_ABI_VERSION := 1
CAPY_PKG_CORE_ABI_MIN := 3
CAPY_PKG_CORE_ABI_MAX := 3
CAPY_PKG_KNOWN_GOOD := 1
CAPY_PKG_DEPENDS_text :=
CAPY_PKG_DEPENDS_core := org.capyos.codecs.image-basic
CAPY_PKG_DEPENDS := $(CAPY_PKG_DEPENDS_$(STAGE))
PUBLISH_URL_BASE ?= https://github.com/henriquefarisco/CapyBrowser/releases/download/v$(CAPY_PKG_VERSION)
CAPY_PKG_DIR := $(BUILD_DIR)/capypkg
CAPY_PKG_BIN := $(CAPY_PKG_DIR)/$(CAPY_PKG_NAME)-$(CAPY_PKG_VERSION).bin
CAPY_PKG_MANIFEST := $(CAPY_PKG_DIR)/$(CAPY_PKG_NAME).manifest
CAPY_PKG_INPUTS := VERSION $(shell find src docs -type f -print)

# Offline release output and opt-in remote publication verification. The remote
# gate deliberately has no default index URL: callers must name the exact index
# being promoted, so an old implicit pin cannot pass by accident.
RELEASE_CHECK_DIR ?= $(BUILD_DIR)/release-check
REMOTE_REPOSITORY ?= henriquefarisco/CapyBrowser
REMOTE_TAG ?= v$(VERSION_STR)
MODULES_INDEX_URL ?=

.PHONY: all clean lint lint-extra security security-extra test test-url test-text test-image test-html test-harness capybrowse capybrowse-net test-net test-host test-css test-cascade test-layout test-displaylist test-page test-download test-session test-forms validate version-check package package-clean release-check release-check-remote

all: test test-url test-text test-image test-html test-harness capybrowse test-host test-css test-cascade test-layout test-displaylist test-page test-download test-session test-forms

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

$(URL_TEST_BIN): $(URL_SRC) tests/test_url.c src/url/url_parse.h src/url/url_internal.h tests/harness/capy_test.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/url -Itests/harness $(URL_SRC) tests/test_url.c $(LDFLAGS) -o $@
	chmod 755 $@

test-url: $(URL_TEST_BIN)
	$(URL_TEST_BIN) tests/fixtures/url

$(TEXT_TEST_BIN): $(URL_SRC) $(TEXT_SRC) tests/test_text.c src/text/html_text.h src/text/html_entities.h src/text/html_tokenizer.h src/url/url_parse.h tests/harness/capy_test.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/url -Isrc/text -Itests/harness $(URL_SRC) $(TEXT_SRC) tests/test_text.c $(LDFLAGS) -o $@
	chmod 755 $@

test-text: $(TEXT_TEST_BIN)
	$(TEXT_TEST_BIN) tests/fixtures/html-to-text

$(IMAGE_TEST_BIN): $(IMAGE_SRC) tests/test_image_adapter.c src/codec/image_adapter.h src/adapter/host_adapter.h tests/harness/capy_test.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/adapter -Isrc/codec -Itests/harness $(IMAGE_SRC) tests/test_image_adapter.c $(LDFLAGS) -o $@
	chmod 755 $@

test-image: $(IMAGE_TEST_BIN)
	$(IMAGE_TEST_BIN)

$(HTML_TEST_BIN): $(HTML_SRC) $(HTML_DEPS) tests/test_html.c src/html/dom.h src/html/dom_internal.h src/text/html_tokenizer.h src/text/html_entities.h tests/harness/capy_test.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/html -Isrc/text -Isrc/url -Itests/harness $(HTML_SRC) $(HTML_DEPS) tests/test_html.c $(LDFLAGS) -o $@
	chmod 755 $@

test-html: $(HTML_TEST_BIN)
	$(HTML_TEST_BIN) tests/fixtures/dom

# Reference host text browser (offline: local file / stdin; no external deps).
capybrowse: $(HOST_BIN)
$(HOST_BIN): $(HOST_SRC) $(HOST_DEPS) host/capy_host.h src/text/html_text.h src/text/html_entities.h src/text/html_tokenizer.h src/url/url_parse.h src/session/session.h src/download/download.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_INCLUDES) $(HOST_SRC) $(HOST_DEPS) $(LDFLAGS) -o $@ && chmod 755 $@

# HTTPS-enabled build (opt-in; requires libcurl dev headers + -lcurl).
capybrowse-net: $(HOST_SRC) $(HOST_DEPS) host/capy_host.h src/text/html_text.h src/url/url_parse.h src/session/session.h src/download/download.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -DCAPY_HOST_HAVE_CURL $(HOST_INCLUDES) $(HOST_SRC) $(HOST_DEPS) $(LDFLAGS) -lcurl -o $(HOST_BIN) && chmod 755 $(HOST_BIN)

# Deterministic smoke for the libcurl-linked binary. It does not require
# external network access: launch the binary and render a local fixture.
test-net: capybrowse-net
	$(HOST_BIN) --help >/dev/null
	$(HOST_BIN) --file tests/fixtures/html-to-text/basic.in >/dev/null

# Host-side test for the reference front-end URL prep (HTTPS-first, fail-closed).
test-host: $(HOST_TEST_BIN) ; $(HOST_TEST_BIN)
$(HOST_TEST_BIN): tests/test_host.c $(HOST_LIB_SRC) $(URL_SRC) host/capy_host.h src/url/url_parse.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Ihost -Isrc/url -Itests/harness tests/test_host.c $(HOST_LIB_SRC) $(URL_SRC) $(LDFLAGS) -o $@ && chmod 755 $@

test-css: $(CSS_TEST_BIN) ; $(CSS_TEST_BIN) tests/fixtures/css
$(CSS_TEST_BIN): $(CSS_SRC) tests/test_css.c src/css/css_parse.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/css -Itests/harness $(CSS_SRC) tests/test_css.c $(LDFLAGS) -o $@ && chmod 755 $@

# Cascade test: matches the stylesheet onto the DOM (consumes css + html parse).
test-cascade: $(CASCADE_TEST_BIN) ; $(CASCADE_TEST_BIN) tests/fixtures/cascade
$(CASCADE_TEST_BIN): $(CASCADE_SRC) $(CSS_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_cascade.c src/css/cascade.h src/css/css_parse.h src/html/dom.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/css -Isrc/html -Isrc/text -Isrc/url -Itests/harness $(CASCADE_SRC) $(CSS_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_cascade.c $(LDFLAGS) -o $@ && chmod 755 $@

# Layout test: static block layout over DOM + computed styles (consumes M1/M2).
test-layout: $(LAYOUT_TEST_BIN) ; $(LAYOUT_TEST_BIN) tests/fixtures/layout
$(LAYOUT_TEST_BIN): $(LAYOUT_SRC) $(CASCADE_SRC) $(CSS_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_layout.c src/layout/layout.h src/css/cascade.h src/css/css_parse.h src/html/dom.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/layout -Isrc/css -Isrc/html -Isrc/text -Isrc/url -Itests/harness $(LAYOUT_SRC) $(CASCADE_SRC) $(CSS_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_layout.c $(LDFLAGS) -o $@ && chmod 755 $@

# Display-list test: versioned draw nodes from box tree + styles (consumes M1/M2/M3a + C1 for link URLs).
test-displaylist: $(DL_TEST_BIN) ; $(DL_TEST_BIN) tests/fixtures/display-list
$(DL_TEST_BIN): $(DL_SRC) $(LAYOUT_SRC) $(CASCADE_SRC) $(CSS_SRC) $(URL_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_displaylist.c src/displaylist/display_list.h src/layout/layout.h src/css/cascade.h src/css/css_parse.h src/html/dom.h src/url/url_parse.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/displaylist -Isrc/layout -Isrc/css -Isrc/html -Isrc/text -Isrc/url -Itests/harness $(DL_SRC) $(LAYOUT_SRC) $(CASCADE_SRC) $(CSS_SRC) $(URL_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_displaylist.c $(LDFLAGS) -o $@ && chmod 755 $@

# Production page-pipeline test: one API composes DOM -> CSS -> layout -> DL.
test-page: $(PAGE_TEST_BIN) ; $(PAGE_TEST_BIN) tests/fixtures/page
$(PAGE_TEST_BIN): $(PAGE_SRC) $(DL_SRC) $(LAYOUT_SRC) $(CASCADE_SRC) $(CSS_SRC) $(URL_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_page.c src/page/page_render.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) $(PAGE_INCLUDES) -Itests/harness $(PAGE_SRC) $(DL_SRC) $(LAYOUT_SRC) $(CASCADE_SRC) $(CSS_SRC) $(URL_SRC) $(HTML_SRC) $(HTML_DEPS) tests/test_page.c $(LDFLAGS) -o $@ && chmod 755 $@

# Download decision test: HTTPS-first + filename sanitization (consumes C1).
test-download: $(DOWNLOAD_TEST_BIN) ; $(DOWNLOAD_TEST_BIN)
$(DOWNLOAD_TEST_BIN): $(DOWNLOAD_SRC) $(URL_SRC) tests/test_download.c src/download/download.h src/url/url_parse.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/download -Isrc/url -Itests/harness $(DOWNLOAD_SRC) $(URL_SRC) tests/test_download.c $(LDFLAGS) -o $@ && chmod 755 $@

# Private-session test: request identity (UA, Referer policy, ephemeral flags; consumes C1).
test-session: $(SESSION_TEST_BIN) ; $(SESSION_TEST_BIN)
$(SESSION_TEST_BIN): $(SESSION_SRC) $(URL_SRC) tests/test_session.c src/session/session.h src/url/url_parse.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/session -Isrc/url -Itests/harness $(SESSION_SRC) $(URL_SRC) tests/test_session.c $(LDFLAGS) -o $@ && chmod 755 $@

# Form-submit test: x-www-form-urlencoded GET/POST request building (consumes C1).
test-forms: $(FORMS_TEST_BIN) ; $(FORMS_TEST_BIN)
$(FORMS_TEST_BIN): $(FORMS_SRC) $(URL_SRC) tests/test_forms.c src/forms/forms.h src/url/url_parse.h tests/harness/capy_test.h | $(BUILD_DIR) ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/forms -Isrc/url -Itests/harness $(FORMS_SRC) $(URL_SRC) tests/test_forms.c $(LDFLAGS) -o $@ && chmod 755 $@

# Syntax-only coverage for the CSS, cascade, host, layout, display-list, download, session and forms modules (tab-free via
# the semicolon recipe form); folded into lint as a prerequisite.
lint-extra: ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/css -fsyntax-only $(CSS_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/css -Isrc/html -fsyntax-only $(CASCADE_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_INCLUDES) -fsyntax-only $(HOST_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/layout -Isrc/css -Isrc/html -fsyntax-only $(LAYOUT_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/displaylist -Isrc/layout -Isrc/css -Isrc/html -Isrc/url -fsyntax-only $(DL_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/download -Isrc/url -fsyntax-only $(DOWNLOAD_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/session -Isrc/url -fsyntax-only $(SESSION_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/forms -Isrc/url -fsyntax-only $(FORMS_SRC)
lint: lint-extra
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/url -fsyntax-only $(URL_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/url -Isrc/text -fsyntax-only $(TEXT_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/adapter -Isrc/codec -fsyntax-only $(IMAGE_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/html -Isrc/text -Isrc/url -fsyntax-only $(HTML_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PAGE_INCLUDES) -fsyntax-only $(PAGE_SRC)
	git -c core.whitespace=cr-at-eol diff --check

# Hardened syntax-only coverage for the CSS, cascade, host, layout, display-list, download, session and forms modules.
security-extra: ; $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/css -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(CSS_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/css -Isrc/html -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(CASCADE_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_INCLUDES) -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(HOST_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/layout -Isrc/css -Isrc/html -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(LAYOUT_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/displaylist -Isrc/layout -Isrc/css -Isrc/html -Isrc/url -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(DL_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/download -Isrc/url -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(DOWNLOAD_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/session -Isrc/url -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(SESSION_SRC) && $(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/forms -Isrc/url -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(FORMS_SRC)
security: security-extra
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/url -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(URL_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/url -Isrc/text -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(TEXT_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/adapter -Isrc/codec -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(IMAGE_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/html -Isrc/text -Isrc/url -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(HTML_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PAGE_INCLUDES) -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(PAGE_SRC)

version-check:
	$(PYTHON) tools/release_gate.py metadata --root .

validate: lint security test test-url test-text test-image test-html test-harness capybrowse test-host test-css test-cascade test-layout test-displaylist test-page test-download test-session test-forms version-check

# package: build the artefact + manifest the in-tree CapyOS adapter
# consumes (see CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
package: $(CAPY_PKG_MANIFEST)

$(CAPY_PKG_BIN): $(CAPY_PKG_INPUTS) | $(BUILD_DIR)
	@mkdir -p $(CAPY_PKG_DIR)
	@tar --format=ustar --owner=0 --group=0 --numeric-owner \
	     --mtime='@0' --sort=name \
	     -cf $@ src docs VERSION
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
	  echo "provides_abi=$(CAPY_PKG_PROVIDES_ABI)" ; \
	  echo "abi_version=$(CAPY_PKG_ABI_VERSION)" ; \
	  echo "core_abi_min=$(CAPY_PKG_CORE_ABI_MIN)" ; \
	  echo "core_abi_max=$(CAPY_PKG_CORE_ABI_MAX)" ; \
	  echo "known_good=$(CAPY_PKG_KNOWN_GOOD)" ; \
	  echo "depends=$(CAPY_PKG_DEPENDS)" ; \
	  echo "---" ; \
	} > $@
	@echo "[package] manifest: $@"

package-clean:
	rm -rf $(CAPY_PKG_DIR)

# Full offline release gate: strict validation, libcurl-linked smoke, both
# package identities, manifest/hash checks and byte-for-byte reproducibility.
# Remote tag/assets/index checks stay in release-check-remote below.
release-check:
	$(MAKE) validate
	$(MAKE) test-net
	$(MAKE) BUILD_DIR="$(RELEASE_CHECK_DIR)/pass1" package-clean
	$(MAKE) BUILD_DIR="$(RELEASE_CHECK_DIR)/pass1" package STAGE=text
	$(MAKE) BUILD_DIR="$(RELEASE_CHECK_DIR)/pass1" package STAGE=core
	$(MAKE) BUILD_DIR="$(RELEASE_CHECK_DIR)/pass2" package-clean
	$(MAKE) BUILD_DIR="$(RELEASE_CHECK_DIR)/pass2" package STAGE=text
	$(MAKE) BUILD_DIR="$(RELEASE_CHECK_DIR)/pass2" package STAGE=core
	$(PYTHON) tools/release_gate.py offline --root . \
		--first-dir "$(RELEASE_CHECK_DIR)/pass1/capypkg" \
		--second-dir "$(RELEASE_CHECK_DIR)/pass2/capypkg" \
		--repository "$(REMOTE_REPOSITORY)"

# Opt-in post-publication gate. Example:
#   make release-check-remote MODULES_INDEX_URL=https://.../modules-index.txt
release-check-remote: release-check
	@if [ -z "$(MODULES_INDEX_URL)" ]; then \
		echo "[release-remote] FAIL: MODULES_INDEX_URL is required" >&2; \
		exit 2; \
	fi
	$(PYTHON) tools/release_gate.py remote --root . \
		--artifacts-dir "$(RELEASE_CHECK_DIR)/pass1/capypkg" \
		--repository "$(REMOTE_REPOSITORY)" \
		--tag "$(REMOTE_TAG)" \
		--index-url "$(MODULES_INDEX_URL)"

clean:
	rm -rf $(BUILD_DIR)
