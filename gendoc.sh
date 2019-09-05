#!/bin/bash
pandoc -M pagetitle="WT* framework" -f markdown -t html -c ./css/markedapp-byword.css -s ./README.md -o ./documentation.html 

