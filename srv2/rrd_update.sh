#!/usr/bin/php
<?php
    chdir('/srv/ch');
    require_once('dewpoint.php');

    $db = new PDO('sqlite:'.__DIR__.'/chalupa2.db') or die("cannot open database"); 
    //$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);

    $query = $db->query("select adc.dt, itp_loznice.temp as t1, itp_kuchyn.temp as t2, itp_cimerka.temp as t3, ".
        "itp_venkovni.temp as t4, adc.text, adc.tint, itp_loznice.hygro as h1, itp_kuchyn.hygro as h2, ".
        "itp_cimerka.hygro as h3, itp_venkovni.hygro as h4, adc.vbatt, itp_studna.level as lvl, adc.outputs ".
        "from adc ".
        "left join itp_loznice on adc.dt=itp_loznice.dt ".
        "left join itp_kuchyn on adc.dt=itp_kuchyn.dt ".
        "left join itp_cimerka on adc.dt=itp_cimerka.dt ".
        "left join itp_venkovni on adc.dt=itp_venkovni.dt ".
        "left join itp_studna on adc.dt=itp_studna.dt ".
        "order by adc.dt asc");

    $row = $query->fetch(PDO::FETCH_ASSOC);
    while ($row != false) {
        $tavg = array();
        $havg = array();
        $outputs = array_pop($row);
        if ($row['t1'] == 0 && $row['h1'] == 0) { 
            $row['t1'] = 'U';
            $row['h1'] = 'U'; 
        } else {
            array_push($tavg, $row['t1']);
            array_push($havg, $row['h1']);
        }
        if ($row['t2'] == 0 && $row['h2'] == 0) {
            $row['t2'] = 'U';
            $row['h2'] = 'U';
        } else {
            array_push($tavg, $row['t2']);
            array_push($havg, $row['h2']);
        }
        if ($row['t3'] == 0 && $row['h3'] == 0) {
            $row['t3'] = 'U';
            $row['h3'] = 'U';
        } else {
            array_push($tavg, $row['t3']);
            array_push($havg, $row['h3']);
        }

        if ($row['lvl'] == 0) {
            $row['lvl'] = 'U';
        }

        $row['idew'] = dewPoint(array_sum($havg)/count($havg), array_sum($tavg)/count($tavg));

        if ($row['t4'] == 0 && $row['h4'] == 0) {
            $row['t4'] = $row['text']; $row['h4'] = 'U';
            $row['text'] = 'U';
            $row['edew'] = 'U';
        } else {
            $row['edew'] = dewPoint($row['h4'], $row['t4']);
        }

        array_push($row, $outputs);
        $data = implode(":", array_values($row));
        rrd_update(__DIR__.'/chalupa.rrd', array($data)); 
        echo $data . "\n";

        $row = $query->fetch(PDO::FETCH_ASSOC);
    }

    $db = null;

    system('./rrd_graph.sh');
?>

