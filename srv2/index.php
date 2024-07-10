<html><head><title>Chalupa</title></head><body><pre>
Navigace: <a href="index.php">tyden</a> <a href="index.php?i=m">mesic</a> <a href="index.php?i=q">ctvrtleti</a> <a href="index.php?i=y">rok</a> <a href="up.php">databaze</a>

<?php
    if (!isset($_GET['i'])) $interval = 'w';
    else $interval = $_GET['i'];

    $db = new PDO('sqlite:'.__DIR__.'/chalupa2.db') or die("cannot open database");
    //$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);

    $query = $db->query("SELECT dt+72424 as dt, datetime(dt+72424, 'unixepoch', 'localtime') as dtf FROM stats ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        $next = ($row['dt'] - time()) / 60;
        echo "Pristi aktualizace: " . $row['dtf'] . " (za " . floor($next/60) . "h " . ($next%60) . "m)\n";
    }
    echo "\n";

    $query = $db->query("SELECT * FROM itp_loznice ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        echo "Loznice:  T = " . $row['temp'] . " °C, H = " . $row['hygro'] . "%\n";
    }

    $query = $db->query("SELECT * FROM itp_kuchyn ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        echo "Kuchyn:   T = " . $row['temp'] . " °C, H = " . $row['hygro'] . "%\n";
    }

    $query = $db->query("SELECT * FROM itp_cimerka ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        echo "Cimerka:  T = " . $row['temp'] . " °C, H = " . $row['hygro'] . "%\n";
    }

    $query = $db->query("SELECT * FROM itp_venkovni ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        echo "Venkovni: T = " . $row['temp'] . " °C, H = " . $row['hygro'] . "%\n";
    }

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM adc ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        echo "Predsin:  T = " . $row['tint'] . " °C\n";
        echo "Venkovni: T = " . $row['text'] . " °C\n";
        echo "Napeti:   U = " . $row['vbatt'] . " V\n";
    }

    $query = $db->query("SELECT * FROM itp_studna ORDER BY dt DESC LIMIT 1");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    if ($row != false) {
        echo "Studna:   h = " . $row['level'] . " cm\n";
    }

    echo "\n\n";

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM stats ORDER BY dt DESC LIMIT 5");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\tip\t\tcnt\tpwron\tlowbatt\titplus\tsq\tgsm\n";
    echo "---------\t\t--\t--\t\t--\t\t---\t-----\t-------\t------\t--\t---\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";

    $db = null;
?>

</pre>

<img src=ch<?php echo $interval; ?>_temp.png?<?php echo time(); ?>>
<p>

<img src=ch<?php echo $interval; ?>_hygro.png?<?php echo time(); ?>>
<p>

<img src=ch<?php echo $interval; ?>_dp.png?<?php echo time(); ?>>
<p>

<img src=ch<?php echo $interval; ?>_lstud.png?<?php echo time(); ?>>
<p>

<img src=ch<?php echo $interval; ?>_out.png?<?php echo time(); ?>>
<p>

<img src=ch<?php echo $interval; ?>_vbatt.png?<?php echo time(); ?>>
<p>

</body></html>
