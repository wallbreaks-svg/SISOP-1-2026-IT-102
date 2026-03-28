#!/bin/bash

case $2 in
  a)
    awk -F',' 'NR>1 {count++} END {print "jumlah seluruh penumpang KANJ adalah " count " orang"}' $1
    ;;
  b)
    awk -F',' 'NR>1 {
    gsub(/\r/,"",$4); 
    gsub(/^ +| +$/,"",$4); 
    if($4!="") g[$4]=1
} END {
    print "Jumlah gerbong penumpang KANJ adalah " length(g)
}' $1
    ;;
  c)
    awk -F',' 'NR>1 {if($2>max){max=$2; nama=$1}} END {print "Penumpang tertua adalah "nama" dengan usia  " max " tahun"}' $1
    ;;
  d)
    awk -F',' 'NR>1 {sum+=$2; count++} END {print "rata rata usia penumpang adalah " int (sum/count) " tahun"}' $1
    ;;
  e)
    awk -F',' 'NR>1&& $3=="Business" {count++} END {print "Jumlah penumpang business class ada "count" orang"}' $1
    ;;
  *)
    echo "Pilihan tidak valid"
    ;;
esac
