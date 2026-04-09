# Plot: construction ns/point vs point-set density for four grid sizes.
set encoding utf8
set terminal cairolatex pdf size 5.8in,2.8in
set output 'generated/construct-vs-density.tex'

set xlabel 'Point-set density [\%]'
set ylabel 'ns / point (construction)'
set y2label 'Mpoint / s'
set y2tics format '%.0f'
set link y2 via 1000.0/y inverse 1000.0/y
set yrange [0:140]

set grid
set key right top

set xtics ('1' 1, '' 2, '5' 5, '10' 10, '25' 25, '50' 50, \
           '75' 75, '90' 90, '95' 95, '' 98, '99' 99, '' 100) rotate by -90
set xrange [0:101]

set style line 1 lc rgb '#111111' lw 2 dt 1 pt 7  ps 0.8
set style line 2 lc rgb '#444444' lw 2 dt 2 pt 9  ps 0.8
set style line 3 lc rgb '#777777' lw 2 dt 3 pt 5  ps 0.8
set style line 4 lc rgb '#AAAAAA' lw 2 dt 4 pt 13 ps 0.8

plot 'plot/construct-vs-density.tsv' \
         u 1:2:3:4   w yerrorlines ls 1 title '$10{\times}10{\times}10$', \
     ''  u 1:5:6:7   w yerrorlines ls 2 title '$100{\times}10{\times}10$', \
     ''  u 1:8:9:10  w yerrorlines ls 3 title '$100{\times}100{\times}10$', \
     ''  u 1:11:12:13 w yerrorlines ls 4 title '$100{\times}100{\times}100$'
