SOURCES = conclusion.tex discussion.tex dsp.tex implementation.tex main.tex neon.tex analysis.tex introduction.tex profiling.tex flag.tex

all:	main.pdf appendix.pdf

main.pdf:	$(SOURCES)
	pdflatex main.tex && pdflatex main.tex

appendix.pdf:	appendix.tex
	pdflatex appendix.tex

clean:
	rm -f *.aux main.log main.bbl main.toc
