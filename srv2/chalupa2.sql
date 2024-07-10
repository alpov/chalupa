ATTACH DATABASE 'chalupa.db' as olddb;

CREATE TABLE stats (
    id integer primary key autoincrement,
    dt numeric not null default (strftime('%s', 'now')),
    ip text,
    cnt integer,
    cnt_pwron integer,
    cnt_lowbatt integer,
    cnt_itplus integer,
    signalq integer,
    cnt_gsm integer
);
INSERT INTO stats SELECT * from olddb.stats;

CREATE TABLE config (
    id integer primary key autoincrement,
    dt numeric not null default (strftime('%s', 'now')),
    kind integer default 1, -- 1=up, 101=update, 102=defaults, 103=reboot
    done integer default 1, -- 0=pending, 1=done
    mcfg_typ_entries integer,
    mcfg_interval integer,
    mcfg_itplus_time integer,
    mcfg_volt_min integer,
    out1_volt_min integer,
    out2_volt_min integer,
    out3_volt_min integer,
    out1_itp_in integer,
    out1_itp_out integer,
    out1_dp_enable integer,
    out1_dp_in_offset integer,
    out1_t_in_max integer,
    out1_t_out_min integer,
    out1_h_out_max integer
);
INSERT INTO config (kind, done, mcfg_typ_entries, mcfg_interval, mcfg_itplus_time, mcfg_volt_min,
    out1_volt_min, out2_volt_min, out3_volt_min, out1_itp_in, out1_itp_out, out1_dp_enable,
    out1_dp_in_offset, out1_t_in_max, out1_t_out_min, out1_h_out_max) VALUES (101, 0,
    5, 3600, 20, 1050, 1200, 1100, 2000, 31, 57, 1, 0, 230, -50, 95);

CREATE TABLE itp (
    id integer primary key autoincrement,
    dt numeric not null,
    itp integer,
    temp numeric,
    hygro numeric,
    lowbatt numeric
);
INSERT INTO itp SELECT * FROM olddb.itp;

CREATE TABLE adc (
    id integer primary key autoincrement,
    dt numeric not null,
    idx integer,
    cnt_rx integer,
    tint numeric,
    text numeric,
    vbatt numeric,
    outputs integer,
    delta_ts integer
);
INSERT INTO adc SELECT id, dt, idx, cnt_rx, tint, text, vbatt, 0, 0 FROM olddb.adc;

DROP VIEW IF EXISTS itp_loznice;
CREATE VIEW itp_loznice as select * from itp where (itp=31 and dt<strftime("%s", "2024-05-19")) or (itp=44 and dt>strftime("%s", "2024-05-01"))
/* itp31(id,dt,itp,"temp",hygro,lowbatt) */;

DROP VIEW IF EXISTS itp_kuchyn;
CREATE VIEW itp_kuchyn as select * from itp where itp=46
/* itp46(id,dt,itp,"temp",hygro,lowbatt) */;

DROP VIEW IF EXISTS itp_cimerka;
CREATE VIEW itp_cimerka as select * from itp where itp=62
/* itp62(id,dt,itp,"temp",hygro,lowbatt) */;

DROP VIEW IF EXISTS itp_venkovni;
CREATE VIEW itp_venkovni as select * from itp where (itp=57 and dt<strftime("%s", "2024-05-14")) or (itp=60 and dt>strftime("%s", "2024-05-01"))
/* itp57(id,dt,itp,"temp",hygro,lowbatt) */;

DROP VIEW IF EXISTS itp_studna;
CREATE VIEW itp_studna as select id,dt,itp,cast(temp*10 as int) as level,lowbatt from itp where itp=1
/* itp_studna(id,dr,itp,level,lowbatt) */;
