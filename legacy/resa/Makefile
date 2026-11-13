.DEFAULT_GOAL := app

GO ?= go
GO_CACHE ?= /tmp/universal-mouse-legacy-go-cache
ARGS ?= list

.PHONY: app build test desktop preview desktop-check install

app:
	GOCACHE="$(GO_CACHE)" GOPROXY=off $(GO) run -buildvcs=false ./cmd/resa $(ARGS)

build:
	GOCACHE="$(GO_CACHE)" GOPROXY=off $(GO) build -buildvcs=false -o bin/resa ./cmd/resa

test:
	GOCACHE="$(GO_CACHE)" GOPROXY=off $(GO) test ./...
	PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -m unittest discover -s internal/device -p '*_test.py'

install:
	pnpm install --frozen-lockfile

desktop: build
	pnpm desktop

preview:
	pnpm preview

desktop-check:
	pnpm check
