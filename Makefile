.PHONY: all check clean codespell coverage format help install install-dev lint test
.PHONY: cpp-check cpp-format cpp-tidy
.PHONY: go-check go-clean go-coverage go-format go-install go-install-dev go-lint go-test
.PHONY: python-check python-clean python-coverage python-format python-install python-install-dev python-lint python-test
.DEFAULT_GOAL := help

# Define a helper for checking command existence
check-command = @if ! command -v $(1) > /dev/null; then \
		echo "Error: '$(1)' not found. Please run 'make install-dev' to install development tools, or ensure it is in your PATH."; \
		exit 1; \
	fi

# Ref that cpp-format/cpp-check compare against (files changed since BASE)
BASE ?= $(shell git rev-parse --verify -q origin/main > /dev/null && echo origin/main || echo main)
CLANG_FORMAT ?= clang-format
CLANG_TIDY ?= clang-tidy
BUILD_DIR ?= build

GIT_CLANG_FORMAT ?= git-clang-format

# Non-vendored C/C++ files: tracked ones changed vs BASE get only their changed lines formatted
# (git clang-format), so legacy files with small edits aren't reformatted wholesale; new
# (untracked) files are formatted in full.
CPP_GLOBS = '*.c' '*.cc' '*.cpp' '*.h' '*.hpp'
CPP_FILTER = grep -v -E '(^|/)(protocol|helics|cparse)/|^src/deps/|^test/doctest\.h$$'
CPP_TRACKED = git diff --name-only --diff-filter=d $(BASE) -- $(CPP_GLOBS) | $(CPP_FILTER)
CPP_UNTRACKED = git ls-files -o --exclude-standard -- $(CPP_GLOBS) | $(CPP_FILTER)

help:
	@echo "Available targets:"
	@echo ""
	@echo "Development:"
	@echo "  all         - Run all tools (format, lint, test)"
	@echo "  check       - Run linters without fixing (for CI)"
	@echo "  codespell   - Check spelling repo-wide (no fixes)"
	@echo "  coverage    - Run unit tests with coverage (python, go)"
	@echo "  format      - Format code (ruff, golangci-lint, clang-format on changed files)"
	@echo "  lint        - Lint code and fix issues (ruff, golangci-lint, codespell)"
	@echo "  test        - Run unit tests (python, go; C++ tests: docker/test/run.sh ctest)"
	@echo ""
	@echo "C++ (changed files vs BASE=$(BASE)):"
	@echo "  cpp-format  - clang-format changed lines (tracked) and new files, in place"
	@echo "  cpp-check   - Fail if changed lines or new files are not clang-formatted"
	@echo "  cpp-tidy    - Run clang-tidy on non-vendored sources (needs $(BUILD_DIR)/compile_commands.json)"
	@echo ""
	@echo "Installation:"
	@echo "  install     - Install runtime dependencies (go modules)"
	@echo "  install-dev - Install development tools (ruff, codespell, vulture, golangci-lint)"
	@echo ""
	@echo "Cleanup:"
	@echo "  clean       - Clean lint/test artifacts"
	@echo ""
	@echo "Help:"
	@echo "  help        - Show this help message"

all: format lint test

check: python-check go-check codespell cpp-check
test: python-test go-test
lint: python-lint go-lint
format: python-format go-format cpp-format
install: python-install go-install
install-dev: python-install-dev go-install-dev
clean: python-clean go-clean
coverage: python-coverage go-coverage

## ---------------------------------- Python ----------------------------------

python-check:
	@$(MAKE) -C src/pybennu check

python-install:
	@$(MAKE) -C src/pybennu install

python-install-dev:
	@$(MAKE) -C src/pybennu install-dev

python-format:
	@$(MAKE) -C src/pybennu format

python-lint:
	@$(MAKE) -C src/pybennu lint

python-test:
	@$(MAKE) -C src/pybennu test

python-clean:
	@$(MAKE) -C src/pybennu clean

python-coverage:
	@$(MAKE) -C src/pybennu coverage

## ---------------------------------- Go ----------------------------------

go-check:
	@$(MAKE) -C src/gobennu check

go-install:
	@$(MAKE) -C src/gobennu install

go-install-dev:
	@$(MAKE) -C src/gobennu install-dev

go-format:
	@$(MAKE) -C src/gobennu format

go-lint:
	@$(MAKE) -C src/gobennu lint

go-test:
	@$(MAKE) -C src/gobennu test

go-clean:
	@$(MAKE) -C src/gobennu clean

go-coverage:
	@$(MAKE) -C src/gobennu coverage

## ---------------------------------- Spelling ----------------------------------

# Repo-wide spell check using .codespellrc (skips vendored code)
codespell:
	$(call check-command,codespell)
	codespell --config .codespellrc .

## ---------------------------------- C++ ----------------------------------

# Format changed lines of tracked files and all of new files, in place
cpp-format:
	$(call check-command,$(CLANG_FORMAT))
	$(call check-command,$(GIT_CLANG_FORMAT))
	@tracked=$$( $(CPP_TRACKED) || true); untracked=$$( $(CPP_UNTRACKED) || true); \
	if [ -n "$$tracked" ]; then $(GIT_CLANG_FORMAT) --binary $(CLANG_FORMAT) -f $(BASE) -- $$tracked; fi; \
	if [ -n "$$untracked" ]; then $(CLANG_FORMAT) -i $$untracked; fi; \
	if [ -z "$$tracked$$untracked" ]; then echo "cpp-format: no changed C/C++ files vs $(BASE)"; fi

# Check formatting of changed lines (tracked) and new files (untracked)
cpp-check:
	$(call check-command,$(CLANG_FORMAT))
	$(call check-command,$(GIT_CLANG_FORMAT))
	@tracked=$$( $(CPP_TRACKED) || true); untracked=$$( $(CPP_UNTRACKED) || true); rc=0; \
	if [ -n "$$tracked" ]; then \
		out=$$($(GIT_CLANG_FORMAT) --binary $(CLANG_FORMAT) --diff $(BASE) -- $$tracked); st=$$?; \
		if echo "$$out" | grep -q '^diff '; then echo "$$out"; rc=1; \
		elif [ $$st -ne 0 ]; then echo "cpp-check: $(GIT_CLANG_FORMAT) failed (exit $$st)"; rc=1; fi; \
	fi; \
	if [ -n "$$untracked" ]; then $(CLANG_FORMAT) --dry-run --Werror $$untracked || rc=1; fi; \
	if [ -z "$$tracked$$untracked" ]; then echo "cpp-check: no changed C/C++ files vs $(BASE)"; fi; \
	exit $$rc

# clang-tidy on non-vendored sources; configure with: cmake -S . -B build
cpp-tidy:
	$(call check-command,$(CLANG_TIDY))
	@test -f $(BUILD_DIR)/compile_commands.json || { echo "Error: $(BUILD_DIR)/compile_commands.json missing; run 'cmake -S . -B $(BUILD_DIR)' first."; exit 1; }
	git ls-files -- 'src/bennu/*.cpp' 'src/bennu/*.cc' 'test/*.cpp' | \
		grep -v -E '(^|/)(protocol|helics|cparse)/' | \
		xargs $(CLANG_TIDY) -p $(BUILD_DIR) --quiet --extra-arg=-std=c++17
