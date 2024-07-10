#!/bin/sh

cd /srv/ch

rrdtool create chalupa.rrd --start 1633789842 --step 1h \
    DS:t_loz:GAUGE:48h:-40:80 \
    DS:t_kch:GAUGE:48h:-40:80 \
    DS:t_cim:GAUGE:48h:-40:80 \
    DS:t_ven:GAUGE:48h:-40:80 \
    DS:t_ven2:GAUGE:48h:-40:80 \
    DS:t_pds:GAUGE:48h:-40:80 \
    DS:h_loz:GAUGE:48h:0:100 \
    DS:h_kch:GAUGE:48h:0:100 \
    DS:h_cim:GAUGE:48h:0:100 \
    DS:h_ven:GAUGE:48h:0:100 \
    DS:v_bat:GAUGE:48h:0:20 \
    DS:l_stud:GAUGE:48h:0:400 \
    DS:dp_vni:GAUGE:48h:-40:80 \
    DS:dp_ven:GAUGE:48h:-40:80 \
    DS:sw:GAUGE:48h:0:4 \
    RRA:AVERAGE:0.5:1h:1y RRA:AVERAGE:0.5:1d:10y

chmod go+w chalupa.rrd

