<?php
    $ip = $_SERVER['REMOTE_ADDR'];
    $base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
    $time_corr = 21; // number of additional seconds in each measurement
    $now = time();
    $db = new PDO('sqlite:'.__DIR__.'/chalupa2.db') or die("cannot open database");
    //$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);
    //ini_set('display_errors', 1);
    //ini_set('display_startup_errors', 1);
    //error_reporting(E_ALL);

    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        $f = fopen('log.txt', 'a+');
        fwrite($f, date("d.m.Y H:i:s") . " " . $ip . " => " . htmlspecialchars(file_get_contents("php://input")) . "\n");

        $stmt = $db->prepare("INSERT INTO stats (ip, cnt, cnt_pwron, cnt_lowbatt, cnt_itplus, signalq, cnt_gsm) VALUES (:ip, :cnt, :cnt_pwron, :cnt_lowbatt, :cnt_itplus, :signalq, :cnt_gsm)");
        $stmt->bindValue(':ip', $ip);
        $stmt->bindValue(':cnt', $_POST['c']);
        $stmt->bindValue(':cnt_pwron', $_POST['cp']);
        $stmt->bindValue(':cnt_lowbatt', $_POST['cl']);
        $stmt->bindValue(':cnt_itplus', $_POST['ci']);
        $stmt->bindValue(':signalq', $_POST['sq']);
        $stmt->bindValue(':cnt_gsm', $_POST['cg']);
        $stmt->execute();

        $stmt = $db->prepare("INSERT INTO config (mcfg_typ_entries, mcfg_interval, mcfg_itplus_time, mcfg_volt_min, ".
            "out1_volt_min, out2_volt_min, out3_volt_min, out1_itp_in, out1_itp_out, out1_dp_enable, out1_dp_in_offset, ".
            "out1_t_in_max, out1_t_out_min, out1_h_out_max) VALUES (:mcfg_typ_entries, :mcfg_interval, :mcfg_itplus_time, ".
            ":mcfg_volt_min, :out1_volt_min, :out2_volt_min, :out3_volt_min, :out1_itp_in, :out1_itp_out, :out1_dp_enable, ".
            ":out1_dp_in_offset, :out1_t_in_max, :out1_t_out_min, :out1_h_out_max)");
        $stmt->bindValue(':mcfg_typ_entries', $_POST['ss']);
        $stmt->bindValue(':mcfg_interval', $_POST['si']);
        $stmt->bindValue(':mcfg_itplus_time', $_POST['st']);
        $stmt->bindValue(':mcfg_volt_min', $_POST['sv']);
        $stmt->bindValue(':out1_volt_min', $_POST['o1v']);
        $stmt->bindValue(':out2_volt_min', $_POST['o2v']);
        $stmt->bindValue(':out3_volt_min', $_POST['o3v']);
        $stmt->bindValue(':out1_itp_in', $_POST['o1si']);
        $stmt->bindValue(':out1_itp_out', $_POST['o1so']);
        $stmt->bindValue(':out1_dp_enable', $_POST['o1de']);
        $stmt->bindValue(':out1_dp_in_offset', $_POST['o1do']);
        $stmt->bindValue(':out1_t_in_max', $_POST['o1ti']);
        $stmt->bindValue(':out1_t_out_min', $_POST['o1to']);
        $stmt->bindValue(':out1_h_out_max', $_POST['o1ho']);
        $stmt->execute();

        $cnt = $_POST['c'];
        $tv = $_POST['tw'];
        $dt = array();
        for ($idx = 0; $idx < $cnt; $idx++) {
            $n1  = strpos($base64, $tv[$idx*8+0]);
            $n2  = strpos($base64, $tv[$idx*8+1]);
            $n3  = strpos($base64, $tv[$idx*8+2]);
            $n4  = strpos($base64, $tv[$idx*8+3]);
            $n5  = strpos($base64, $tv[$idx*8+4]);
            $n6  = strpos($base64, $tv[$idx*8+5]);
            $n7  = strpos($base64, $tv[$idx*8+6]);
            $n8  = strpos($base64, $tv[$idx*8+7]);
            // 765432-103210-987654-321098-765432-100987-654321-0210xx
            // iiiiii-iicccc-tttttt-tttttt-tttttt-ttvvvv-vvvvvv-vvvvxx
            // dddddd-ddrrrr-IIIIII-IIIIEE-EEEEEE-EEBBBB-BBBBBB-BUUUxx
            $idx_rx = (($n1 & 0x3F) << 2) | (($n2 & 0x30) >> 4);
            $cnt_rx = (($n2 & 0x0F));
            $ti = (($n3 & 0x3F) << 4) | (($n4 & 0x3C) >> 4);
            $te = (($n4 & 0x03) << 8) | (($n5 & 0x3F) << 2) | (($n6 & 0x30) >> 4);
            $vb = (($n6 & 0x0F) << 7) | (($n7 & 0x3F) << 1) | (($n8 & 0x20) >> 5);
            $vu = (($n8 & 0x1C) >> 2);
            if ($ti >= 512) $ti = $ti - 1024;
            if ($te >= 512) $te = $te - 1024;
            $ts = (($cnt + $_POST['cl']) - ($idx_rx + 1)) * ($_POST['i'] * 60 + $time_corr);
            $dt[] = $now - $ts;

            $stmt = $db->prepare("INSERT INTO adc (dt, idx, cnt_rx, tint, text, vbatt, outputs, delta_ts) VALUES (:dt, :idx, :cnt_rx, :ti, :te, :vb, :vu, :ts)");
            $stmt->bindValue(':dt', end($dt));
            $stmt->bindValue(':idx', $idx_rx);
            $stmt->bindValue(':cnt_rx', $cnt_rx);
            $stmt->bindValue(':ti', $ti/10.);
            $stmt->bindValue(':te', $te/10.);
            $stmt->bindValue(':vb', $vb/100.);
            $stmt->bindValue(':vu', $vu);
            $stmt->bindValue(':ts', $ts);
            $stmt->execute();
        }

        $interval = $_POST['i'];
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
                    $stmt->bindValue(':dt', $dt[$idx]);
                    $stmt->bindValue(':itp', $id);
                    $stmt->bindValue(':temp', $temp/10.);
                    $stmt->bindValue(':hygro', $hygro);
                    $stmt->bindValue(':lowbatt', $lowbatt);
                    $stmt->execute();
                }
            }
        }

        $query = $db->query("SELECT kind, id, mcfg_typ_entries, mcfg_interval, mcfg_itplus_time, mcfg_volt_min, ".
            "out1_volt_min, out2_volt_min, out3_volt_min, out1_itp_in, out1_itp_out, out1_dp_enable, out1_dp_in_offset, ".
            "out1_t_in_max, out1_t_out_min, out1_h_out_max FROM config WHERE done == 0 ORDER BY dt DESC LIMIT 1");
        $row = $query->fetch(PDO::FETCH_ASSOC);
        if ($row == false) {
            $result = "r=ack\n";
        } else {
            if ($row['kind'] == 101) {
                $result = "r=update";
                if (!empty($row['mcfg_typ_entries'])) $result .= sprintf("&ss=%d", $row['mcfg_typ_entries']);
                if (!empty($row['mcfg_interval'])) $result .= sprintf("&si=%d", $row['mcfg_interval']);
                if (!empty($row['mcfg_itplus_time'])) $result .= sprintf("&st=%d", $row['mcfg_itplus_time']);
                if (!empty($row['mcfg_volt_min'])) $result .= sprintf("&sv=%d", $row['mcfg_volt_min']);
                if (!empty($row['out1_volt_min'])) $result .= sprintf("&o1v=%d", $row['out1_volt_min']);
                if (!empty($row['out2_volt_min'])) $result .= sprintf("&o2v=%d", $row['out2_volt_min']);
                if (!empty($row['out3_volt_min'])) $result .= sprintf("&o3v=%d", $row['out3_volt_min']);
                if (!empty($row['out1_itp_in'])) $result .= sprintf("&o1si=%d", $row['out1_itp_in']);
                if (!empty($row['out1_itp_out'])) $result .= sprintf("&o1so=%d", $row['out1_itp_out']);
                if (!empty($row['out1_dp_enable'])) $result .= sprintf("&o1de=%d", $row['out1_dp_enable']);
                if (!empty($row['out1_dp_in_offset'])) $result .= sprintf("&o1do=%d", $row['out1_dp_in_offset']);
                if (!empty($row['out1_t_in_max'])) $result .= sprintf("&o1ti=%d", $row['out1_t_in_max']);
                if (!empty($row['out1_t_out_min'])) $result .= sprintf("&o1to=%d", $row['out1_t_out_min']);
                if (!empty($row['out1_h_out_max'])) $result .= sprintf("&o1ho=%d", $row['out1_h_out_max']);
                $result .= "\n";
            } else if ($row['kind'] == 102) {
                $result = "r=defaults\n";
            } else if ($row['kind'] == 103) {
                $result = "r=reboot\n";
            } else {
                $result = "r=ack\n";
            }
            $stmt = $db->prepare("UPDATE config SET done = 1 WHERE id == :id");
            $stmt->bindValue(':id', $row['id']);
            $stmt->execute();
        }
        print($result);
        fwrite($f, date("d.m.Y H:i:s") . " " . $ip . " <= " . htmlspecialchars($result));
        fclose($f);
    } else {
?>
<html><body>
<h1>Zobrazeni GET/POST pozadavku</h1>
<pre><?php echo implode(PHP_EOL, array_slice(array_reverse(explode(PHP_EOL, file_get_contents('log.txt'))), 0, 11)); ?></pre>
<h1>Dump databaze</h1>
<pre>
<?php
    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM stats ORDER BY dt DESC LIMIT 10");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\tip\t\tcnt\tpwron\tlowbatt\titplus\tsq\tgsm\n";
    echo "---------\t\t--\t--\t\t--\t\t---\t-----\t-------\t------\t--\t---\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM config ORDER BY dt DESC LIMIT 60");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\tkind\tdone\tss\tsi\tst\tsv\to1v\to2v\to3v\to1si\to1so\to1de\to1do\to1ti\to1to\to1ho\n";
    echo "---------\t\t--\t--\t\t----\t----\t--\t--\t--\t--\t---\t---\t---\t----\t----\t----\t----\t----\t----\t----\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM itp ORDER BY dt DESC, itp ASC LIMIT 240");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\titp\ttemp\thygro\tlowbatt\n";
    echo "---------\t\t--\t--\t\t---\t----\t-----\t-------\n";
    while ($row != false) {
        echo implode(array_values($row), "\t") . "\n";
        $row = $query->fetch(PDO::FETCH_ASSOC);
    }
    echo "\n";

    $query = $db->query("SELECT datetime(dt, 'unixepoch', 'localtime') as dtf, * FROM adc ORDER BY dt DESC LIMIT 60");
    $row = $query->fetch(PDO::FETCH_ASSOC);
    echo "timestamp\t\tid\tdt\t\tidx\tcnt_rx\ttint\ttext\tvbatt\toutputs\tdelta_ts\n";
    echo "---------\t\t--\t--\t\t---\t------\t----\t----\t-----\t-------\t--------\n";
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
