#!/usr/bin/gnuplot

set terminal pngcairo enhanced font "Arial,12" size 1600,900
set output 'latency_cdf.png'

set title "Hashmap Operation insert & erase (n=1,000,000)"
set xlabel "Retired instructions (PERF\\_COUNT\\_HW\\_INSTRUCTIONS)"
set ylabel "Cumulative Probability (%)"

# Fine-grained grid
set grid xtics ytics mxtics mytics linewidth 0.5
set mxtics 10  # 10 minor divisions between major x-ticks
set mytics 10  # 10 minor divisions between major y-ticks
set xtics auto  # Auto-adjust major x-ticks based on data range
# set ytics 0,10,100 format "%.0f%%"

set xrange [0:500]

set key right bottom

stats 'std.dat' using 1 name "TIMES" nooutput

# Plot CDF for all entries
plot \
  'std.dat'             using 1:(100.0*$0/TIMES_records) with lines lw 1 title "std::unordered\\_map (max: 698)", \
  'unordered_dense.dat' using 1:(100.0*$0/TIMES_records) with lines lw 1 title "ankerl::unordered\\_dense::map (max: 418)", \
  'boost_unordered_flat_map.dat' using 1:(100.0*$0/TIMES_records) with lines lw 1 title "boost::unordered\\_flat\\_map (max: 251230)", \
  'boost_unordered_map.dat' using 1:(100.0*$0/TIMES_records) with lines lw 1 title "boost::unordered\\_map (max: 629)", \
