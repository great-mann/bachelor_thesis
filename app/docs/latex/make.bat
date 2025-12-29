SET TEXINPUTS=./YY_Styles//;.
pdflatex -aux-directory=./ZZ_Temp main.tex
bibtex -include-directory="./ZZ_Temp/" ZZ_Temp/main
pause

