# Build the report PDF (needs a TeX distribution with vntex; Overleaf: pdfLaTeX)
TARGET = HK261_CO3053_CCAS2_2353122_2352844_2352849

all: $(TARGET).pdf main.pdf

$(TARGET).pdf: $(TARGET).tex references.bib sections/*.tex
	latexmk -pdf -interaction=nonstopmode $(TARGET).tex

main.pdf: main.tex $(TARGET).tex references.bib sections/*.tex
	latexmk -pdf -interaction=nonstopmode main.tex

clean:
	latexmk -C $(TARGET).tex
	latexmk -C main.tex

.PHONY: all clean
