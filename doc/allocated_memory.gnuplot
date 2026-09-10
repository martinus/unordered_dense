#!/usr/bin/gnuplot
#
# Draws allocated_memory.png from the CSVs scripts/ab/alloc_timeline.cpp writes. That script passes
# the series in, so a machine without boost or abseil gets a chart of whatever it could measure
# instead of an error about a missing file:
#
#   gnuplot -e "files='a.csv b.csv'; titles='name-a name-b'" allocated_memory.gnuplot
#
# noenhanced, because every title here is a C++ name full of underscores and enhanced text turns
# each one into a subscript.

set encoding utf8
set terminal pngcairo size 900,600 font 'Verdana,10' noenhanced

if (!exists("files")) {
    files = 'allocated_memory_map.csv allocated_memory_segmented_map.csv'
    titles = 'ankerl::unordered_dense::map ankerl::unordered_dense::segmented_map'
}

set datafile separator ','

# The x axis ends where the slowest map finished, rather than at the next round number.
set autoscale xfix
set format x "%.2f"

# Border and grid: gray, thin, behind the data.
set style line 11 lc rgb '#808080' lt 1
set border 3 back ls 11
set tics nomirror out scale 0.75
set style line 12 lc rgb '#808080' lt 0 lw 1
set grid back ls 12

set style line 1 lt 1 lc rgb '#1B9E77' # dark teal
set style line 2 lt 1 lc rgb '#D95F02' # dark orange
set style line 3 lt 1 lc rgb '#7570B3' # dark lilac
set style line 4 lt 1 lc rgb '#E7298A' # dark magenta

set key left top
set output 'allocated_memory.png'

set xlabel "Runtime [s]"
set ylabel "Allocated memory [MB]"
set title "Inserting 10 million uint64_t -> uint64_t pairs, every allocation counted at its malloc size"

# steps, not lines: memory changes at an instant and holds until the next allocation, so an
# interpolated slope between two points would be a picture of something that never happened.
# The two maps this repository ships are drawn heavier than the maps they are being compared
# against, so the eye lands on them first. Keyed off the name rather than the position, so adding
# or dropping a series cannot silently emphasise the wrong one.
plot for [i=1:words(files)] word(files, i) using 1:($2/1e6) \
    with steps ls i lw (strstrt(word(titles, i), "ankerl") > 0 ? 3.5 : 1.5) title word(titles, i)
