# Plot: at_random absolute performance for multiple-survey scenarios.
set encoding utf8
set terminal cairolatex pdf size 5.5in,3.0in
set output 'generated/at-multiple-survey.tex'

set ylabel 'ns / \texttt{at}'
set y2label 'M\texttt{at} / s'
set y2tics format '%.1f'
set link y2 via 1000.0/y inverse 1000.0/y
set yrange [0:160]
set grid ytics
set key below center horizontal

set style data histogram
set style histogram errorbars gap 1.5 lw 1
set style fill pattern border rgb 'black'
set boxwidth 0.8

set xtics rotate by -35

set linetype 1 lc rgb '#000000'
set linetype 2 lc rgb '#000000'

plot 'plot/at-multiple-survey.tsv' \
         u 2:3:4:xtic(1) title 'IPT'          lt 1 fs pattern 2, \
     ''  u 5:6:7         title 'SortedPoints' lt 2 fs pattern 4
