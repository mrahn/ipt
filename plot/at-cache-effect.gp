# Plot: aggregate cache-configuration trade-offs relative to c0r0e0l0.
set encoding utf8
set terminal cairolatex pdf size 5.8in,4.5in
set output 'generated/at-cache-effect.tex'

set grid back ytics lc rgb 'gray60' dt 3 lw 1
set style fill pattern border rgb 'black'
set boxwidth 0.8

set linetype 1 lc rgb '#000000'
set linetype 2 lc rgb '#000000'
set linetype 3 lc rgb '#000000'
set linetype 4 lc rgb '#000000'

row_count = 16
block_gap = 1
block_span = row_count + block_gap
default_row = 7
frontier_row = 13

set multiplot layout 2,1 margins 0.08,0.98,0.14,0.97 spacing 0.09,0.20

set style data histogram
set style histogram errorbars gap 1 lw 1
set key at graph 0.5, -0.33 center horizontal maxrows 1
set ylabel 'Time ratio vs.\ \texttt{c0r0e0l0}'
set yrange [0:2.0]
set ytics 0.2
set xrange [-0.5:3 * row_count + 2 * block_gap - 0.5]
unset xlabel
set grid back xtics ytics lc rgb 'gray60' dt 3 lw 1
set xtics \
    ('c0r1e1l0' default_row, \
     'c0r1e1l1' frontier_row, \
     'c0r1e1l0' default_row + block_span, \
     'c0r1e1l1' frontier_row + block_span, \
     'c0r1e1l0' default_row + 2 * block_span, \
     'c0r1e1l1' frontier_row + 2 * block_span)
set arrow from graph 0, first 1 to graph 1, first 1 \
    nohead lt 0 lw 1 lc rgb 'gray50'

plot 'plot/at-cache-effect.tsv' \
         u ($0):3                               w boxes      title 'construct'     lt 1 fs pattern 2, \
     ''  u ($0):3:4:5                           w yerrorbars notitle               lt 1, \
     ''  u ($0 + block_span):6                               w boxes      title '\texttt{pos}' lt 2 fs pattern 4, \
     ''  u ($0 + block_span):6:7:8                           w yerrorbars notitle               lt 2, \
     ''  u ($0 + 2 * block_span):9                               w boxes      title '\texttt{at}'  lt 3 fs pattern 5, \
     ''  u ($0 + 2 * block_span):9:10:11                       w yerrorbars notitle               lt 3

unset key
unset arrow
unset grid
set grid back ytics lc rgb 'gray60' dt 3 lw 1
set style histogram cluster gap 1
set ylabel 'Memory ratio vs.\ \texttt{c0r0e0l0}'
set xlabel 'Configurations ordered by memory ratio'
set yrange [0:2.0]
set ytics 0.2
set xrange [-0.5:15.5]
set xtics rotate by -35
plot 'plot/at-cache-effect.tsv' \
         u 12:xticlabels(2) title 'memory' lt 4 fs pattern 6

unset multiplot
