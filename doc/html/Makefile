# ==============================================================================
.SUFFIXES : .html .xml .gif .jpg .png .css .js .faq

include Files
# Files to build locally (not found from xml) (also temp files)
FILES_LOCAL=readme.html input/default.html faq/index.html
# Other temp files to remove
# (XXX Crap name xcixml - XSL should use different entries depending
# automatically)
FILES_TMP=dest.tar.gz readme.xml input/default.xcixml input/default.xml input/basic.tmp input/tree.tmp faq/index.xml

# ------------------------------------------------------------------------------

SSHUSER= $(USER)

build: depends $(FILES_DEST) $(FILES_LOCAL) dest.tar.gz

$(FILES_DEST): ss/wrap.xsl

depends:
	@perl files.pl > Files

readme.xml: ../../source/XCSoarV4-README.txt
	perl wrap.pl Readme < ../../source/XCSoarV4-README.txt > readme.xml

faq/index.xml: faq/*.faq faq2index.pl
	perl faq2index.pl > $@

input/default.xcixml: ../../source/Common/Data/Input/default.xci xci2xml.pl
	cp ../../source/Common/Data/Input/default.xci input/default.xci
	perl xci2xml.pl ../../source/Common/Data/Input/default.xci > $@

clean: depends
	@-rm $(FILES_CLEAN) $(FILES_LOCAL) $(FILES_TMP) dest.tar.gz >/dev/null 2>/dev/null

dest.tar.gz: $(FILES_DEST) $(FILES_LOCAL)
	tar cvzf dest.tar.gz $(FILES_DEST) $(FILES_LOCAL)

nightlycvs:
	ssh $(SSHUSER)@shell.sourceforge.net "cvs -d:pserver:anonymous@cvs1:/cvsroot/xcsoar export -Dtomorrow source && tar czvf /home/groups/x/xc/xcsoar/htdocs/xcsoar-nightlyCVS.tar.gz source && rm -Rf source"

install: build nightlycvs
	scp dest.tar.gz $(SSHUSER)@shell.sourceforge.net:/home/groups/x/xc/xcsoar/htdocs/
	ssh $(SSHUSER)@shell.sourceforge.net "cd /home/groups/x/xc/xcsoar/htdocs/; tar xvzf dest.tar.gz; chmod -R g+rwX /home/groups/x/xc/xcsoar/htdocs"

installstyle: 
	scp style.css $(SSHUSER)@shell.sourceforge.net:/home/groups/x/xc/xcsoar/htdocs/

%.xml : %.xcixml ss/xci.xsl ss/xci_tree.xsl ss/xci_basic.xsl
	xsltproc ss/xci_basic.xsl $< > input/basic.tmp
	xsltproc ss/xci_tree.xsl $< > input/tree.tmp
	xsltproc ss/xci.xsl $< > $@

%.html : %.xml ss/wrap.xsl
	xsltproc ss/wrap.xsl $< > $@

%.xml : %.faq ss/faq.xsl
	xsltproc ss/faq.xsl $< > $@

manual.ps : manual.fo
	/Users/scottp/src/fop-0.20.5/fop.sh manual.fo manual.ps

manual.pdf : manual.fo
	/Users/scottp/src/fop-0.20.5/fop.sh manual.fo manual.pdf

manual.fo : manual.xhtml
	xsltproc ss/xhtml2fo.xsl manual.xhtml > manual.fo

manual.xhtml : depends $(FILES_GEN) manual.xml readme.html input/default.html faq/index.html
	xsltproc ss/manual2temp.xsl manual.xml > temp.xml
	xsltproc ss/manual2xhtml.xsl temp.xml > manual.xhtml

