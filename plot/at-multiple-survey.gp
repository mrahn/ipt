# Plot: at_random absolute performance for multiple-survey scenarios.
set encoding utf8
set terminal cairolatex pdf size 6.0in,3.8in
set output 'generated/at-multiple-survey.tex'

set ylabel 'ns / \texttt{at}'
set y2label 'M\texttt{at} / s'
set y2label offset -1.0,0
set y2tics format '%.1f'
set link y2 via 1000.0/y inverse 1000.0/y
set yrange [0:250]
set grid ytics
set key below center horizontal maxcols 4

limit = 250.0

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

plot 'plot/at-multiple-survey.tsv' \
         u (($4 <= limit) ? $2 : 1/0):(($4 <= limit) ? $3 : 1/0):(($4 <= limit) ? $4 : 1/0):xtic(1) \
                           title 'IPT'                     lt 1 fs pattern 2, \
     ''  u (($7 <= limit) ? $5 : 1/0):(($7 <= limit) ? $6 : 1/0):(($7 <= limit) ? $7 : 1/0) \
                           title 'SortedPoints'            lt 2 fs pattern 4, \
    1/0                   title 'DenseBitset'             with boxes lt 3 fs pattern 5, \
    1/0                   title 'BlockBitmap'             with boxes lt 4 fs pattern 6, \
     ''  u (($16 <= limit) ? $14 : 1/0):(($16 <= limit) ? $15 : 1/0):(($16 <= limit) ? $16 : 1/0) \
                           title 'Roaring'                 lt 5 fs pattern 7, \
     ''  u (($19 <= limit) ? $17 : 1/0):(($19 <= limit) ? $18 : 1/0):(($19 <= limit) ? $19 : 1/0) \
                           title 'LexRun'                  lt 6 fs pattern 8, \
     ''  u (($22 <= limit) ? $20 : 1/0):(($22 <= limit) ? $21 : 1/0):(($22 <= limit) ? $22 : 1/0) \
                           title 'RowCSR\textsubscript{1}' lt 7 fs pattern 9, \
     ''  u (($25 <= limit) ? $23 : 1/0):(($25 <= limit) ? $24 : 1/0):(($25 <= limit) ? $25 : 1/0) \
                           title 'RowCSR\textsubscript{2}' lt 8 fs pattern 10
