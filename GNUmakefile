# PKDCLX 1.1.1
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0

CFLAGS=-O3 -std=c89 -pedantic -Wall -Wextra

all: dclzip

dclzip: dclzip.c pkdcl.c pkdcl.h
	@eval echo "$${CC:-$(CC)}" $${CFLAGS:-$(CFLAGS)} -o $@ dclzip.c pkdcl.c $${LDFLAGS:-$(LDFLAGS)}
	@eval "$${CC:-$(CC)}" $${CFLAGS:-$(CFLAGS)} -o $@ dclzip.c pkdcl.c $${LDFLAGS:-$(LDFLAGS)}

clean:
	@test -f dclzip && eval echo "$${RM:-$(RM)}" dclzip || :
	@test -f dclzip && eval "$${RM:-$(RM)}" dclzip || :

.PHONY: clean all
