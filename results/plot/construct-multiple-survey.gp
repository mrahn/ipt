# Plot: construction time for multiple-survey scenarios.
set encoding utf8
set terminal cairolatex pdf size 6.0in,3.8in
set output 'generated/construct-multiple-survey.tex'

set ylabel 'ns / point (construction)'
set y2label 'Mpoint / s'
set y2label offset -1.0,0
set y2tics format '%.0f'
set link y2 via 1000.0/y inverse 1000.0/y
set yrange [0:30]
set grid ytics
set key below center horizontal maxcols 4

set style data histogram
set style histogram errorbars gap 1.5 lw 1
set style fill pattern border rgb 'black'
set boxwidth 0.8

set xtics rotate by -35

set linetype 1 lc rgb '#000000'
set linetype 2 lc rgb '#202020'
set linetype 3 lc rgb '#404040'
set linetype 4 lc rgb '#606060'
set linetype 5 lc rgb '#808080'
set linetype 6 lc rgb '#a0a0a0'
set linetype 7 lc rgb '#303030'
set linetype 8 lc rgb '#707070'

plot 'results/plot/construct-multiple-survey.tsv' \
         u 2:3:4:xtic(1)    title 'IPT'                     lt 1 fs pattern 2, \
     ''  u 5:6:7           title 'SortedPoints'            lt 2 fs pattern 4, \
     ''  u 8:9:10          title 'DenseBitset'             lt 3 fs pattern 5, \
     ''  u 11:12:13        title 'BlockBitmap'             lt 4 fs pattern 6, \
     ''  u 14:15:16        title 'Roaring'                 lt 5 fs pattern 7, \
     ''  u 17:18:19        title 'LexRun'                  lt 6 fs pattern 8, \
     ''  u 20:21:22        title 'RowCSR\textsubscript{1}' lt 7 fs pattern 9, \
     ''  u 23:24:25        title 'RowCSR\textsubscript{2}' lt 8 fs pattern 10
