# Plot: hit-vs-miss try_pos timings vs miss_ratio.
#
# Two-panel multiplot: left = in-grid misses, right = out-of-grid
# misses.  One curve per algorithm; x = miss_ratio (linear, 0..1);
# y = ns / try_pos on a logarithmic axis so the wide IPT/baseline
# spread is visible on a single figure.
#
# TSV columns (1-indexed):
#   1  miss_ratio
#   2  in_grid_greedy-plus-merge       10  out_of_grid_greedy-plus-merge
#   3  in_grid_sorted-points           11  out_of_grid_sorted-points
#   4  in_grid_dense-bitset            12  out_of_grid_dense-bitset
#   5  in_grid_block-bitmap            13  out_of_grid_block-bitmap
#   6  in_grid_roaring                 14  out_of_grid_roaring
#   7  in_grid_lex-run                 15  out_of_grid_lex-run
#   8  in_grid_row-csr-k1              16  out_of_grid_row-csr-k1
#   9  in_grid_row-csr-kdminus1        17  out_of_grid_row-csr-kdminus1
set encoding utf8
set terminal cairolatex pdf size 6.0in,3.6in
set output 'generated/pos-mix-summary.tex'

set logscale y
set ylabel 'ns / \texttt{try\_pos}'
set xlabel 'miss ratio'
set xrange [-0.05:1.05]
set yrange [1:100000]
set xtics 0,0.25,1
set ytics \
    ('$10^{0}$' 1, \
     '$10^{1}$' 10, \
     '$10^{2}$' 100, \
     '$10^{3}$' 1000, \
     '$10^{4}$' 10000, \
     '$10^{5}$' 100000)
set grid back ytics xtics lc rgb 'gray60' dt 3 lw 1
set key off

set linetype 1 lc rgb '#000000' lw 2 pt 7
set linetype 2 lc rgb '#404040' lw 2 pt 5
set linetype 3 lc rgb '#606060' lw 2 pt 9
set linetype 4 lc rgb '#808080' lw 2 pt 11
set linetype 5 lc rgb '#a0a0a0' lw 2 pt 13
set linetype 6 lc rgb '#202020' lw 2 dt 2 pt 6
set linetype 7 lc rgb '#404040' lw 2 dt 2 pt 4
set linetype 8 lc rgb '#606060' lw 2 dt 2 pt 8

set multiplot

# Left panel.
set lmargin at screen 0.10
set rmargin at screen 0.48
set bmargin at screen 0.28
set tmargin at screen 0.94
set title 'in-grid'
set ylabel 'ns / \texttt{try\_pos}'
plot 'plot/pos-mix-summary.tsv' \
         u 1:2  w lp title 'IPT'                     lt 1, \
     ''  u 1:3  w lp title 'SortedPoints'            lt 2, \
     ''  u 1:4  w lp title 'DenseBitset'             lt 3, \
     ''  u 1:5  w lp title 'BlockBitmap'             lt 4, \
     ''  u 1:6  w lp title 'Roaring'                 lt 5, \
     ''  u 1:7  w lp title 'LexRun'                  lt 6, \
     ''  u 1:8  w lp title 'RowCSR\textsubscript{1}' lt 7, \
     ''  u 1:9  w lp title 'RowCSR\textsubscript{2}' lt 8

# Right panel.
set lmargin at screen 0.57
set rmargin at screen 0.98
set bmargin at screen 0.28
set tmargin at screen 0.94
set title 'out-of-grid'
unset ylabel
plot 'plot/pos-mix-summary.tsv' \
         u 1:10 w lp notitle lt 1, \
     ''  u 1:11 w lp notitle lt 2, \
     ''  u 1:12 w lp notitle lt 3, \
     ''  u 1:13 w lp notitle lt 4, \
     ''  u 1:14 w lp notitle lt 5, \
     ''  u 1:15 w lp notitle lt 6, \
     ''  u 1:16 w lp notitle lt 7, \
     ''  u 1:17 w lp notitle lt 8

# Legend strip.
unset logscale y
unset grid
unset border
unset xlabel
unset ylabel
unset xtics
unset ytics
set format x ''
set format y ''
set xrange [0:1]
set yrange [0:1]
set lmargin at screen 0.10
set rmargin at screen 0.98
set bmargin at screen 0.02
set tmargin at screen 0.16
unset title
set key center center horizontal maxcols 8 samplen 1 spacing 1
plot 1/0 w lp title 'IPT'                     lt 1, \
    1/0 w lp title 'SortedPoints'            lt 2, \
    1/0 w lp title 'DenseBitset'             lt 3, \
    1/0 w lp title 'BlockBitmap'             lt 4, \
    1/0 w lp title 'Roaring'                 lt 5, \
    1/0 w lp title 'LexRun'                  lt 6, \
    1/0 w lp title 'RowCSR\textsubscript{1}' lt 7, \
    1/0 w lp title 'RowCSR\textsubscript{2}' lt 8

unset multiplot
