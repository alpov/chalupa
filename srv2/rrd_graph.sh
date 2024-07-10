#!/bin/sh

cd /srv/ch

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
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Akumulator"

rrdtool graph chw_lstud.png -w 785 -h 320 -a PNG --slope-mode --start -7d --end now  --vertical-label "hladina [cm]" \
    DEF:l_stud=chalupa.rrd:l_stud:AVERAGE LINE1:l_stud#0000FF:"Studna"

rrdtool graph chw_out.png -w 785 -h 150 -a PNG --slope-mode -l 0 -u 4 --start -7d --end now  --vertical-label "vystupy [-]" \
    DEF:sw=chalupa.rrd:sw:AVERAGE LINE1:sw#0000FF:"Vystupy (1=WC, 2=vetrani)"

rrdtool graph chw_dp.png -w 785 -h 320 -a PNG --slope-mode --start -7d --end now  --vertical-label "rosny bod [°C]" \
    DEF:dp_vni=chalupa.rrd:dp_vni:AVERAGE LINE1:dp_vni#0000FF:"Vnitrni prumerny" \
    DEF:dp_ven=chalupa.rrd:dp_ven:AVERAGE LINE1:dp_ven#FF00FF:"Venkovni"


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

rrdtool graph chm_dp.png -w 785 -h 320 -a PNG --slope-mode --start -1m --end now  --vertical-label "rosny bod [°C]" \
    DEF:dp_vni=chalupa.rrd:dp_vni:AVERAGE LINE1:dp_vni#0000FF:"Vnitrni prumerny" \
    DEF:dp_ven=chalupa.rrd:dp_ven:AVERAGE LINE1:dp_ven#FF00FF:"Venkovni"

rrdtool graph chm_out.png -w 785 -h 150 -a PNG --slope-mode -l 0 -u 4 --start -1m --end now  --vertical-label "vystupy [-]" \
    DEF:sw=chalupa.rrd:sw:AVERAGE LINE1:sw#0000FF:"Vystupy (1=WC, 2=vetrani)"

rrdtool graph chm_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -1m --end now  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Akumulator"

rrdtool graph chm_lstud.png -w 785 -h 320 -a PNG --slope-mode --start -1m --end now  --vertical-label "hladina [cm]" \
    DEF:l_stud=chalupa.rrd:l_stud:AVERAGE LINE1:l_stud#0000FF:"Studna"

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

rrdtool graph chq_dp.png -w 785 -h 320 -a PNG --slope-mode --start -3m --end now  --vertical-label "rosny bod [°C]" \
    DEF:dp_vni=chalupa.rrd:dp_vni:AVERAGE LINE1:dp_vni#0000FF:"Vnitrni prumerny" \
    DEF:dp_ven=chalupa.rrd:dp_ven:AVERAGE LINE1:dp_ven#FF00FF:"Venkovni"

rrdtool graph chq_out.png -w 785 -h 150 -a PNG --slope-mode -l 0 -u 4 --start -3m --end now  --vertical-label "vystupy [-]" \
    DEF:sw=chalupa.rrd:sw:AVERAGE LINE1:sw#0000FF:"Vystupy (1=WC, 2=vetrani)"

rrdtool graph chq_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -3m --end now  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Akumulator"

rrdtool graph chq_lstud.png -w 785 -h 320 -a PNG --slope-mode --start -3m --end now  --vertical-label "hladina [cm]" \
    DEF:l_stud=chalupa.rrd:l_stud:AVERAGE LINE1:l_stud#0000FF:"Studna"

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

rrdtool graph chy_dp.png -w 785 -h 320 -a PNG --slope-mode --start -1y --end now  --vertical-label "rosny bod [°C]" \
    DEF:dp_vni=chalupa.rrd:dp_vni:AVERAGE LINE1:dp_vni#0000FF:"Vnitrni prumerny" \
    DEF:dp_ven=chalupa.rrd:dp_ven:AVERAGE LINE1:dp_ven#FF00FF:"Venkovni"

rrdtool graph chy_out.png -w 785 -h 150 -a PNG --slope-mode -l 0 -u 4 --start -1y --end now  --vertical-label "vystupy [-]" \
    DEF:sw=chalupa.rrd:sw:AVERAGE LINE1:sw#0000FF:"Vystupy (1=WC, 2=vetrani)"

rrdtool graph chy_vbatt.png -w 785 -h 320 -a PNG --slope-mode --start -1y --end now --step 1d  --vertical-label "napeti [V]" \
    DEF:v_bat=chalupa.rrd:v_bat:AVERAGE LINE1:v_bat#0000FF:"Akumulator"

rrdtool graph chy_lstud.png -w 785 -h 320 -a PNG --slope-mode --start -1y --end now  --vertical-label "hladina [cm]" \
    DEF:l_stud=chalupa.rrd:l_stud:AVERAGE LINE1:l_stud#0000FF:"Studna"

