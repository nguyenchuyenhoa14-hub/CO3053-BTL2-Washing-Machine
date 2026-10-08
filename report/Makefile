# Build the report PDF (needs a TeX distribution with vntex; Overleaf: pdfLaTeX)
main.pdf: main.tex references.bib sections/*.tex
	latexmk -pdf -interaction=nonstopmode main.tex

clean:
	latexmk -C

.PHONY: clean
