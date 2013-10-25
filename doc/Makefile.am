man_MANS = gamemus.1
man_MANS += dro2txt.1

EXTRA_DIST = gamemus.xml
EXTRA_DIST += dro2txt.xml
EXTRA_DIST += camoto.xsl

# Also distribute the converted man pages so users don't need DocBook installed
EXTRA_DIST += $(man_MANS)

HTML_MAN = gamemus.html
HTML_MAN += dro2txt.html

.PHONY: html

html: $(HTML_MAN)

MAINTAINERCLEANFILES = $(man_MANS) $(HTML_MAN)

%.1: %.xml camoto.xsl
	xmlto --skip-validation man -m $(<D)/camoto.xsl $<

%.html: %.xml camoto.xsl
	xmlto --skip-validation html-nochunks -m $(<D)/camoto.xsl $<
