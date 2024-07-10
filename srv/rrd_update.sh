#!/usr/bin/php
<?php
    chdir('/srv/ch');

    $db = new PDO('sqlite:'.__DIR__.'/chalupa.db') or die("cannot open database"); 
    //$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);

    $query = $db->query("select adc.dt, itp31.temp as t1, itp46.temp as t2, itp62.temp as t3, ".
        "itp57.temp as t4, adc.text, adc.tint, itp31.hygro as h1, itp46.hygro as h2, ".
        "itp62.hygro as h3, itp57.hygro as h4, adc.vbatt ".
        "from adc ".
        "left join itp31 on adc.dt=itp31.dt ".
        "left join itp46 on adc.dt=itp46.dt ".
        "left join itp62 on adc.dt=itp62.dt ".
        "left join itp57 on adc.dt=itp57.dt ".
        "order by adc.dt asc");

    $row = $query->fetch(PDO::FETCH_ASSOC);
    while ($row != false) {
        if ($row['t1'] == 0 && $row['h1'] == 0) { $row['t1'] = 'U'; $row['h1'] = 'U'; }
        if ($row['t2'] == 0 && $row['h2'] == 0) { $row['t2'] = 'U'; $row['h2'] = 'U'; }
        if ($row['t3'] == 0 && $row['h3'] == 0) { $row['t3'] = 'U'; $row['h3'] = 'U'; }
        if ($row['t4'] == 0 && $row['h4'] == 0) {
            $row['t4'] = $row['text']; $row['h4'] = 'U';
            $row['text'] = 'U';
        }
        $data = implode(array_values($row), ":");
        rrd_update(__DIR__.'/chalupa.rrd', array($data)); 
        echo $data . "\n";

        $row = $query->fetch(PDO::FETCH_ASSOC);
    }

    $db = null;

    system('./rrd_graph.sh');
?>

