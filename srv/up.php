<?php
    $ip = $_SERVER['REMOTE_ADDR'];
    $base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
    $time_corr = 21; // number of additional seconds in each measurement
    $now = time();
    $db = new PDO('sqlite:'.__DIR__.'/chalupa.db') or die("cannot open database");
    //$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);

    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        $f = fopen('log.txt', 'a+');
        fwrite($f, date("d.m.Y H:i:s") . " " . $ip . " => " . htmlspecialchars(file_get_contents("php://input")) . "\n");
        fclose($f);

        $stmt = $db->prepare("INSERT INTO stats (ip, cnt, cnt_pwron, cnt_lowbatt, cnt_itplus, signalq, cnt_gsm) VALUES (:ip, :cnt, :cnt_pwron, :cnt_lowbatt, :cnt_itplus, :signalq, :cnt_gsm)");
        $stmt->bindValue(':ip', $ip);
        $stmt->bindValue(':cnt', $_POST['c']);
        $stmt->bindValue(':cnt_pwron', $_POST['cp']);
        $stmt->bindValue(':cnt_lowbatt', $_POST['cl']);
        $stmt->bindValue(':cnt_itplus', $_POST['ci']);
        $stmt->bindValue(':signalq', $_POST['sq']);
        $stmt->bindValue(':cnt_gsm', $_POST['cg']);
        $stmt->execute();

        $interval = $_POST['i'];
        $cnt = $_POST['c'];
        $cnt_lowbatt = $_POST['cl'];
        foreach ($_POST as $key => $value) {
            if (strncmp($key, 'it', 2) == 0) {
                $id = substr($key, 2);
                for ($idx = 0; $idx < $cnt; $idx++) {
                    $n1 = strpos($base64, $value[$idx*3+0]);
                    $n2 = strpos($base64, $value[$idx*3+1]);
                    $n3 = strpos($base64, $value[$idx*3+2]);
                    // 987654-321065-432100
                    // TTTTTT-TTTTHH-HHHHHB
                    $temp = (($n1 & 0x3F) << 4) | (($n2 & 0x3C) >> 2);
                    $hygro = (($n2 & 0x03) << 5) | (($n3 & 0x3E) >> 1);
                    $lowbatt = (($n3 & 0x01));
                    if ($temp >= 512) $temp = $temp - 1024;

                    $stmt = $db->prepare("INSERT INTO itp (dt, itp, temp, hygro, lowbatt) VALUES (:dt, :itp, :temp, :hygro, :lowbatt)");
                    $stmt->bindValue(':dt', $now - ($cnt - ($idx+1))*($interval*60+$time_corr)); // FIXME with idx_rx
                    $stmt->bindValue(':itp', $id);
                    $stmt->bindValue(':temp', $temp/10.);
                    $stmt->bindValue(':hygro', $hygro);
                    $stmt->bindValue(':lowbatt', $lowbatt);
                    $stmt->execute();
                }
            }
        }
        $tv = $_POST['tv'];
        for ($idx = 0; $idx < $cnt; $idx++) {
            $n1 = strpos($base64, $value[$idx*8+0]);
            $n2 = strpos($base64, $value[$idx*8+1]);
            $n3 = strpos($base64, $value[$idx*8+2]);
            $n4 = strpos($base64, $value[$idx*8+3]);
            $n5 = strpos($base64, $value[$idx*8+4]);
            $n6 = strpos($base64, $value[$idx*8+5]);
            $n7 = strpos($base64, $value[$idx*8+6]);
            $n8 = strpos($base64, $value[$idx*8+7]);
            // 765432-103210-987654-321098-765432-107654-321076-543210
            // iiiiii-iicccc-tttttt-tttttt-tttttt-ttvvvv-vvvvvv-vvvvvv
            // dddddd-ddrrrr-IIIIII-IIIIEE-EEEEEE-EEBBBB-BBBBPP-PPPPPP
            $idx_rx = (($n1 & 0x3F) << 2) | (($n2 & 0x30) >> 4);
            $cnt_rx = (($n2 & 0x0F));
            $ti = (($n3 & 0x3F) << 4) | (($n4 & 0x3C) >> 4);
            $te = (($n4 & 0x03) << 8) | (($n5 & 0x3F) << 2) | (($n6 & 0x30) >> 4);
            $vb = (($n6 & 0x0F) << 4) | (($n7 & 0x3C) >> 2);
            $vp = (($n7 & 0x03) << 6) | (($n8 & 0x3F));
            if ($ti >= 512) $ti = $ti - 1024;
            if ($te >= 512) $te = $te - 1024;

            $stmt = $db->prepare("INSERT INTO adc (dt, idx, cnt_rx, tint, text, vbatt, vpv) VALUES (:dt, :idx, :cnt_rx, :ti, :te, :vb, :vp)");
            $stmt->bindValue(':dt', $now - ($cnt - ($idx+1))*($interval*60+$time_corr)); // FIXME with idx_rx
            $stmt->bindValue(':idx', $idx_rx);
            $stmt->bindValue(':cnt_rx', $cnt_rx);
            $stmt->bindValue(':ti', $ti/10.);
            $stmt->bindValue(':te', $te/10.);
            $stmt->bindValue(':vb', $vb/10.);
            $stmt->bindValue(':vp', $vp/10.);
            $stmt->execute();
        }

        printf("ACK\n");
    } else {
?>
<html><body>
<h1>Zobrazeni GET/POST pozadavku</h1>
<pre><?php require('log.txt'); ?></pre>
<h1>Dump databaze</h1>
<pre>
<?php
    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM stats ORDER BY dt DESC");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\tip\t\tcnt\tpwron\tlowbatt\titplus\tsq\tgsm\n";
    echo "---------\t\t--\t--\t\t--\t\t---\t-----\t-------\t------\t--\t---\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM itp ORDER BY dt DESC, itp ASC");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\titp\ttemp\thygro\tlowbatt\n";
    echo "---------\t\t--\t--\t\t---\t----\t-----\t-------\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM adc ORDER BY dt DESC");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\tidx\tcnt_rx\ttint\ttext\tvbatt\tvpv\n";
    echo "---------\t\t--\t--\t\t---\t------\t----\t----\t-----\t---\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";
?>
</pre>
</body></html>
<?php
    }
    $db = null;
?>
