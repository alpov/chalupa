#!/bin/sh

cd /srv/www/sos.alpov.net/ch

#    DEF:t_ven2=chalupa.rrd:t_ven2:AVERAGE LINE1:t_ven2#CCCCCC:"Venkovni-NTC"
#    DEF:t_pds=chalupa.rrd:t_pds:AVERAGE LINE1:t_pds#00FF00:"Predsin-NTC"

rrdtool graph chw_temp.png -w 785 -h 320 -a PNG --slope-mode --start -7d --end now  --vertical-label "teplota [°C]" \
    DEF:t_loz=chalupa.rrd:t_loz:AVERAGE LINE1:t_loz#FF0000:"Loznice" \
    DEF:t_kch=chalupa.rrd:t_kch:AVERAGE LINE1:t_kch#0000FF:"Kuchyn" \
    DEF:t_cim=chalupa.rrd:t_cim:AVERAGE LINE1:t_cim#00FFFF:"Cimerka" \
    DEF:t_ven=chalupa.rrd:t_ven:AVERAGE LINE1:t_ven#FF00FF:"Venkovni"

rrdtool graph chw_hygro.png -w 785 -h 320 -a PNG --slope-mode --start -7d --end now  --vertical-label "vlhkost [%]" \
    DEF:h_loz=chalupa.rrd:h_loz:AVERAGE LINE1:h_loz#FF0000:"Loznice" \
    DEF:h_kch=chalupa.rrd:h_kch:AVERAGE LINE1:h_kch#0000FF:"Kuchyn" \
    DEF:h_cim=chalupa.rrd:h_cim:AVERAGE LINE1:h_cim#00FFFF:"Cimerka" \
    DEF:h_ven=chalupa.rrd:h_ven:AVERAGE LINE1:h_ven#FF00FF:"Venkovni"

rrdtool graph chw_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -7d --end now  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Battery"

rrdtool graph chm_temp.png -w 785 -h 320 -a PNG --slope-mode --start -1m --end now  --vertical-label "teplota [°C]" \
    DEF:t_loz=chalupa.rrd:t_loz:AVERAGE LINE1:t_loz#FF0000:"Loznice" \
    DEF:t_kch=chalupa.rrd:t_kch:AVERAGE LINE1:t_kch#0000FF:"Kuchyn" \
    DEF:t_cim=chalupa.rrd:t_cim:AVERAGE LINE1:t_cim#00FFFF:"Cimerka" \
    DEF:t_ven=chalupa.rrd:t_ven:AVERAGE LINE1:t_ven#FF00FF:"Venkovni"

rrdtool graph chm_hygro.png -w 785 -h 320 -a PNG --slope-mode --start -1m --end now  --vertical-label "vlhkost [%]" \
    DEF:h_loz=chalupa.rrd:h_loz:AVERAGE LINE1:h_loz#FF0000:"Loznice" \
    DEF:h_kch=chalupa.rrd:h_kch:AVERAGE LINE1:h_kch#0000FF:"Kuchyn" \
    DEF:h_cim=chalupa.rrd:h_cim:AVERAGE LINE1:h_cim#00FFFF:"Cimerka" \
    DEF:h_ven=chalupa.rrd:h_ven:AVERAGE LINE1:h_ven#FF00FF:"Venkovni"

rrdtool graph chm_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -1m --end now  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Battery"

rrdtool graph chq_temp.png -w 785 -h 320 -a PNG --slope-mode --start -3m --end now  --vertical-label "teplota [°C]" \
    DEF:t_loz=chalupa.rrd:t_loz:AVERAGE LINE1:t_loz#FF0000:"Loznice" \
    DEF:t_kch=chalupa.rrd:t_kch:AVERAGE LINE1:t_kch#0000FF:"Kuchyn" \
    DEF:t_cim=chalupa.rrd:t_cim:AVERAGE LINE1:t_cim#00FFFF:"Cimerka" \
    DEF:t_ven=chalupa.rrd:t_ven:AVERAGE LINE1:t_ven#FF00FF:"Venkovni"

rrdtool graph chq_hygro.png -w 785 -h 320 -a PNG --slope-mode --start -3m --end now  --vertical-label "vlhkost [%]" \
    DEF:h_loz=chalupa.rrd:h_loz:AVERAGE LINE1:h_loz#FF0000:"Loznice" \
    DEF:h_kch=chalupa.rrd:h_kch:AVERAGE LINE1:h_kch#0000FF:"Kuchyn" \
    DEF:h_cim=chalupa.rrd:h_cim:AVERAGE LINE1:h_cim#00FFFF:"Cimerka" \
    DEF:h_ven=chalupa.rrd:h_ven:AVERAGE LINE1:h_ven#FF00FF:"Venkovni"

rrdtool graph chq_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -3m --end now  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Battery"

rrdtool graph chy_temp.png -w 785 -h 320 -a PNG --slope-mode --start -1y --end now --step 1d  --vertical-label "teplota [°C]" \
    DEF:t_loz=chalupa.rrd:t_loz:AVERAGE LINE1:t_loz#FF0000:"Loznice" \
    DEF:t_kch=chalupa.rrd:t_kch:AVERAGE LINE1:t_kch#0000FF:"Kuchyn" \
    DEF:t_cim=chalupa.rrd:t_cim:AVERAGE LINE1:t_cim#00FFFF:"Cimerka" \
    DEF:t_ven=chalupa.rrd:t_ven:AVERAGE LINE1:t_ven#FF00FF:"Venkovni"

rrdtool graph chy_hygro.png -w 785 -h 320 -a PNG --slope-mode --start -1y --end now --step 1d  --vertical-label "vlhkost [%]" \
    DEF:h_loz=chalupa.rrd:h_loz:AVERAGE LINE1:h_loz#FF0000:"Loznice" \
    DEF:h_kch=chalupa.rrd:h_kch:AVERAGE LINE1:h_kch#0000FF:"Kuchyn" \
    DEF:h_cim=chalupa.rrd:h_cim:AVERAGE LINE1:h_cim#00FFFF:"Cimerka" \
    DEF:h_ven=chalupa.rrd:h_ven:AVERAGE LINE1:h_ven#FF00FF:"Venkovni"

rrdtool graph chy_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -1y --end now --step 1d  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Akumulator"
