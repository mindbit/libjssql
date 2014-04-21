SUBDIRS = src
AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4

ChangeLog:
	git log --oneline --decorate --color

dist: ChangeLog

.PHONY: ChangeLog