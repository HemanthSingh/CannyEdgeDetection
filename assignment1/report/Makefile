SOURCES = conclusion.tex discussion.tex dsp.tex integration.tex main.tex neon.tex

all:	main.pdf

main.pdf:	$(SOURCES)
	pdflatex main.tex && bibtex main && pdflatex main.tex && pdflatex main.tex

clean:
	rm -f *.aux main.log main.bbl main.toc
