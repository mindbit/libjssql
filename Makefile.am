SUBDIRS = src
AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4

README: README.md
	pandoc -f commonmark -t plain $< -o $@

CLEANFILES = README

ChangeLog:
	git log --oneline --decorate --color

dist: ChangeLog

.PHONY: ChangeLog
